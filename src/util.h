// Common utility functions and RAII helpers.
// Shared across the pluto codebase to avoid duplication.

#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <sys/types.h>

namespace pluto {


// Return the user's home directory path. Never empty on Haiku
// (falls back to "/boot/home" if $HOME is unset).
std::string HomeDir();


// Return the pluto settings directory: ~/config/settings/pluto
// Does NOT create it — use EnsureSettingsDir() for that.
std::string SettingsDir();


// Ensure the pluto settings directory exists (creates if needed).
// Returns the path.
std::string EnsureSettingsDir();


// Sanitize a string for use as a filename. Lowercases, replaces
// non-alnum with hyphens, collapses runs, trims trailing hyphens.
// Returns "entry" if result would be empty.
std::string Slugify(const std::string& text);


// Trim leading and trailing whitespace from a string.
std::string Trim(const std::string& s);


// RAII wrapper for pipe obtained from fork/execvp (no shell injection).
// Automatically waits for child process on destruction.
//
// Usage:
//   PipeGuard pipe("ls -la", "r");
//   if (!pipe) { /* error */ }
//   while (fgets(buf, sizeof(buf), pipe.get())) { ... }
//   int rc = pipe.close();  // or let destructor handle it
//
class PipeGuard {
public:
	explicit PipeGuard(const char* command, const char* mode = "r");
	~PipeGuard();

	// Non-copyable, non-movable.
	PipeGuard(const PipeGuard&) = delete;
	PipeGuard& operator=(const PipeGuard&) = delete;

	// Access the underlying FILE*.
	FILE* get() const { return fPipe; }

	// Boolean conversion: true if pipe is open.
	explicit operator bool() const { return fPipe != NULL; }

	// Explicitly close and return the exit status.
	// Returns -1 if already closed or was never opened.
	int Close();

private:
	FILE* fPipe;
	pid_t fPid;
};

// Shell-quote a string: wraps in single quotes, escaping embedded quotes.
// Use this when constructing shell commands (e.g., for system(), popen, or
// passing to `sh -c`). Never interpolate unquoted user data into commands.
std::string shq(const std::string& s);

// Execute a hook command safely using fork/exec (no shell injection).
// Sets the given environment variable in the child process before exec.
// Returns the exit code, or -1 on fork failure.
int RunHookSafe(const char* cmd, const char* env_name, const std::string& env_value);

// Execute multiple hook commands safely.
void RunHooksSafe(const std::vector<std::string>& hooks, const char* phase, const std::string& env_value);

// Run a command via fork/exec and capture stdout/stderr.
// Returns exit code, fills output with combined stdout+stderr.
// Returns -1 on fork/exec failure.
int RunCommandCapture(const char* cmd, std::string& output);

// Split command string into argv array (simple shell-like parsing, no shell execution).
std::vector<std::string> split_command(const std::string& cmd);

// Execute command via fork/execvp with argv array (no shell).
// Returns exit code, or -1 on error. Captures stdout+stderr into output.
int execvp_capture(const std::vector<std::string>& argv, std::string& output);

// Execute command in a specific working directory via fork/execvp.
// Returns exit code, fills output. No shell.
int execvp_capture_cwd(const std::string& cwd, const std::vector<std::string>& argv, std::string& output);

}  // namespace pluto
