#include "agent_store.h"
#include "util.h"

#include <Entry.h>
#include <Directory.h>
#include <Path.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

std::string home_dir() {
	return HomeDir();
}

void ensure_dir(const std::string& dir) {
	std::string acc;
	for (size_t i = 0; i < dir.size(); ++i) {
		acc += dir[i];
		if (dir[i] == '/' && i > 0) ::mkdir(acc.c_str(), 0755);
	}
	::mkdir(dir.c_str(), 0755);
}

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Shell-quote a string: wraps in single quotes, escaping embedded quotes.
std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

bool parse_fm_line(const std::string& line, std::string& key, std::string& val) {
	size_t colon = line.find(':');
	if (colon == std::string::npos) return false;
	key = trim(line.substr(0, colon));
	val = trim(line.substr(colon + 1));
	return !key.empty();
}

// Split a comma-separated string into trimmed tokens.
std::vector<std::string> split_csv(const std::string& s) {
	std::vector<std::string> out;
	std::istringstream ss(s);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		std::string t = trim(tok);
		if (!t.empty()) out.push_back(t);
	}
	return out;
}

// Copy a file from src to dst.
bool copy_file(const std::string& src, const std::string& dst) {
	std::ifstream in(src, std::ios::binary);
	if (!in) return false;
	std::ofstream out(dst, std::ios::binary | std::ios::trunc);
	if (!out) return false;
	out << in.rdbuf();
	return out.good();
}

// Copy a directory recursively (simple implementation).
bool copy_dir(const std::string& src, const std::string& dst) {
	ensure_dir(dst);
	DIR* d = opendir(src.c_str());
	if (!d) return false;

	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name == "." || name == "..") continue;
		if (name == ".git") continue;  // skip .git

		std::string sp = src + "/" + name;
		std::string dp = dst + "/" + name;

		struct stat st;
		if (::stat(sp.c_str(), &st) != 0) continue;

		if (S_ISDIR(st.st_mode)) {
			if (!copy_dir(sp, dp)) { closedir(d); return false; }
		} else {
			if (!copy_file(sp, dp)) { closedir(d); return false; }
		}
	}
	closedir(d);
	return true;
}

void rm_rf(const std::string& path) {
	// Use native Haiku APIs instead of system() for security
	BEntry entry(path.c_str());
	if (entry.InitCheck() != B_OK) return;
	if (entry.IsDirectory()) {
		BDirectory dir(&entry);
		BEntry child;
		while (dir.GetNextEntry(&child) == B_OK) {
			BPath p;
			if (child.GetPath(&p) == B_OK) {
				rm_rf(p.Path());
			}
		}
	}
	entry.Remove();
}

std::string settings_dir() {
	return home_dir() + "/config/settings/pluto";
}

std::string skills_dir() {
	return settings_dir() + "/skills";
}

std::string routines_dir() {
	return settings_dir() + "/routines";
}

std::string commands_dir() {
	return settings_dir() + "/commands";
}

// Register skill from agent: symlink or copy skill dir.
bool register_skill(const std::string& agent_dir, const std::string& skill_name) {
	std::string src = agent_dir + "/skills/" + skill_name;
	std::string dst = skills_dir() + "/" + skill_name;

	struct stat st;
	if (::stat(src.c_str(), &st) != 0) return false;

	ensure_dir(skills_dir());
	if (S_ISDIR(st.st_mode)) {
		return copy_dir(src, dst);
	} else {
		// Single file: create dir + SKILL.md
		ensure_dir(dst);
		return copy_file(src, dst + "/SKILL.md");
	}
}

// Register routine from agent.
bool register_routine(const std::string& agent_dir, const std::string& routine_name) {
	std::string src = agent_dir + "/routines/" + routine_name + ".md";
	std::string dst = routines_dir() + "/" + routine_name + ".md";

	struct stat st;
	if (::stat(src.c_str(), &st) != 0) {
		// Try without .md extension (already has it).
		src = agent_dir + "/routines/" + routine_name;
		if (::stat(src.c_str(), &st) != 0) return false;
	}

	ensure_dir(routines_dir());
	return copy_file(src, dst);
}

// Register command from agent.
bool register_command(const std::string& agent_dir, const std::string& cmd_name) {
	std::string src = agent_dir + "/commands/" + cmd_name + ".md";
	std::string dst = commands_dir() + "/" + cmd_name + ".md";

	struct stat st;
	if (::stat(src.c_str(), &st) != 0) {
		src = agent_dir + "/commands/" + cmd_name;
		if (::stat(src.c_str(), &st) != 0) return false;
	}

	ensure_dir(commands_dir());
	return copy_file(src, dst);
}

// Unregister skill.
void unregister_skill(const std::string& skill_name) {
	std::string path = skills_dir() + "/" + skill_name;
	rm_rf(path);
}

// Unregister routine.
void unregister_routine(const std::string& routine_name) {
	std::string path = routines_dir() + "/" + routine_name + ".md";
	::unlink(path.c_str());
}

// Unregister command.
void unregister_command(const std::string& cmd_name) {
	std::string path = commands_dir() + "/" + cmd_name + ".md";
	::unlink(path.c_str());
}

void usage(const char* prog) {
	std::fprintf(stderr,
		"usage: %s agent install <path|url> [--trust]\n"
		"       %s agent remove <name>\n"
		"       %s agent update <name> | --all\n"
		"       %s agent search <query>\n"
		"       %s agent list\n"
		"       %s agent info <name>\n"
		"       %s agent run <file.md>\n",
		prog, prog, prog, prog, prog, prog, prog);
}

}  // namespace

std::string AgentsDir() {
	return settings_dir() + "/agents";
}

bool ParseAgent(const std::string& path, Agent& out) {
	std::ifstream f(path);
	if (!f) return false;

	out = Agent{};
	out.path = path;

	// Extract directory name as default agent name.
	std::string dir = path;
	size_t slash = dir.rfind('/');
	if (slash != std::string::npos) {
		dir = dir.substr(0, slash);
		size_t prev = dir.rfind('/');
		if (prev != std::string::npos)
			out.name = dir.substr(prev + 1);
	}

	std::string content;
	{
		std::ostringstream os;
		os << f.rdbuf();
		content = os.str();
	}

	// Parse YAML-ish frontmatter (--- ... ---).
	if (content.size() >= 4 && content.substr(0, 4) == "---\n") {
		size_t end = content.find("\n---\n", 4);
		if (end == std::string::npos) {
			// No closing --- : treat entire file as body, no frontmatter.
			return true;
		}
		std::string fm = content.substr(4, end - 4);

		std::istringstream ss(fm);
		std::string line;
		while (std::getline(ss, line)) {
			std::string key, val;
			if (!parse_fm_line(line, key, val)) continue;
			if (key == "name") out.name = val;
			else if (key == "description") out.description = val;
			else if (key == "author") out.author = val;
			else if (key == "version") out.version = val;
			else if (key == "tools") out.tools = split_csv(val);
			else if (key == "skills") out.skills = split_csv(val);
			else if (key == "routines") out.routines = split_csv(val);
			else if (key == "commands") out.commands = split_csv(val);
		}
	}

	return !out.name.empty();
}

std::vector<Agent> ListAgents() {
	std::vector<Agent> out;
	std::string dir = AgentsDir();
	DIR* d = opendir(dir.c_str());
	if (!d) return out;

	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name == "." || name == "..") continue;

		std::string agent_md = dir + "/" + name + "/AGENT.md";
		Agent a;
		if (ParseAgent(agent_md, a)) {
			out.push_back(std::move(a));
		}
	}
	closedir(d);

	std::sort(out.begin(), out.end(),
		[](const Agent& a, const Agent& b) { return a.name < b.name; });
	return out;
}

bool PreviewAgent(const std::string& source_dir, Agent& agent,
                   std::string& body) {
	std::string agent_md = source_dir + "/AGENT.md";
	if (!ParseAgent(agent_md, agent))
		return false;

	// Extract body (after frontmatter).
	std::ifstream f(agent_md);
	if (!f) return false;
	std::ostringstream os;
	os << f.rdbuf();
	std::string content = os.str();

	if (content.size() >= 4 && content.substr(0, 4) == "---\n") {
		size_t end = content.find("\n---\n", 4);
		if (end != std::string::npos)
			body = content.substr(end + 5);
		else
			body = content;
	} else {
		body = content;
	}

	// Trim leading whitespace.
	size_t first = body.find_first_not_of("\n\r ");
	if (first != std::string::npos && first > 0)
		body = body.substr(first);

	return true;
}

bool InstallAgent(const std::string& source_dir, std::string& error) {
	// Look for AGENT.md in the source directory.
	std::string agent_md = source_dir + "/AGENT.md";
	Agent agent;
	if (!ParseAgent(agent_md, agent)) {
		error = "no valid AGENT.md found in " + source_dir;
		return false;
	}

	if (agent.name.empty()) {
		error = "agent has no name";
		return false;
	}

	// Destination directory.
	std::string dst = AgentsDir() + "/" + agent.name;

	// Check if already installed.
	struct stat st;
	if (::stat(dst.c_str(), &st) == 0) {
		error = "agent '" + agent.name + "' is already installed; "
		        "remove it first with `pluto agent remove " + agent.name + "`";
		return false;
	}

	// Copy the entire agent directory.
	ensure_dir(AgentsDir());
	if (!copy_dir(source_dir, dst)) {
		error = "failed to copy agent files to " + dst;
		return false;
	}

	// Register bundled resources.
	for (const auto& s : agent.skills)
		register_skill(dst, s);
	for (const auto& r : agent.routines)
		register_routine(dst, r);
	for (const auto& c : agent.commands)
		register_command(dst, c);

	return true;
}

bool InstallAgentFromUrl(const std::string& url, std::string& error) {
	// Clone to a temp directory.
	char tmpl[] = "/tmp/pluto-agent-XXXXXX";
	const char* tmpdir = mkdtemp(tmpl);
	if (!tmpdir) {
		error = "failed to create temp directory";
		return false;
	}
	std::string tmp = std::string(tmpdir);

	// SECURITY NOTE: git clone does not verify code signatures.
	// A compromised repository could contain arbitrary code in
	// skills, routines, or hook commands. The interactive preview
	// (shown before install) is the user's only line of defense.
	std::fprintf(stderr, "[agent] cloning %s (unverified source)\n",
		url.c_str());

	std::string cmd = "git clone --depth=1 " + shq(url) + " " + shq(tmp) + " 2>&1";
    // Use fork/exec instead of popen for security
    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "fork failed\n");
        rm_rf(tmp);
        return false;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c",
            ("git clone --depth=1 " + shq(url) + " " + shq(tmp) + " 2>&1").c_str(),
        nullptr);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::fprintf(stderr, "git clone failed (exit code %d)\n",
            WEXITSTATUS(status));
        rm_rf(tmp);
        return false;
    }

	// Check if AGENT.md is at root or in a subdirectory.
	struct stat st;
	std::string agent_md = tmp + "/AGENT.md";
	if (::stat(agent_md.c_str(), &st) == 0) {
		bool ok = InstallAgent(tmp, error);
		if (ok) {
			// Save the source URL for future updates.
			Agent a;
			if (ParseAgent(agent_md, a) && !a.name.empty()) {
				std::string src_file = AgentsDir() + "/" + a.name + "/.agent-source";
				std::ofstream sf(src_file, std::ios::trunc);
				if (sf) sf << url << "\n";
			}
		}
		rm_rf(tmp);
		return ok;
	}

	// Try subdirectories (e.g. agents/name/AGENT.md in a registry repo).
	DIR* d = opendir(tmp.c_str());
	if (!d) {
		error = "no AGENT.md found in cloned repository";
		rm_rf(tmp);
		return false;
	}

	bool installed_any = false;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name == "." || name == ".." || name == ".git") continue;
		std::string sub = tmp + "/" + name + "/AGENT.md";
		if (::stat(sub.c_str(), &st) == 0) {
			std::string sub_err;
			if (InstallAgent(tmp + "/" + name, sub_err)) {
				// Save source URL for each installed agent.
				std::string src_file = AgentsDir() + "/" + name + "/.agent-source";
				std::ofstream sf(src_file, std::ios::trunc);
				if (sf) sf << url << "\n";
				installed_any = true;
			} else {
				std::fprintf(stderr, "  skip %s: %s\n", name.c_str(), sub_err.c_str());
			}
		}
	}
	closedir(d);

	rm_rf(tmp);
	if (!installed_any) {
		error = "no installable agents found in repository";
		return false;
	}
	return true;
}

bool RemoveAgent(const std::string& name, std::string& error) {
	std::string dir = AgentsDir() + "/" + name;
	std::string agent_md = dir + "/AGENT.md";

	Agent agent;
	if (!ParseAgent(agent_md, agent)) {
		error = "agent '" + name + "' is not installed";
		return false;
	}

	// Unregister bundled resources.
	for (const auto& s : agent.skills)
		unregister_skill(s);
	for (const auto& r : agent.routines)
		unregister_routine(r);
	for (const auto& c : agent.commands)
		unregister_command(c);

	// Remove agent directory.
	rm_rf(dir);
	return true;
}

std::vector<std::string> CheckAgentDeps(const Agent& a,
                                           const std::vector<std::string>& available) {
	std::vector<std::string> missing;
	for (const auto& t : a.tools) {
		if (std::find(available.begin(), available.end(), t) == available.end())
			missing.push_back(t);
	}
	return missing;
}

std::string AgentSourceUrl(const std::string& name) {
	std::string path = AgentsDir() + "/" + name + "/.agent-source";
	std::ifstream f(path);
	if (!f) return {};
	std::string url;
	std::getline(f, url);
	return trim(url);
}

bool UpdateAgent(const std::string& name, std::string& error) {
	// Check agent exists.
	std::string agent_md = AgentsDir() + "/" + name + "/AGENT.md";
	Agent old_agent;
	if (!ParseAgent(agent_md, old_agent)) {
		error = "agent '" + name + "' is not installed";
		return false;
	}

	// Need a source URL.
	std::string url = AgentSourceUrl(name);
	if (url.empty()) {
		error = "agent '" + name + "' was installed locally (no source URL); "
		        "reinstall from URL to enable updates";
		return false;
	}

	// Clone to temp dir.
	char tmpl[] = "/tmp/pluto-agent-XXXXXX";
	const char* tmpdir = mkdtemp(tmpl);
	if (!tmpdir) {
		error = "failed to create temp directory";
		return false;
	}
	std::string tmp = std::string(tmpdir);

	std::string cmd = "git clone --depth=1 " + shq(url) + " " + shq(tmp) + " 2>&1";
    // Use fork/exec instead of popen for security
    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "fork failed\n");
        rm_rf(tmp);
        return false;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c",
            ("git clone --depth=1 " + shq(url) + " " + shq(tmp) + " 2>&1").c_str(),
        nullptr);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::fprintf(stderr, "git clone failed (exit code %d)\n",
            WEXITSTATUS(status));
        rm_rf(tmp);
        return false;
    }

	// Find the AGENT.md in the cloned repo (root or subdir).
	std::string new_agent_dir = tmp;
	struct stat st;
	std::string new_md = tmp + "/AGENT.md";
	if (::stat(new_md.c_str(), &st) != 0) {
		// Try subdirectory with agent name.
		new_md = tmp + "/" + name + "/AGENT.md";
		if (::stat(new_md.c_str(), &st) == 0) {
			new_agent_dir = tmp + "/" + name;
		} else {
			error = "AGENT.md not found in repository";
			rm_rf(tmp);
			return false;
		}
	}

	Agent new_agent;
	if (!ParseAgent(new_md, new_agent)) {
		error = "failed to parse remote AGENT.md";
		rm_rf(tmp);
		return false;
	}

	// Compare versions.
	if (!old_agent.version.empty() && !new_agent.version.empty()
	    && old_agent.version == new_agent.version) {
		rm_rf(tmp);
		error = "";  // not an error, just up to date
		return false;
	}

	// Remove old, install new.
	std::string rem_err;
	if (!RemoveAgent(name, rem_err)) {
		error = "failed to remove old version: " + rem_err;
		rm_rf(tmp);
		return false;
	}

	std::string inst_err;
	if (!InstallAgent(new_agent_dir, inst_err)) {
		error = "failed to install new version: " + inst_err;
		rm_rf(tmp);
		return false;
	}

	// Restore source URL.
	std::string src_file = AgentsDir() + "/" + name + "/.agent-source";
	std::ofstream sf(src_file, std::ios::trunc);
	if (sf) sf << url << "\n";

	rm_rf(tmp);
	return true;
}

int UpdateAllAgents() {
	auto agents = ListAgents();
	int updated = 0;
	for (const auto& a : agents) {
		std::string url = AgentSourceUrl(a.name);
		if (url.empty()) continue;  // skip locally-installed agents

		std::printf("Checking %s...\n", a.name.c_str());
		std::string error;
		if (UpdateAgent(a.name, error)) {
			std::printf("  Updated %s to new version.\n", a.name.c_str());
			++updated;
		} else if (error.empty()) {
			std::printf("  Already up to date.\n");
		} else {
			std::fprintf(stderr, "  Failed: %s\n", error.c_str());
		}
	}
	return updated;
}

// Case-insensitive substring search helper.
static bool icontains(const std::string& haystack, const std::string& needle) {
	if (needle.empty()) return true;
	auto it = std::search(haystack.begin(), haystack.end(),
		needle.begin(), needle.end(),
		[](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a))
			    == std::tolower(static_cast<unsigned char>(b));
		});
	return it != haystack.end();
}

static bool agent_matches(const Agent& a, const std::string& query) {
	if (icontains(a.name, query)) return true;
	if (icontains(a.description, query)) return true;
	if (icontains(a.author, query)) return true;
	for (const auto& t : a.tools)
		if (icontains(t, query)) return true;
	for (const auto& s : a.skills)
		if (icontains(s, query)) return true;
	return false;
}

std::vector<Agent> SearchAgents(const std::string& query) {
	std::vector<Agent> results;

	// Search installed agents.
	auto installed = ListAgents();
	for (auto& a : installed) {
		if (agent_matches(a, query))
			results.push_back(std::move(a));
	}

	// Also search example agents (if the examples directory exists).
	// Check relative path (works when running from the source tree)
	// and the installed system data path.
	std::vector<std::string> example_dirs;
	struct stat st;
	if (::stat("examples/agents", &st) == 0 && S_ISDIR(st.st_mode))
		example_dirs.push_back("examples/agents");
	std::string sys_examples = "/boot/system/data/pluto/examples/agents";
	if (::stat(sys_examples.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		example_dirs.push_back(sys_examples);

	for (const auto& edir : example_dirs) {
		DIR* d = opendir(edir.c_str());
		if (!d) continue;
		struct dirent* e;
		while ((e = readdir(d)) != nullptr) {
			std::string name = e->d_name;
			if (name == "." || name == "..") continue;
			std::string md = edir + "/" + name + "/AGENT.md";
			Agent a;
			if (ParseAgent(md, a) && agent_matches(a, query)) {
				// Skip if already in results (installed version).
				bool dup = false;
				for (const auto& r : results)
					if (r.name == a.name) { dup = true; break; }
				if (!dup) {
					a.path = md;  // mark as example
					results.push_back(std::move(a));
				}
			}
		}
		closedir(d);
	}

	std::sort(results.begin(), results.end(),
		[](const Agent& a, const Agent& b) { return a.name < b.name; });
	return results;
}

int RunAgentStoreCli(int argc, char** argv) {
	if (argc < 1) {
		usage("pluto");
		return 2;
	}

	std::string sub = argv[0];

	if (sub == "list") {
		auto agents = ListAgents();
		if (agents.empty()) {
			std::printf("(no agents installed)\n");
			return 0;
		}
		for (const auto& a : agents) {
			std::printf("  %-20s", a.name.c_str());
			if (!a.version.empty()) std::printf(" v%s", a.version.c_str());
			if (!a.description.empty()) std::printf("  %s", a.description.c_str());
			std::printf("\n");
		}
		return 0;
	}

	if (sub == "install") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto agent install <path|url> [--trust]\n");
			return 2;
		}
		std::string source = argv[1];
		bool trust = false;
		for (int i = 2; i < argc; ++i) {
			if (std::string(argv[i]) == "--trust") trust = true;
		}

		std::string error;

		// Determine if it's a URL or a local path.
		bool is_url = (source.find("://") != std::string::npos)
		           || (source.find("git@") == 0);

		// If it's not a URL and doesn't look like a path, try the
		// default registry (short name install).
		struct stat src_st;
		if (!is_url && ::stat(source.c_str(), &src_st) != 0) {
			std::printf("Resolving '%s' from default registry...\n",
				source.c_str());
			if (InstallAgentFromRegistry(source, error)) {
				std::printf("Installed '%s' from registry.\n", source.c_str());
				return 0;
			}
			std::fprintf(stderr, "install failed: %s\n", error.c_str());
			return 1;
		}

		// For URL installs, clone first to preview.
		std::string preview_dir;
		std::string tmp;
		if (is_url) {
			std::printf("Cloning %s...\n", source.c_str());
			char tmpl[] = "/tmp/pluto-agent-XXXXXX";
			const char* tmpdir = mkdtemp(tmpl);
			if (!tmpdir) {
				std::fprintf(stderr, "install failed: cannot create temp dir\n");
				return 1;
			}
			tmp = std::string(tmpdir);
			std::string cmd = "git clone --depth=1 " + shq(source) + " "
				+ shq(tmp) + " 2>&1";
    // Use fork/exec instead of popen for security
    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "install failed: fork failed\n");
        rm_rf(tmp);
        return 1;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c",
            ("git clone --depth=1 " + shq(source) + " " + shq(tmp) + " 2>&1").c_str(),
        nullptr);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::fprintf(stderr, "git clone failed (exit code %d)\n",
            WEXITSTATUS(status));
        rm_rf(tmp);
        return 1;
    }
			preview_dir = tmp;
			// Check for subdir agent.
			struct stat st;
			if (::stat((tmp + "/AGENT.md").c_str(), &st) != 0) {
				// Look in subdirectories.
				DIR* d = opendir(tmp.c_str());
				if (d) {
					struct dirent* e;
					while ((e = readdir(d)) != nullptr) {
						std::string n = e->d_name;
						if (n == "." || n == ".." || n == ".git") continue;
						if (::stat((tmp + "/" + n + "/AGENT.md").c_str(), &st) == 0) {
							preview_dir = tmp + "/" + n;
							break;
						}
					}
					closedir(d);
				}
			}
		} else {
			preview_dir = source;
		}

		// Security review: show prompt before installing.
		if (!trust) {
			Agent preview_a;
			std::string body;
			if (PreviewAgent(preview_dir, preview_a, body)) {
				std::printf("\n=== Agent Review ===\n");
				std::printf("Name:    %s\n", preview_a.name.c_str());
				if (!preview_a.version.empty())
					std::printf("Version: %s\n", preview_a.version.c_str());
				if (!preview_a.author.empty())
					std::printf("Author:  %s\n", preview_a.author.c_str());
				if (!preview_a.tools.empty()) {
					std::printf("Tools:   ");
					for (size_t i = 0; i < preview_a.tools.size(); ++i) {
						if (i > 0) std::printf(", ");
						std::printf("%s", preview_a.tools[i].c_str());
					}
					std::printf("\n");
				}
				std::printf("\n--- Prompt ---\n");
				// Show first 80 lines of the prompt for security review.
				std::istringstream ss(body);
				std::string line;
				int lines = 0;
				int total_lines = 0;
				// Count total lines first.
				{ std::istringstream cs(body); std::string tmp;
				  while (std::getline(cs, tmp)) ++total_lines; }
				while (std::getline(ss, line) && lines < 80) {
					std::printf("%s\n", line.c_str());
					++lines;
				}
				if (lines >= 80 && !ss.eof())
					std::printf("  ... (%d more lines, %zu bytes total)\n",
						total_lines - lines, body.size());
				std::printf("--- End ---\n\n");

				std::printf("Install this agent? [y/N] ");
				std::fflush(stdout);
				char ch = 'n';
				if (std::scanf(" %c", &ch) != 1) ch = 'n';
				if (ch != 'y' && ch != 'Y') {
					std::printf("Installation cancelled.\n");
					if (!tmp.empty()) rm_rf(tmp);
					return 0;
				}
			}
		}

		bool ok;
		if (is_url) {
			// Already cloned — install from temp dir.
			struct stat st;
			if (::stat((tmp + "/AGENT.md").c_str(), &st) == 0) {
				ok = InstallAgent(tmp, error);
				if (ok) {
					// Save source URL.
					Agent a;
					if (ParseAgent(tmp + "/AGENT.md", a) && !a.name.empty()) {
						std::string sf = AgentsDir() + "/" + a.name + "/.agent-source";
						std::ofstream f(sf, std::ios::trunc);
						if (f) f << source << "\n";
					}
				}
			} else {
				// Registry: install from subdirs.
				ok = false;
				DIR* d = opendir(tmp.c_str());
				if (d) {
					struct dirent* e;
					while ((e = readdir(d)) != nullptr) {
						std::string n = e->d_name;
						if (n == "." || n == ".." || n == ".git") continue;
						std::string sub_md = tmp + "/" + n + "/AGENT.md";
						if (::stat(sub_md.c_str(), &st) == 0) {
							std::string sub_err;
							if (InstallAgent(tmp + "/" + n, sub_err)) {
								std::string sf = AgentsDir() + "/" + n + "/.agent-source";
								std::ofstream f(sf, std::ios::trunc);
								if (f) f << source << "\n";
								ok = true;
							} else {
								std::fprintf(stderr, "  skip %s: %s\n",
									n.c_str(), sub_err.c_str());
							}
						}
					}
					closedir(d);
				}
				if (!ok) error = "no installable agents found";
			}
			rm_rf(tmp);
		} else {
			ok = InstallAgent(source, error);
		}

		if (!ok) {
			std::fprintf(stderr, "install failed: %s\n", error.c_str());
			return 1;
		}
		std::printf("Agent installed successfully.\n");
		return 0;
	}

	if (sub == "remove") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto agent remove <name>\n");
			return 2;
		}
		std::string name = argv[1];
		std::string error;
		if (!RemoveAgent(name, error)) {
			std::fprintf(stderr, "remove failed: %s\n", error.c_str());
			return 1;
		}
		std::printf("Agent '%s' removed.\n", name.c_str());
		return 0;
	}

	if (sub == "info") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto agent info <name>\n");
			return 2;
		}
		std::string name = argv[1];
		std::string agent_md = AgentsDir() + "/" + name + "/AGENT.md";
		Agent a;
		if (!ParseAgent(agent_md, a)) {
			std::fprintf(stderr, "agent '%s' not found\n", name.c_str());
			return 1;
		}
		std::printf("Name:        %s\n", a.name.c_str());
		if (!a.version.empty())
			std::printf("Version:     %s\n", a.version.c_str());
		if (!a.author.empty())
			std::printf("Author:      %s\n", a.author.c_str());
		if (!a.description.empty())
			std::printf("Description: %s\n", a.description.c_str());
		if (!a.tools.empty()) {
			std::printf("Tools:       ");
			for (size_t i = 0; i < a.tools.size(); ++i) {
				if (i > 0) std::printf(", ");
				std::printf("%s", a.tools[i].c_str());
			}
			std::printf("\n");
		}
		if (!a.skills.empty()) {
			std::printf("Skills:      ");
			for (size_t i = 0; i < a.skills.size(); ++i) {
				if (i > 0) std::printf(", ");
				std::printf("%s", a.skills[i].c_str());
			}
			std::printf("\n");
		}
		if (!a.routines.empty()) {
			std::printf("Routines:    ");
			for (size_t i = 0; i < a.routines.size(); ++i) {
				if (i > 0) std::printf(", ");
				std::printf("%s", a.routines[i].c_str());
			}
			std::printf("\n");
		}
		if (!a.commands.empty()) {
			std::printf("Commands:    ");
			for (size_t i = 0; i < a.commands.size(); ++i) {
				if (i > 0) std::printf(", ");
				std::printf("%s", a.commands[i].c_str());
			}
			std::printf("\n");
		}

		// Print the prompt body.
		std::ifstream f(agent_md);
		if (f) {
			std::string content;
			std::ostringstream os;
			os << f.rdbuf();
			content = os.str();
			// Skip frontmatter.
			if (content.size() >= 4 && content.substr(0, 4) == "---\n") {
				size_t end = content.find("\n---\n", 4);
				if (end != std::string::npos) {
					std::string body = content.substr(end + 5);
					if (!body.empty()) {
						std::printf("\n--- Prompt ---\n%s", body.c_str());
						if (body.back() != '\n') std::printf("\n");
					}
				}
			}
		}
		return 0;
	}

	if (sub == "search") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto agent search <query>\n");
			return 2;
		}
		std::string query = argv[1];
		auto results = SearchAgents(query);
		if (results.empty()) {
			std::printf("No agents matching \"%s\".\n", query.c_str());
			return 0;
		}
		std::printf("Found %zu agent(s) matching \"%s\":\n\n",
			results.size(), query.c_str());
		for (const auto& a : results) {
			bool installed = (a.path.find("/agents/") != std::string::npos
			               && a.path.find("/examples/") == std::string::npos);
			std::printf("  %-20s", a.name.c_str());
			if (!a.version.empty()) std::printf(" v%s", a.version.c_str());
			if (installed)
				std::printf("  [installed]");
			else
				std::printf("  [example]");
			if (!a.description.empty())
				std::printf("  %s", a.description.c_str());
			std::printf("\n");
		}
		return 0;
	}

	if (sub == "update") {
		if (argc >= 2 && std::string(argv[1]) == "--all") {
			int n = UpdateAllAgents();
			std::printf("%d agent(s) updated.\n", n);
			return 0;
		}
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto agent update <name> | --all\n");
			return 2;
		}
		std::string name = argv[1];
		std::string error;
		if (UpdateAgent(name, error)) {
			std::printf("Agent '%s' updated successfully.\n", name.c_str());
			return 0;
		}
		if (error.empty()) {
			std::printf("Agent '%s' is already up to date.\n", name.c_str());
			return 0;
		}
		std::fprintf(stderr, "update failed: %s\n", error.c_str());
		return 1;
	}

	// Unknown subcommand under `pluto agent`.
	if (sub == "run") {
		// `pluto agent run` is handled in main.cpp before we get here.
		std::fprintf(stderr, "usage: pluto agent run <file.md>\n");
		return 2;
	}

	usage("pluto");
	return 2;
}

const char* kDefaultRegistryUrl =
	"https://codeberg.org/atomozero/pluto-agents";

bool InstallAgentFromRegistry(const std::string& name, std::string& error) {
	// Clone the registry (shallow), find the agent directory, install it.
	char tmpl[] = "/tmp/pluto-registry-XXXXXX";
	const char* tmpdir = mkdtemp(tmpl);
	if (!tmpdir) {
		error = "failed to create temp directory";
		return false;
	}
	std::string tmp = std::string(tmpdir);

	std::string cmd = "git clone --depth=1 "
		+ shq(std::string(kDefaultRegistryUrl)) + " " + shq(tmp) + " 2>&1";
	std::fprintf(stderr, "Fetching from default registry...\n");
    // Use fork/exec instead of popen for security
    pid_t pid = fork();
    if (pid < 0) {
        error = "fork failed";
        rm_rf(tmp);
        return false;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c",
            ("git clone --depth=1 " + shq(std::string(kDefaultRegistryUrl)) + " " + shq(tmp) + " 2>&1").c_str(),
            nullptr);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = "registry clone failed (exit code " + std::to_string(WEXITSTATUS(status)) + ")";
        rm_rf(tmp);
        return false;
    }

	// Look for the agent in agents/<name>/ or <name>/
	std::string agent_dir;
	struct stat st;
	std::string try1 = tmp + "/agents/" + name;
	std::string try2 = tmp + "/" + name;
	if (::stat((try1 + "/AGENT.md").c_str(), &st) == 0)
		agent_dir = try1;
	else if (::stat((try2 + "/AGENT.md").c_str(), &st) == 0)
		agent_dir = try2;
	else {
		error = "agent '" + name + "' not found in registry";
		rm_rf(tmp);
		return false;
	}

	bool ok = InstallAgent(agent_dir, error);
	if (ok) {
		// Save the registry URL + name as source for future updates.
		Agent a;
		if (ParseAgent(agent_dir + "/AGENT.md", a) && !a.name.empty()) {
			std::string src_file = AgentsDir() + "/" + a.name + "/.agent-source";
			std::ofstream sf(src_file, std::ios::trunc);
			if (sf) sf << kDefaultRegistryUrl << "#" << name << "\n";
		}
	}
	rm_rf(tmp);
	return ok;
}

}  // namespace pluto
