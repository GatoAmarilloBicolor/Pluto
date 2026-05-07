// Common utility functions and RAII helpers.
// Shared across the pluto codebase to avoid duplication.

#pragma once

#include <cstdio>
#include <string>

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


// RAII wrapper for FILE* obtained from popen().
// Automatically calls pclose() on destruction.
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

	// Explicitly close and return the exit status (like pclose).
	// Returns -1 if already closed or was never opened.
	int close();

private:
	FILE* fPipe;
};


}  // namespace pluto
