// Common utility functions and RAII helpers.

#include "util.h"

#include <cctype>
#include <cstdlib>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pluto {


std::string
HomeDir()
{
	const char* h = std::getenv("HOME");
	if (h != NULL) return h;
	return "/boot/home";
}

std::string
SettingsDir()
{
	return HomeDir() + "/config/settings/pluto";
}

std::string
EnsureSettingsDir()
{
	std::string dir = SettingsDir();
	::mkdir(dir.c_str(), 0755);
	return dir;
}

std::string
Slugify(const std::string& text)
{
	std::string slug;
	for (char c : text) {
		if (std::isalnum(static_cast<unsigned char>(c)))
			slug += std::tolower(static_cast<unsigned char>(c));
		else if ((c == ' ' || c == '-' || c == '_') && !slug.empty() && slug.back() != '-')
			slug += '-';
	}
	while (!slug.empty() && slug.back() == '-') slug.pop_back();
	if (slug.empty()) slug = "entry";
	return slug;
}

std::string
Trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;

	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;

	return s.substr(start, end - start);
}

PipeGuard::PipeGuard(const char* command, const char* mode)
    : fPipe(popen(command, mode))
{
}

PipeGuard::~PipeGuard()
{
	close();
}

int
PipeGuard::close()
{
	if (fPipe == NULL) return -1;
	int rc = pclose(fPipe);
	fPipe = NULL;
	return rc;
}


} // namespace pluto

// Execute a hook command safely using fork/exec (no shell injection).
// Sets the given environment variable in the child process before exec.
// Returns the exit code, or -1 on fork failure.
int RunHookSafe(const char* cmd, const char* env_name, const std::string& env_value) {
	pid_t pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		// Child: set env var safely, then exec hook via /bin/sh.
		::setenv(env_name, env_value.c_str(), 1);
		::execl("/bin/sh", "sh", "-c", cmd, nullptr);
		::_exit(127);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Execute multiple hook commands safely.
void RunHooksSafe(const std::vector<std::string>& hooks, const char* phase, const std::string& env_value) {
	for (const auto& cmd : hooks) {
		if (cmd.empty()) continue;
		std::fprintf(stderr, "[hook %s] %s\n", phase, cmd.c_str());
		int rc = RunHookSafe(cmd.c_str(), "PLUTO_PROMPT", env_value);
		if (rc != 0)
			std::fprintf(stderr, "[hook %s] exit %d\n", phase, rc);
	}
}

} // namespace pluto
