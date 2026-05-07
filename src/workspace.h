#pragma once

#include <string>
#include <vector>

namespace pluto {

// Initialize workspace to cwd. Call once at startup.
void WorkspaceInit();

// Get the current workspace root (cwd at startup).
std::string WorkspaceRoot();

// Change the workspace root at runtime (e.g. from Tracker add-on).
// Also chdir()s to the new root so tools resolve paths correctly.
void WorkspaceSetRoot(const std::string& path);

// Check if a path is within the workspace or a trusted directory.
// Returns true if allowed, false if needs approval.
bool WorkspaceAllows(const std::string& path);

// Trust management — persisted in ~/config/settings/pluto/trusted_workspaces
bool IsWorkspaceTrusted(const std::string& dir);
void TrustWorkspace(const std::string& dir);
void UntrustWorkspace(const std::string& dir);
std::vector<std::string> ListTrustedWorkspaces();

// Record an access for audit logging
void LogAccess(const std::string& path, const std::string& action, bool allowed);

// Session-only trust (not persisted). Used for "once" mode.
void TrustSession();
bool IsSessionTrusted();

// Override the trusted_workspaces file path (for testing).
void SetTrustedFileOverride(const std::string& path);

// Override the access.log file path (for testing).
void SetLogFileOverride(const std::string& path);

// ---- /init: project detection and pluto.md generation ----

struct LangStats {
	std::string language;
	int files = 0;
	int lines = 0;
};

struct ProjectInfo {
	std::string git_remote;
	std::string main_language;
	std::vector<std::string> interesting_files;  // build files found
	std::vector<std::string> subdirs;             // top-level directories
	std::vector<LangStats>   lang_stats;          // per-language counts
	std::string readme_excerpt;                   // first ~30 lines of README
	std::string dependencies_excerpt;             // parsed from package.json etc.
	std::string source_samples;                   // excerpts of key source files
	int total_files = 0;
	int total_lines = 0;
};

// Auto-detect project info (git remote, language, build system).
ProjectInfo DetectProject(const std::string& root);

// Generate a pluto.md template from detected info.
std::string GeneratePlutoMd(const std::string& root, const ProjectInfo& info);

// Create pluto.md in `root`. Returns the path on success, empty on error.
// Sets `err` on failure. If `force` is false and the file exists, sets
// `err` and returns empty.
std::string InitProjectMemory(const std::string& root, bool force,
                                std::string& err, ProjectInfo& info);

}  // namespace pluto
