// Common utility functions and RAII helpers.
// Shared across the pluto codebase to avoid duplication.

#include "util.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pluto {

namespace {

std::string trim_impl(const std::string& s) {
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;

	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;

	return s.substr(start, end - start);
}

} // namespace

std::string HomeDir() {
	const char* h = std::getenv("HOME");
	if (h != nullptr) return h;
	return "/boot/home";
}

std::string SettingsDir() {
	return HomeDir() + "/config/settings/pluto";
}

std::string EnsureSettingsDir() {
	std::string dir = SettingsDir();
	::mkdir(dir.c_str(), 0755);
	return dir;
}

std::string Slugify(const std::string& text) {
	std::string slug;
	slug.reserve(text.size());
	for (char c : text) {
		unsigned char u = static_cast<unsigned char>(c);
		if (std::isalnum(u)) {
			slug += static_cast<char>(std::tolower(u));
		} else if ((c == ' ' || c == '-' || c == '_') && !slug.empty() && slug.back() != '-') {
			slug += '-';
		}
	}
	while (!slug.empty() && slug.back() == '-') slug.pop_back();
	if (slug.empty()) slug = "entry";
	return slug;
}

std::string Trim(const std::string& s) {
	return trim_impl(s);
}

PipeGuard::PipeGuard(const char* command, const char* mode)
    : fPipe(nullptr), fPid(-1) {
    // Only support "r" mode (reading from command stdout)
    if (strcmp(mode, "r") != 0) return;

    // Parse command into argv
    std::vector<std::string> argv = split_command(command);
    if (argv.empty()) return;

    int pipefd[2];
    if (pipe(pipefd) != 0) return;

    pid_t pid = fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        // Child process
        ::close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[1]);
		auto args = make_argv(argv);
		execvp(args[0], args.data());
        _exit(127);
    }

    // Parent process
    fPid = pid;
    ::close(pipefd[1]);
    fPipe = fdopen(pipefd[0], "r");
}

PipeGuard::~PipeGuard() {
	this->Close();
}

int PipeGuard::Close() {
    if (fPipe == nullptr && fPid <= 0) return -1;
    int rc = -1;
    if (fPipe) {
        fclose(fPipe);
        fPipe = nullptr;
    }
    if (fPid > 0) {
        int status = 0;
        waitpid(fPid, &status, 0);
        rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        fPid = -1;
    }
    return rc;
}

// Split command string into argv array (simple shell-like parsing, no shell execution)
std::vector<std::string> split_command(const std::string& cmd) {
	std::vector<std::string> args;
	std::string current;
	bool in_single = false;
	bool in_double = false;
	bool escaped = false;

	for (size_t i = 0; i < cmd.size(); ++i) {
		char c = cmd[i];
		if (escaped) {
			current += c;
			escaped = false;
		} else if (c == '\\') {
			escaped = true;
		} else if (c == '\'' && !in_double) {
			in_single = !in_single;
		} else if (c == '"' && !in_single) {
			in_double = !in_double;
		} else if ((c == ' ' || c == '\t') && !in_single && !in_double) {
			if (!current.empty()) {
				args.push_back(current);
				current.clear();
			}
		} else {
			current += c;
		}
	}
	if (!current.empty()) args.push_back(current);
	return args;
}

// Execute command via execvp with argv array (no shell).
// Returns exit code, or -1 on error.
static int execvp_with_env(const std::vector<std::string>& argv, const char* env_name, const std::string& env_value) {
	if (argv.empty()) return -1;
	pid_t pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		if (env_name && !env_value.empty()) {
			::setenv(env_name, env_value.c_str(), 1);
		}
		auto args = make_argv(argv);
		execvp(args[0], args.data());
		::_exit(127);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int RunHookSafe(const char* cmd, const char* env_name, const std::string& env_value) {
	auto argv = split_command(cmd);
	return execvp_with_env(argv, env_name, env_value);
}

void RunHooksSafe(const std::vector<std::string>& hooks, const char* phase, const std::string& env_value) {
	for (const auto& cmd : hooks) {
		if (cmd.empty()) continue;
		std::fprintf(stderr, "[hook %s] %s\n", phase, cmd.c_str());
		int rc = RunHookSafe(cmd.c_str(), "PLUTO_PROMPT", env_value);
		if (rc != 0)
			std::fprintf(stderr, "[hook %s] exit %d\n", phase, rc);
	}
}

int RunCommandCapture(const char* cmd, std::string& output) {
	auto argv = split_command(cmd);
	if (argv.empty()) return -1;

	int pipefd[2];
	if (pipe(pipefd) != 0) return -1;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		auto args = make_argv(argv);
		execvp(args[0], args.data());
		_exit(127);
	}

	close(pipefd[1]);
	char buf[4096];
	ssize_t n;
	while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
		output.append(buf, n);
	}
	close(pipefd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Execute command via fork/execvp with argv array (no shell).
// Returns exit code, or -1 on error. Captures stdout+stderr into output.
int execvp_capture(const std::vector<std::string>& argv, std::string& output) {
	if (argv.empty()) return -1;

	int pipefd[2];
	if (pipe(pipefd) != 0) return -1;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		auto args = make_argv(argv);
		execvp(args[0], args.data());
		_exit(127);
	}

	close(pipefd[1]);
	char buf[4096];
	ssize_t n;
	while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
		output.append(buf, n);
	}
	close(pipefd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Shell-quote a string: wraps in single quotes, escaping embedded quotes.
// Use when constructing shell commands (for system(), popen, sh -c).
std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

// Convert vector<string> to argv array for execvp/posix_spawn (no shell).
// Returns vector<char*> with nullptr terminator. Strings are copied to ensure
// valid pointers for the duration of the exec call.
std::vector<char*> make_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& s : args) {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

// Execute command in a specific working directory via fork/execvp.
// Returns exit code, fills output. No shell.
int execvp_capture_cwd(const std::string& cwd, const std::vector<std::string>& argv, std::string& output) {
	if (argv.empty()) return -1;

	int pipefd[2];
	if (pipe(pipefd) != 0) return -1;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
			_exit(127);
		}
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		auto args = make_argv(argv);
		execvp(args[0], args.data());
		_exit(127);
	}

	close(pipefd[1]);
	char buf[4096];
	ssize_t n;
	while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
		output.append(buf, n);
	}
	close(pipefd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace pluto