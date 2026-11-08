// Common utility functions and RAII helpers.
// Shared across the pluto codebase to avoid duplication.

#include "util.h"

#include <cctype>
#include <cstdlib>
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
    : fPipe(popen(command, mode)) {}

PipeGuard::~PipeGuard() {
	close();
}

int PipeGuard::close() {
	if (fPipe == nullptr) return -1;
	int rc = pclose(fPipe);
	fPipe = nullptr;
	return rc;
}

int RunHookSafe(const char* cmd, const char* env_name, const std::string& env_value) {
	pid_t pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		::setenv(env_name, env_value.c_str(), 1);
		::execl("/bin/sh", "sh", "-c", cmd, nullptr);
		::_exit(127);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
		execl("/bin/sh", "sh", "-c", cmd, nullptr);
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