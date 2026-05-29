#include "workspace.h"
#include "filelock.h"
#include "util.h"

#include <Path.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace pluto {

namespace {

std::string g_WorkspaceRoot;
bool g_session_trusted = false;

// Shell-safe quoting: wraps in single quotes, escapes embedded quotes.
std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

// Empty means "use default". Non-empty means override (for tests).
std::string g_trusted_file_override;
std::string g_log_file_override;

std::string home_dir() {
	return HomeDir();
}

std::string settings_dir() {
	return SettingsDir();
}

void ensure_dir(const std::string& path) {
	::mkdir(path.c_str(), 0755);
}

std::string trusted_file_path() {
	if (!g_trusted_file_override.empty()) return g_trusted_file_override;
	return settings_dir() + "/trusted_workspaces";
}

std::string log_file_path() {
	if (!g_log_file_override.empty()) return g_log_file_override;
	return settings_dir() + "/access.log";
}

std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
		--b;
	return s.substr(a, b - a);
}

// Normalize a path string by resolving . and .. components.
// Does NOT touch the filesystem — purely lexical.
std::string normalize_path(const std::string& path) {
	std::vector<std::string> parts;
	bool absolute = !path.empty() && path[0] == '/';
	size_t i = 0;
	while (i < path.size()) {
		size_t j = path.find('/', i);
		if (j == std::string::npos) j = path.size();
		std::string comp = path.substr(i, j - i);
		i = j + 1;
		if (comp.empty() || comp == ".") continue;
		if (comp == "..") {
			if (!parts.empty() && parts.back() != "..")
				parts.pop_back();
			else if (!absolute)
				parts.push_back("..");
		} else {
			parts.push_back(comp);
		}
	}
	std::string result;
	if (absolute) result = "/";
	for (size_t k = 0; k < parts.size(); ++k) {
		if (k > 0) result += "/";
		result += parts[k];
	}
	if (result.empty()) result = ".";
	return result;
}

// Canonicalize a path: resolve via BPath if possible. If the full
// path doesn't exist (e.g. not-yet-created file), resolve the longest
// existing parent prefix and append the remainder. The result is always
// normalized (no . or .. components) to prevent path traversal.
std::string canonicalize(const std::string& raw) {
	if (raw.empty()) return raw;

	// Try BPath normalization first (resolves symlinks for existing paths).
	BPath bpath;
	if (bpath.SetTo(raw.c_str(), nullptr, true) == B_OK)
		return bpath.Path();

	// Walk up until we find an existing parent.
	std::string dir = raw;
	std::string tail;
	while (!dir.empty() && dir != "/") {
		size_t slash = dir.rfind('/');
		if (slash == std::string::npos) break;
		std::string name = dir.substr(slash);
		dir = dir.substr(0, slash);
		if (dir.empty()) dir = "/";
		tail = name + tail;
		if (bpath.SetTo(dir.c_str(), nullptr, true) == B_OK) {
			// Normalize to eliminate any .. in the unresolved tail.
			std::string result = normalize_path(std::string(bpath.Path()) + tail);
			while (result.size() > 1 && result.back() == '/')
				result.pop_back();
			return result;
		}
	}

	// Fall back: normalize to eliminate .. traversals.
	std::string s = normalize_path(raw);
	while (s.size() > 1 && s.back() == '/') s.pop_back();
	return s;
}

bool starts_with(const std::string& str, const std::string& prefix) {
	if (prefix.size() > str.size()) return false;
	if (str.compare(0, prefix.size(), prefix) != 0) return false;
	// Must be exact match or followed by '/'
	return str.size() == prefix.size() || str[prefix.size()] == '/';
}

}  // namespace

void WorkspaceInit() {
	char buf[PATH_MAX];
	if (::getcwd(buf, sizeof(buf)))
		g_WorkspaceRoot = std::string(buf);
}

std::string WorkspaceRoot() {
	return g_WorkspaceRoot;
}

void WorkspaceSetRoot(const std::string& path) {
	std::string cp = canonicalize(path);
	if (cp.empty()) cp = path;
	g_WorkspaceRoot = cp;
	::chdir(cp.c_str());
}

bool WorkspaceAllows(const std::string& path) {
	// If workspace not initialized, allow everything (backwards compat).
	if (g_WorkspaceRoot.empty()) return true;

	// Fast path: check raw path first before expensive canonicalization.
	// This avoids BPath/realpath for the common case where the path is
	// already under the workspace root. Skip if the path contains ".."
	// which could escape the workspace despite matching the prefix.
	if (starts_with(path, g_WorkspaceRoot)
	    && path.find("..") == std::string::npos) return true;

	// Session trust covers everything — no need to canonicalize.
	if (g_session_trusted) return true;

	// Canonicalize for symlink resolution and .. elimination.
	std::string cp = canonicalize(path);
	if (cp.empty()) cp = path;

	if (starts_with(cp, g_WorkspaceRoot)) return true;

	// Check trusted workspaces.
	auto trusted = ListTrustedWorkspaces();
	for (const auto& tw : trusted) {
		if (starts_with(cp, tw)) return true;
	}

	return false;
}

bool IsWorkspaceTrusted(const std::string& dir) {
	std::string cd = canonicalize(dir);
	auto trusted = ListTrustedWorkspaces();
	for (const auto& tw : trusted) {
		if (tw == cd) return true;
	}
	return false;
}

void TrustWorkspace(const std::string& dir) {
	std::string cd = canonicalize(dir);

	ensure_dir(settings_dir());
	ScopedFileLock lock(trusted_file_path() + ".lock");
	if (!lock.locked()) return;

	// Re-check under lock to avoid duplicates.
	auto trusted = ListTrustedWorkspaces();
	for (const auto& tw : trusted) {
		if (tw == cd) return;
	}

	std::ofstream f(trusted_file_path(), std::ios::app);
	if (f) f << cd << "\n";
}

void UntrustWorkspace(const std::string& dir) {
	std::string cd = canonicalize(dir);

	ScopedFileLock lock(trusted_file_path() + ".lock");
	if (!lock.locked()) return;

	auto trusted = ListTrustedWorkspaces();
	std::ofstream f(trusted_file_path(), std::ios::trunc);
	if (!f) return;
	for (const auto& tw : trusted) {
		if (tw != cd) f << tw << "\n";
	}
}

std::vector<std::string> ListTrustedWorkspaces() {
	std::vector<std::string> out;
	std::ifstream f(trusted_file_path());
	if (!f) return out;
	std::string line;
	while (std::getline(f, line)) {
		std::string s = trim(line);
		if (!s.empty()) out.push_back(s);
	}
	return out;
}

void LogAccess(const std::string& path, const std::string& action, bool allowed) {
	ensure_dir(settings_dir());
	ScopedFileLock lock(log_file_path() + ".lock", 500);
	if (!lock.locked()) return;

	std::ofstream f(log_file_path(), std::ios::app);
	if (!f) return;

	std::time_t now = std::time(nullptr);
	char ts[32];
	std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S",
		std::localtime(&now));

	f << ts << "  " << (allowed ? "ALLOW" : "DENY") << "  "
	  << action << "  " << path << "\n";
}

void TrustSession() {
	g_session_trusted = true;
}

bool IsSessionTrusted() {
	return g_session_trusted;
}

void SetTrustedFileOverride(const std::string& path) {
	g_trusted_file_override = path;
}

void SetLogFileOverride(const std::string& path) {
	g_log_file_override = path;
}

// ---- /init: project detection and pluto.md generation ----

// Count lines in a file (up to a limit for speed).
static int count_lines(const std::string& path) {
	std::ifstream f(path);
	if (!f) return 0;
	int n = 0;
	std::string line;
	while (std::getline(f, line)) ++n;
	return n;
}

// Read the first N lines of a file.
static std::string read_head(const std::string& path, int max_lines) {
	std::ifstream f(path);
	if (!f) return {};
	std::string result, line;
	int n = 0;
	while (n < max_lines && std::getline(f, line)) {
		result += line + "\n";
		++n;
	}
	return result;
}

ProjectInfo DetectProject(const std::string& root) {
	ProjectInfo info;

	// Detect git remote.
	{
		std::string cmd = "cd " + shq(root) + " && git remote get-url origin 2>/dev/null";
		FILE* p = popen(cmd.c_str(), "r");
		if (p) {
			char buf[512];
			if (std::fgets(buf, sizeof(buf), p)) {
				info.git_remote = buf;
				while (!info.git_remote.empty()
				    && (info.git_remote.back() == '\n'
				     || info.git_remote.back() == '\r'))
					info.git_remote.pop_back();
			}
			pclose(p);
		}
	}

	// Detect top-level subdirectories.
	{
		std::string cmd = "cd '" + root +
			"' && find . -maxdepth 1 -type d 2>/dev/null | sort";
		FILE* p = popen(cmd.c_str(), "r");
		if (p) {
			char buf[1024];
			while (std::fgets(buf, sizeof(buf), p)) {
				std::string d = buf;
				while (!d.empty() && (d.back() == '\n' || d.back() == '\r'))
					d.pop_back();
				// Remove "./" prefix, skip "." itself and hidden dirs.
				if (d.size() > 2 && d[0] == '.' && d[1] == '/')
					d = d.substr(2);
				if (d.empty() || d == "." || d[0] == '.') continue;
				info.subdirs.push_back(d);
			}
			pclose(p);
		}
	}

	// Collect all files and count per-language stats.
	struct ExtLang { const char* ext; const char* lang; };
	ExtLang ext_map[] = {
		{".cpp", "C++"}, {".cc", "C++"}, {".cxx", "C++"},
		{".c", "C"}, {".h", "C/C++ Header"},
		{".hpp", "C++ Header"}, {".hxx", "C++ Header"},
		{".py", "Python"}, {".pyx", "Python"},
		{".rs", "Rust"}, {".go", "Go"},
		{".js", "JavaScript"}, {".jsx", "JavaScript"},
		{".ts", "TypeScript"}, {".tsx", "TypeScript"},
		{".java", "Java"}, {".kt", "Kotlin"},
		{".rb", "Ruby"}, {".lua", "Lua"},
		{".sh", "Shell"}, {".bash", "Shell"},
		{".html", "HTML"}, {".css", "CSS"}, {".scss", "CSS"},
		{".json", "JSON"}, {".yaml", "YAML"}, {".yml", "YAML"},
		{".xml", "XML"}, {".toml", "TOML"},
		{".md", "Markdown"}, {".txt", "Text"},
		{".sql", "SQL"}, {".swift", "Swift"},
		{".m", "Objective-C"}, {".mm", "Objective-C++"},
		{".zig", "Zig"}, {".nim", "Nim"}, {".d", "D"},
		{nullptr, nullptr}
	};

	// Map language name → accumulated stats.
	struct LangAcc { int files; int lines; };
	std::vector<std::pair<std::string, LangAcc>> lang_map;
	auto find_lang = [&](const std::string& name) -> LangAcc& {
		for (auto& p : lang_map)
			if (p.first == name) return p.second;
		lang_map.push_back({name, {0, 0}});
		return lang_map.back().second;
	};

	{
		std::string cmd = "cd '" + root +
			"' && git ls-files 2>/dev/null || find . -maxdepth 4 -type f "
			"-not -path '*/.git/*' -not -path '*/.pluto/*' "
			"-not -path '*/node_modules/*' -not -path '*/target/*' "
			"-not -path '*/__pycache__/*' 2>/dev/null";
		FILE* p = popen(cmd.c_str(), "r");
		if (p) {
			char buf[1024];
			while (std::fgets(buf, sizeof(buf), p)) {
				std::string f = buf;
				while (!f.empty() && (f.back() == '\n' || f.back() == '\r'))
					f.pop_back();
				if (f.empty()) continue;

				++info.total_files;

				// Match extension.
				for (ExtLang* e = ext_map; e->ext; ++e) {
					size_t elen = strlen(e->ext);
					if (f.size() > elen
					    && f.compare(f.size() - elen, elen, e->ext) == 0) {
						LangAcc& acc = find_lang(e->lang);
						acc.files++;
						std::string full = root + "/" + f;
						int lc = count_lines(full);
						acc.lines += lc;
						info.total_lines += lc;
						break;
					}
				}
			}
			pclose(p);
		}
	}

	// Sort languages by line count descending.
	std::sort(lang_map.begin(), lang_map.end(),
		[](const auto& a, const auto& b) {
			return a.second.lines > b.second.lines;
		});
	for (const auto& lm : lang_map) {
		if (lm.second.files > 0)
			info.lang_stats.push_back({lm.first, lm.second.files, lm.second.lines});
	}

	// Main language = most lines (excluding headers, config, docs).
	for (const auto& ls : info.lang_stats) {
		if (ls.language.find("Header") != std::string::npos) continue;
		if (ls.language == "JSON" || ls.language == "YAML"
		    || ls.language == "TOML" || ls.language == "XML"
		    || ls.language == "Markdown" || ls.language == "Text") continue;
		info.main_language = ls.language;
		break;
	}

	// Note interesting project/build files.
	const char* markers[] = {
		"Makefile", "CMakeLists.txt", "Jamfile", "meson.build",
		"package.json", "Cargo.toml", "go.mod", "pyproject.toml",
		"setup.py", "build.gradle", "pom.xml", "Dockerfile",
		".gitignore", "LICENSE", "README.md", "README",
		nullptr
	};
	for (const char** m = markers; *m; ++m) {
		std::string path = root + "/" + *m;
		struct stat st2;
		if (::stat(path.c_str(), &st2) == 0 && S_ISREG(st2.st_mode))
			info.interesting_files.push_back(*m);
	}

	// Read README excerpt.
	for (const char* rn : {"README.md", "README", "README.txt", "readme.md"}) {
		std::string rpath = root + "/" + rn;
		struct stat st3;
		if (::stat(rpath.c_str(), &st3) == 0) {
			info.readme_excerpt = read_head(rpath, 30);
			break;
		}
	}

	// Read dependency/config file excerpts.
	for (const char* dep : {"package.json", "Cargo.toml", "go.mod",
	                         "pyproject.toml", "requirements.txt",
	                         "Makefile", "CMakeLists.txt", "Jamfile"}) {
		std::string dpath = root + "/" + dep;
		struct stat st4;
		if (::stat(dpath.c_str(), &st4) == 0) {
			std::string content = read_head(dpath, 50);
			if (!content.empty()) {
				info.dependencies_excerpt += "── " + std::string(dep) + " ──\n";
				info.dependencies_excerpt += content + "\n";
			}
		}
	}

	// Read key source files (entry points, main files, configs).
	// Collect up to ~3000 lines total from the most important files.
	{
		// Priority files to sample.
		std::vector<std::string> candidates;

		// 1. Explicit entry points.
		for (const char* ep : {"main.cpp", "main.c", "main.py", "main.go",
		                        "main.rs", "main.js", "main.ts",
		                        "src/main.cpp", "src/main.c", "src/main.py",
		                        "src/main.rs", "src/main.go",
		                        "src/lib.rs", "src/index.js", "src/index.ts",
		                        "src/app.py", "app.py", "server.py",
		                        "src/App.tsx", "src/App.jsx"}) {
			std::string p = root + "/" + ep;
			struct stat st5;
			if (::stat(p.c_str(), &st5) == 0 && S_ISREG(st5.st_mode))
				candidates.push_back(ep);
		}

		// 2. All source files from git or find, sorted by size (largest first
		//    = likely most important), pick top files not already in candidates.
		{
			struct FileSz { std::string path; long size; };
			std::vector<FileSz> all_src;
			std::string cmd = "cd '" + root +
				"' && git ls-files 2>/dev/null || find . -maxdepth 3 -type f "
				"-not -path '*/.git/*' -not -path '*/node_modules/*' 2>/dev/null";
			FILE* p = popen(cmd.c_str(), "r");
			if (p) {
				char buf[1024];
				while (std::fgets(buf, sizeof(buf), p)) {
					std::string f = buf;
					while (!f.empty() && (f.back() == '\n' || f.back() == '\r'))
						f.pop_back();
					if (f.empty()) continue;
					// Only source code files.
					bool is_src = false;
					for (const char* ext : {".cpp", ".c", ".h", ".hpp",
					     ".py", ".rs", ".go", ".js", ".ts", ".tsx", ".jsx",
					     ".java", ".rb", ".lua", ".zig", ".swift"}) {
						size_t elen = strlen(ext);
						if (f.size() > elen
						    && f.compare(f.size() - elen, elen, ext) == 0)
							{ is_src = true; break; }
					}
					if (!is_src) continue;
					struct stat st6;
					std::string full = root + "/" + f;
					if (::stat(full.c_str(), &st6) == 0)
						all_src.push_back({f, st6.st_size});
				}
				pclose(p);
			}
			// Sort by size descending.
			std::sort(all_src.begin(), all_src.end(),
				[](const FileSz& a, const FileSz& b) {
					return a.size > b.size;
				});
			for (const auto& fs : all_src) {
				bool dup = false;
				for (const auto& c : candidates)
					if (c == fs.path) { dup = true; break; }
				if (!dup) candidates.push_back(fs.path);
				if (candidates.size() >= 12) break;
			}
		}

		// Read excerpts (first 80 lines each, up to ~3000 lines total).
		int total_sample_lines = 0;
		for (const auto& c : candidates) {
			if (total_sample_lines > 3000) break;
			std::string full = root + "/" + c;
			std::string content = read_head(full, 80);
			if (content.empty()) continue;
			info.source_samples += "── " + c + " ──\n";
			info.source_samples += content + "\n";
			// Count lines added.
			for (char ch : content)
				if (ch == '\n') ++total_sample_lines;
		}
	}

	return info;
}

std::string GeneratePlutoMd(const std::string& root, const ProjectInfo& info) {
	std::ostringstream os;
	os << "# Project Memory\n\n";

	// Project name from directory basename.
	std::string name = root;
	size_t slash = name.rfind('/');
	if (slash != std::string::npos) name = name.substr(slash + 1);
	os << "Project: **" << name << "**\n\n";

	if (!info.git_remote.empty())
		os << "Repository: " << info.git_remote << "\n\n";
	if (!info.main_language.empty())
		os << "Primary language: " << info.main_language << "\n\n";

	// Project files found.
	if (!info.interesting_files.empty()) {
		os << "Project files: ";
		for (size_t i = 0; i < info.interesting_files.size(); ++i) {
			if (i > 0) os << ", ";
			os << info.interesting_files[i];
		}
		os << "\n\n";
	}

	// Statistics.
	if (info.total_files > 0) {
		os << "## Statistics\n\n";
		os << "Total: " << info.total_files << " files, "
		   << info.total_lines << " lines\n\n";
		if (!info.lang_stats.empty()) {
			os << "| Language | Files | Lines |\n";
			os << "|----------|------:|------:|\n";
			for (const auto& ls : info.lang_stats) {
				if (ls.files == 0) continue;
				os << "| " << ls.language << " | " << ls.files
				   << " | " << ls.lines << " |\n";
			}
			os << "\n";
		}
	}

	// Directory structure.
	if (!info.subdirs.empty()) {
		os << "## Directory structure\n\n";
		os << "```\n" << name << "/\n";
		for (const auto& d : info.subdirs)
			os << "  " << d << "/\n";
		os << "```\n\n";
	}

	// README excerpt.
	if (!info.readme_excerpt.empty()) {
		os << "## README (excerpt)\n\n";
		os << info.readme_excerpt << "\n";
	}

	// Dependencies.
	if (!info.dependencies_excerpt.empty()) {
		os << "## Dependencies\n\n";
		os << "```\n" << info.dependencies_excerpt << "```\n\n";
	}

	os << "## Instructions\n\n";
	os << "<!-- Add project-specific instructions for the assistant here.\n";
	os << "     This file is injected into the system prompt as <project_memory>.\n";
	os << "     Examples:\n";
	os << "     - Coding conventions and style guidelines\n";
	os << "     - Build and test commands\n";
	os << "     - Architecture notes\n";
	os << "     - Files or patterns to avoid modifying\n";
	os << "-->\n";

	return os.str();
}

std::string InitProjectMemory(const std::string& root, bool force,
                                std::string& err, ProjectInfo& info) {
	if (root.empty()) {
		err = "cannot determine workspace root";
		return {};
	}

	std::string md_path = root + "/pluto.md";

	struct stat st;
	if (!force && ::stat(md_path.c_str(), &st) == 0) {
		err = "pluto.md already exists";
		return {};
	}

	info = DetectProject(root);
	std::string content = GeneratePlutoMd(root, info);

	std::ofstream f(md_path, std::ios::trunc);
	if (!f) {
		err = "failed to write " + md_path;
		return {};
	}
	f << content;
	f.close();

	return md_path;
}

}  // namespace pluto
