#include "Builtin.h"
#include "Tool.h"
#include "../json.h"
#include "../workspace.h"

#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>
#include <string>

namespace pluto {

namespace {

// Extract file paths mentioned in unified diff headers (--- a/... and +++ b/...).
static std::set<std::string> extract_patch_paths(const std::string& patch) {
	std::set<std::string> paths;
	std::istringstream ss(patch);
	std::string line;
	while (std::getline(ss, line)) {
		// +++ b/path or +++ path (skip /dev/null for new files)
		if (line.size() > 4 && line.substr(0, 4) == "+++ ") {
			std::string p = line.substr(4);
			// Strip "b/" prefix from git-style diffs.
			if (p.size() > 2 && p[0] == 'b' && p[1] == '/')
				p = p.substr(2);
			// Strip trailing timestamp if present (patch -p0 style).
			auto tab = p.find('\t');
			if (tab != std::string::npos) p.resize(tab);
			if (p != "/dev/null" && !p.empty())
				paths.insert(p);
		}
	}
	return paths;
}

// Shell-safe quoting: wraps in single quotes, escaping embedded quotes.
static std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

// Split command string into argv array (simple shell-like parsing, no shell execution)
static std::vector<std::string> split_command(const std::string& cmd) {
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

// Execute command via fork/execvp with argv array (no shell).
// Returns exit code, or -1 on error.
static int execvp_capture(const std::vector<std::string>& argv, std::string& output) {
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
		std::vector<char*> args;
		args.reserve(argv.size() + 1);
		for (const auto& s : argv) args.push_back(const_cast<char*>(s.c_str()));
		args.push_back(nullptr);
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

}  // namespace

class ApplyPatchTool : public Tool {
public:
	std::string name() const override { return "apply_patch"; }

	std::string description() const override {
		return "Apply a unified diff patch to one or more files. More efficient "
		       "than edit_file for large or multi-file changes. The patch must "
		       "be in unified diff format (as produced by `diff -u` or `git "
		       "diff`). Uses strip level 1 by default (git-style a/b/ prefixes). "
		       "Set strip to 0 for paths without prefix.";
	}

	std::string input_schema_json() const override {
		return R"({"type":"object","properties":{)"
		       R"("patch":{"type":"string","description":"Unified diff content"},)"
		       R"("strip":{"type":"integer","description":"Strip leading path components (-p flag), default 1"}})"
		       R"(,"required":["patch"]})";
	}

	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			auto paths = extract_patch_paths(v.str("patch"));
			if (paths.empty())
				return "apply_patch (no files detected)";
			std::string out = "apply_patch";
			int n = 0;
			for (const auto& p : paths) {
				out += " " + p;
				if (++n >= 5 && paths.size() > 5) {
					out += " (+" + std::to_string(paths.size() - 5) + " more)";
					break;
				}
			}
			return out;
		} catch (...) { return Tool::preview(input_json); }
	}

	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string patch;
		int strip = 1;
		try {
			auto v = json::Value::parse(input_json);
			patch = v.str("patch");
			if (const auto* s = v.find("strip"); s && s->is_num())
				strip = static_cast<int>(s->as_num());
		} catch (const std::exception& e) {
			r.is_error = true;
			r.output = std::string("args: ") + e.what();
			return r;
		}
		if (patch.empty()) {
			r.is_error = true;
			r.output = "patch content required";
			return r;
		}
		if (strip < 0 || strip > 8) strip = 1;

		// Validate that all target files are within the workspace.
		auto paths = extract_patch_paths(patch);
		std::string ws = WorkspaceRoot();
		for (const auto& p : paths) {
			// Resolve relative paths against workspace root and canonicalize
			// to prevent traversal via ".." or symlinks.
			std::string full = p;
			if (!p.empty() && p[0] != '/')
				full = ws + "/" + p;
			char* resolved = realpath(full.c_str(), nullptr);
			if (resolved) { full = resolved; free(resolved); }
			if (!WorkspaceAllows(full)) {
				LogAccess(full, "apply_patch", false);
				r.is_error = true;
				r.output = full + " is outside workspace " + ws
				         + " — use `pluto trust add <dir>` to allow";
				return r;
			}
			LogAccess(full, "apply_patch", true);
		}

		// Write patch to a temporary file.
		char tmp[] = "/tmp/pluto-patch-XXXXXX";
		int fd = mkstemp(tmp);
		if (fd < 0) {
			r.is_error = true;
			r.output = "mkstemp failed: " + std::string(std::strerror(errno));
			return r;
		}
		{
			ssize_t written = ::write(fd, patch.data(), patch.size());
			::close(fd);
			if (written < 0 || static_cast<size_t>(written) != patch.size()) {
				::unlink(tmp);
				r.is_error = true;
				r.output = "failed to write temp patch file";
				return r;
			}
		}

		// Run patch --dry-run first to validate.
		std::string cd_prefix;
		if (!ws.empty())
			cd_prefix = "cd " + shq(ws) + " && ";

		std::string dry_cmd = cd_prefix + "patch --dry-run -p"
		                    + std::to_string(strip) + " < " + shq(tmp)
		                    + " 2>&1";
		std::string dry_out;
		{
			auto argv = split_command(dry_cmd);
			if (argv.empty()) {
				::unlink(tmp);
				r.is_error = true;
				r.output = "empty command";
				return r;
			}
			int rc = execvp_capture(argv, dry_out);
			if (rc != 0) {
				::unlink(tmp);
				r.is_error = true;
				r.output = "patch --dry-run failed:\n" + dry_out;
				return r;
			}
		}

		// Apply for real.
		std::string apply_cmd = cd_prefix + "patch -p"
		                      + std::to_string(strip) + " < " + shq(tmp)
		                      + " 2>&1";
		std::string apply_out;
		{
			auto argv = split_command(apply_cmd);
			if (argv.empty()) {
				::unlink(tmp);
				r.is_error = true;
				r.output = "empty command";
				return r;
			}
			int rc = execvp_capture(argv, apply_out);
			::unlink(tmp);
			if (rc != 0) {
				r.is_error = true;
				r.output = "patch failed:\n" + apply_out;
				return r;
			}
		}

		r.output = apply_out;
		return r;
	}
};

std::unique_ptr<Tool> MakeApplyPatchTool() {
	return std::make_unique<ApplyPatchTool>();
}

}  // namespace pluto
