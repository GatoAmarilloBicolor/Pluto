#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pluto {

struct Routine {
	std::string name;
	std::string path;           // absolute path to .md file
	std::string schedule;       // "manual", "on_boot", "on_file_change <dir>", cron-ish
	std::string model;          // model override (empty = default)
	std::vector<std::string> tools;  // tool allowlist (empty = all)
	bool notify = false;        // BNotification on completion
	bool enabled = true;
	std::string body;           // prompt template (after frontmatter)
};

// Directory for user routines.
std::string RoutinesDir();

// Directory for routine execution logs.
std::string RoutineLogsDir();

// Parse a routine .md file. Returns false on error.
bool ParseRoutine(const std::string& path, Routine& out);

// List all routines found in routines_dir().
std::vector<Routine> ListRoutines();

// Expand template variables in the routine body:
//   {{date}}           -> current date/time
//   {{clipboard}}      -> system clipboard text
//   {{changed_files}}  -> git modified files (auto-detected if empty)
std::string ExpandTemplate(const std::string& body,
                            const std::string& changed_files = "");

// Detect modified files using git status in the current working directory.
// Returns a newline-separated list of file paths, or "(no git changes)" if
// the working tree is clean or not a git repo.
std::string DetectChangedFiles();

// Build a complete prompt for running a routine in a chat context.
// Wraps the expanded body with tool restrictions and routine metadata
// so the model respects the routine's constraints.
std::string BuildRoutinePrompt(const Routine& r);

// Import a routine .md file: validates frontmatter syntax and copies to
// routines_dir().  Returns false (with error message) on invalid syntax
// or I/O failure.
bool ImportRoutine(const std::string& source_path, std::string& error);

// Enable or disable a routine by rewriting its frontmatter.
// When a routine with an automatic schedule (on_boot, on_file_change,
// or a periodic interval) is enabled, a launch_daemon job is installed;
// when disabled, it is removed.
bool SetRoutineEnabled(const std::string& path, bool enabled, std::string& error);

// Install a launch_daemon job for a routine with an automatic trigger.
// Returns false (with error message) if the routine is manual.
bool InstallRoutineJob(const Routine& r, std::string& error);

// Remove the launch_daemon job for a routine.
bool UninstallRoutineJob(const std::string& name, std::string& error);

// Append output to the routine's log file.
void LogRoutineRun(const std::string& name, const std::string& output);

// Read last N lines from a routine's log.
std::string ReadRoutineLog(const std::string& name, int lines = 50);

// Run the `pluto routine` subcommand.
int RunRoutineCli(int argc, char** argv);

}  // namespace pluto
