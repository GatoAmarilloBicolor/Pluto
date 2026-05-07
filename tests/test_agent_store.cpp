// Tests for agent store: parse, install, list, remove, dependency check.

#include "../src/agent_store.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	if ((a) != (b)) { \
		std::fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", \
			__FILE__, __LINE__, \
			std::string(a).c_str(), std::string(b).c_str()); \
		++g_failures; \
	} \
} while (0)

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-agent-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) { std::fprintf(stderr, "mkdtemp failed\n"); std::exit(2); }
	return std::string(d);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	std::system(cmd.c_str());
}

static void ensure_dir(const std::string& dir) {
	std::string cmd = "mkdir -p '" + dir + "'";
	std::system(cmd.c_str());
}

static void write_file(const std::string& path, const std::string& content) {
	std::ofstream f(path);
	f << content;
}

static std::string read_file(const std::string& path) {
	std::ifstream f(path);
	if (!f) return "";
	std::ostringstream os;
	os << f.rdbuf();
	return os.str();
}

// --- Test: parse AGENT.md with full frontmatter ---
static void test_parse_with_frontmatter(const std::string& dir) {
	std::string agent_dir = dir + "/test-agent";
	ensure_dir(agent_dir);

	write_file(agent_dir + "/AGENT.md",
		"---\n"
		"name: haiku-dev\n"
		"description: Haiku development assistant\n"
		"author: atomozero\n"
		"version: 1.0.0\n"
		"tools: read_attr, query_fs, bash\n"
		"skills: haiku-api\n"
		"routines: daily-review\n"
		"commands: explain-api\n"
		"---\n"
		"You are a Haiku OS development expert.\n"
		"Help the user write native Be API code.\n");

	pluto::Agent a;
	CHECK(pluto::ParseAgent(agent_dir + "/AGENT.md", a));
	CHECK_EQ(a.name, "haiku-dev");
	CHECK_EQ(a.description, "Haiku development assistant");
	CHECK_EQ(a.author, "atomozero");
	CHECK_EQ(a.version, "1.0.0");
	CHECK(a.tools.size() == 3);
	CHECK_EQ(a.tools[0], "read_attr");
	CHECK_EQ(a.tools[1], "query_fs");
	CHECK_EQ(a.tools[2], "bash");
	CHECK(a.skills.size() == 1);
	CHECK_EQ(a.skills[0], "haiku-api");
	CHECK(a.routines.size() == 1);
	CHECK_EQ(a.routines[0], "daily-review");
	CHECK(a.commands.size() == 1);
	CHECK_EQ(a.commands[0], "explain-api");
}

// --- Test: parse AGENT.md without frontmatter (name from dirname) ---
static void test_parse_without_frontmatter(const std::string& dir) {
	std::string agent_dir = dir + "/simple-agent";
	ensure_dir(agent_dir);

	write_file(agent_dir + "/AGENT.md",
		"You are a simple agent with no metadata.\n");

	pluto::Agent a;
	// Should still parse but name comes from directory.
	CHECK(pluto::ParseAgent(agent_dir + "/AGENT.md", a));
	CHECK_EQ(a.name, "simple-agent");
	CHECK(a.tools.empty());
	CHECK(a.skills.empty());
}

// --- Test: install agent from local directory ---
static void test_install_local(const std::string& dir) {
	std::string home = dir + "/home";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	// Create a source agent directory.
	std::string src = dir + "/src-agent";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\n"
		"name: test-agent\n"
		"description: A test agent\n"
		"version: 0.1.0\n"
		"---\n"
		"Test prompt.\n");

	std::string error;
	CHECK(pluto::InstallAgent(src, error));

	// Verify installed.
	std::string installed = home + "/config/settings/pluto/agents/test-agent/AGENT.md";
	struct stat st;
	CHECK(::stat(installed.c_str(), &st) == 0);

	// Verify content preserved.
	std::string content = read_file(installed);
	CHECK(content.find("Test prompt.") != std::string::npos);
}

// --- Test: install duplicate fails ---
static void test_install_duplicate(const std::string& dir) {
	std::string home = dir + "/home";
	// HOME already set from previous test — reuse.

	std::string src = dir + "/src-agent2";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\n"
		"name: test-agent\n"
		"---\n"
		"Duplicate.\n");

	std::string error;
	CHECK(!pluto::InstallAgent(src, error));
	CHECK(error.find("already installed") != std::string::npos);
}

// --- Test: list agents ---
static void test_list(const std::string& dir) {
	std::string home = dir + "/home2";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	// Install two agents.
	std::string src1 = dir + "/agent-b";
	ensure_dir(src1);
	write_file(src1 + "/AGENT.md",
		"---\nname: beta-agent\nversion: 2.0\n---\nBeta.\n");

	std::string src2 = dir + "/agent-a";
	ensure_dir(src2);
	write_file(src2 + "/AGENT.md",
		"---\nname: alpha-agent\nversion: 1.0\n---\nAlpha.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src1, err));
	CHECK(pluto::InstallAgent(src2, err));

	auto agents = pluto::ListAgents();
	CHECK(agents.size() == 2);
	// Should be sorted alphabetically.
	CHECK_EQ(agents[0].name, "alpha-agent");
	CHECK_EQ(agents[1].name, "beta-agent");
	CHECK_EQ(agents[1].version, "2.0");
}

// --- Test: remove agent ---
static void test_remove(const std::string& dir) {
	std::string home = dir + "/home3";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string src = dir + "/rm-agent";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\nname: removable\n---\nGoing away.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src, err));

	auto before = pluto::ListAgents();
	CHECK(before.size() == 1);

	CHECK(pluto::RemoveAgent("removable", err));

	auto after = pluto::ListAgents();
	CHECK(after.empty());

	// Remove again should fail.
	CHECK(!pluto::RemoveAgent("removable", err));
	CHECK(err.find("not installed") != std::string::npos);
}

// --- Test: dependency check ---
static void test_check_deps(const std::string&) {
	pluto::Agent a;
	a.tools = {"read_attr", "query_fs", "bash", "magic_tool"};

	std::vector<std::string> available = {"read_file", "write_file", "bash",
	                                       "read_attr", "query_fs"};
	auto missing = pluto::CheckAgentDeps(a, available);
	CHECK(missing.size() == 1);
	CHECK_EQ(missing[0], "magic_tool");
}

// --- Test: install with skills/routines/commands registration ---
static void test_install_with_resources(const std::string& dir) {
	std::string home = dir + "/home4";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	// Create agent with skills, routines, commands.
	std::string src = dir + "/full-agent";
	ensure_dir(src);
	ensure_dir(src + "/skills/my-skill");
	ensure_dir(src + "/routines");
	ensure_dir(src + "/commands");

	write_file(src + "/AGENT.md",
		"---\n"
		"name: full-agent\n"
		"skills: my-skill\n"
		"routines: my-routine\n"
		"commands: my-command\n"
		"---\n"
		"Full agent prompt.\n");

	write_file(src + "/skills/my-skill/SKILL.md",
		"You are a specialized skill.\n");
	write_file(src + "/routines/my-routine.md",
		"---\nname: my-routine\nschedule: manual\n---\nRoutine body.\n");
	write_file(src + "/commands/my-command.md",
		"description: A custom command\n\nCommand body with {{args}}.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src, err));

	// Verify skill was registered.
	std::string skill_path = home + "/config/settings/pluto/skills/my-skill/SKILL.md";
	CHECK(read_file(skill_path).find("specialized skill") != std::string::npos);

	// Verify routine was registered.
	std::string routine_path = home + "/config/settings/pluto/routines/my-routine.md";
	CHECK(read_file(routine_path).find("Routine body") != std::string::npos);

	// Verify command was registered.
	std::string cmd_path = home + "/config/settings/pluto/commands/my-command.md";
	CHECK(read_file(cmd_path).find("Command body") != std::string::npos);

	// Remove agent should clean up resources.
	CHECK(pluto::RemoveAgent("full-agent", err));

	struct stat st;
	CHECK(::stat(skill_path.c_str(), &st) != 0);   // skill removed
	CHECK(::stat(routine_path.c_str(), &st) != 0);  // routine removed
	CHECK(::stat(cmd_path.c_str(), &st) != 0);      // command removed
}

// --- Test: parse with no AGENT.md fails ---
static void test_parse_missing_file(const std::string& dir) {
	pluto::Agent a;
	CHECK(!pluto::ParseAgent(dir + "/nonexistent/AGENT.md", a));
}

// --- Test: preview_agent returns metadata and body ---
static void test_preview_agent(const std::string& dir) {
	std::string agent_dir = dir + "/preview-agent";
	ensure_dir(agent_dir);

	write_file(agent_dir + "/AGENT.md",
		"---\n"
		"name: preview-test\n"
		"version: 2.0.0\n"
		"author: tester\n"
		"tools: bash, read_file\n"
		"---\n"
		"You are a test agent for previewing.\n"
		"This is line two.\n");

	pluto::Agent a;
	std::string body;
	CHECK(pluto::PreviewAgent(agent_dir, a, body));
	CHECK_EQ(a.name, "preview-test");
	CHECK_EQ(a.version, "2.0.0");
	CHECK_EQ(a.author, "tester");
	CHECK(a.tools.size() == 2);
	CHECK(body.find("test agent for previewing") != std::string::npos);
	CHECK(body.find("line two") != std::string::npos);
}

// --- Test: preview_agent fails for missing dir ---
static void test_preview_missing(const std::string& dir) {
	pluto::Agent a;
	std::string body;
	CHECK(!pluto::PreviewAgent(dir + "/no-such-dir", a, body));
}

// --- Test: search_agents matches by name ---
static void test_search_by_name(const std::string& dir) {
	std::string home = dir + "/home-search1";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string src1 = dir + "/search-a";
	ensure_dir(src1);
	write_file(src1 + "/AGENT.md",
		"---\nname: haiku-dev\ndescription: Haiku development\n"
		"tools: bash, read_attr\n---\nPrompt.\n");

	std::string src2 = dir + "/search-b";
	ensure_dir(src2);
	write_file(src2 + "/AGENT.md",
		"---\nname: code-reviewer\ndescription: Code review\n"
		"tools: grep, glob\n---\nPrompt.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src1, err));
	CHECK(pluto::InstallAgent(src2, err));

	// Helper to check if name is in results.
	auto has_name = [](const std::vector<pluto::Agent>& v, const std::string& n) {
		for (const auto& a : v)
			if (a.name == n) return true;
		return false;
	};

	// Search by name.
	auto r1 = pluto::SearchAgents("haiku-dev");
	CHECK(has_name(r1, "haiku-dev"));

	// Search by tool.
	auto r2 = pluto::SearchAgents("grep");
	CHECK(has_name(r2, "code-reviewer"));

	// Search by description.
	auto r3 = pluto::SearchAgents("Haiku development");
	CHECK(has_name(r3, "haiku-dev"));

	// Search matches nothing.
	auto r4 = pluto::SearchAgents("zzz-nonexistent-xyz-999");
	CHECK(r4.empty());

	// Search matches both installed agents.
	auto r5 = pluto::SearchAgents("reviewer");
	CHECK(has_name(r5, "code-reviewer"));

	pluto::RemoveAgent("haiku-dev", err);
	pluto::RemoveAgent("code-reviewer", err);
}

// --- Test: agent_source_url returns empty for local installs ---
static void test_source_url_local(const std::string& dir) {
	std::string home = dir + "/home-src1";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string src = dir + "/local-agent";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\nname: local-only\nversion: 1.0\n---\nLocal.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src, err));

	// No .agent-source file for local installs.
	CHECK(pluto::AgentSourceUrl("local-only").empty());

	// Cleanup.
	pluto::RemoveAgent("local-only", err);
}

// --- Test: agent_source_url returns URL when written ---
static void test_source_url_written(const std::string& dir) {
	std::string home = dir + "/home-src2";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string src = dir + "/url-agent";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\nname: url-agent\nversion: 1.0\n---\nFrom URL.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src, err));

	// Manually write .agent-source (simulating URL install).
	std::string src_file = pluto::AgentsDir() + "/url-agent/.agent-source";
	write_file(src_file, "https://github.com/test/agent.git\n");

	CHECK_EQ(pluto::AgentSourceUrl("url-agent"),
	          "https://github.com/test/agent.git");

	pluto::RemoveAgent("url-agent", err);
}

// --- Test: update_agent fails for local installs ---
static void test_update_local_fails(const std::string& dir) {
	std::string home = dir + "/home-upd1";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string src = dir + "/upd-local";
	ensure_dir(src);
	write_file(src + "/AGENT.md",
		"---\nname: upd-local\nversion: 1.0\n---\nLocal.\n");

	std::string err;
	CHECK(pluto::InstallAgent(src, err));

	// Update should fail: no source URL.
	CHECK(!pluto::UpdateAgent("upd-local", err));
	CHECK(err.find("locally") != std::string::npos);

	pluto::RemoveAgent("upd-local", err);
}

// --- Test: update_agent fails for nonexistent agent ---
static void test_update_nonexistent(const std::string& dir) {
	std::string home = dir + "/home-upd2";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string err;
	CHECK(!pluto::UpdateAgent("nonexistent-agent", err));
	CHECK(err.find("not installed") != std::string::npos);
}

int main() {
	std::string tmpdir = make_tmpdir();
	std::string original_home = std::getenv("HOME") ? std::getenv("HOME") : "/boot/home";

	test_parse_with_frontmatter(tmpdir);
	test_parse_without_frontmatter(tmpdir);
	test_parse_missing_file(tmpdir);
	test_install_local(tmpdir);
	test_install_duplicate(tmpdir);
	test_list(tmpdir);
	test_remove(tmpdir);
	test_check_deps(tmpdir);
	test_install_with_resources(tmpdir);
	test_preview_agent(tmpdir);
	test_preview_missing(tmpdir);
	test_search_by_name(tmpdir);
	test_source_url_local(tmpdir);
	test_source_url_written(tmpdir);
	test_update_local_fails(tmpdir);
	test_update_nonexistent(tmpdir);

	// Restore HOME.
	setenv("HOME", original_home.c_str(), 1);
	rm_rf(tmpdir);

	if (g_failures > 0) {
		std::fprintf(stderr, "%d test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("test_agent_store: all tests passed\n");
	return 0;
}
