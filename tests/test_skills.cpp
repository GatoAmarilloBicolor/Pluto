// Tests for skill loading: always-on and trigger-based.

#include "../src/config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
	char tmpl[] = "/tmp/pluto-test-skills-XXXXXX";
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

// --- Test: always-on skill (no trigger) ---
static void test_always_on_skill(const std::string& dir) {
	std::string home = dir + "/home1";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string skill_dir = home + "/config/settings/pluto/skills/my-skill";
	ensure_dir(skill_dir);
	write_file(skill_dir + "/SKILL.md",
		"You are an always-on skill.\n");

	pluto::Config cfg;
	pluto::ApplySkills(cfg);

	// Should be in system prompt.
	CHECK(cfg.system_prompt.find("<skill name=\"my-skill\">") != std::string::npos);
	CHECK(cfg.system_prompt.find("always-on skill") != std::string::npos);
	// No triggered skills.
	CHECK(cfg.triggered_skills.empty());
}

// --- Test: triggered skill (with trigger frontmatter) ---
static void test_triggered_skill(const std::string& dir) {
	std::string home = dir + "/home2";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string skill_dir = home + "/config/settings/pluto/skills/docker-skill";
	ensure_dir(skill_dir);
	write_file(skill_dir + "/SKILL.md",
		"---\n"
		"trigger: docker, container, dockerfile\n"
		"---\n"
		"You are a Docker expert.\n");

	pluto::Config cfg;
	pluto::ApplySkills(cfg);

	// Should NOT be in system prompt yet.
	CHECK(cfg.system_prompt.find("<skill name=\"docker-skill\">") == std::string::npos);
	// Should be in available_skills catalog.
	CHECK(cfg.system_prompt.find("<available_skills>") != std::string::npos);
	CHECK(cfg.system_prompt.find("docker-skill") != std::string::npos);
	// Should be stored as triggered skill.
	CHECK(cfg.triggered_skills.size() == 1);
	CHECK_EQ(cfg.triggered_skills[0].name, "docker-skill");
	CHECK(cfg.triggered_skills[0].triggers.size() == 3);
	CHECK_EQ(cfg.triggered_skills[0].triggers[0], "docker");
	CHECK_EQ(cfg.triggered_skills[0].triggers[1], "container");
	CHECK_EQ(cfg.triggered_skills[0].triggers[2], "dockerfile");
}

// --- Test: inject triggered skill on matching message ---
static void test_inject_on_match(const std::string& dir) {
	std::string home = dir + "/home3";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string skill_dir = home + "/config/settings/pluto/skills/git-skill";
	ensure_dir(skill_dir);
	write_file(skill_dir + "/SKILL.md",
		"---\n"
		"trigger: git, rebase, merge\n"
		"---\n"
		"You are a Git expert.\n");

	pluto::Config cfg;
	pluto::ApplySkills(cfg);
	CHECK(cfg.triggered_skills.size() == 1);

	// No match.
	auto activated = pluto::InjectTriggeredSkills(cfg, "help me write code");
	CHECK(activated.empty());
	CHECK(cfg.triggered_skills.size() == 1);

	// Match on "rebase" (case insensitive).
	activated = pluto::InjectTriggeredSkills(cfg, "How do I rebase my branch?");
	CHECK(activated.size() == 1);
	CHECK_EQ(activated[0], "git-skill");
	// Now injected into system prompt.
	CHECK(cfg.system_prompt.find("<skill name=\"git-skill\">") != std::string::npos);
	CHECK(cfg.system_prompt.find("Git expert") != std::string::npos);
	// Removed from triggered_skills.
	CHECK(cfg.triggered_skills.empty());
}

// --- Test: mixed always-on and triggered skills ---
static void test_mixed_skills(const std::string& dir) {
	std::string home = dir + "/home4";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string s1 = home + "/config/settings/pluto/skills/always-skill";
	ensure_dir(s1);
	write_file(s1 + "/SKILL.md", "Always active.\n");

	std::string s2 = home + "/config/settings/pluto/skills/lazy-skill";
	ensure_dir(s2);
	write_file(s2 + "/SKILL.md",
		"---\ntrigger: special-keyword\n---\nLazy loaded.\n");

	pluto::Config cfg;
	pluto::ApplySkills(cfg);

	// Always skill is in prompt.
	CHECK(cfg.system_prompt.find("<skill name=\"always-skill\">") != std::string::npos);
	// Lazy skill is in available catalog but not in active skills.
	CHECK(cfg.system_prompt.find("<skill name=\"lazy-skill\">") == std::string::npos);
	CHECK(cfg.system_prompt.find("lazy-skill") != std::string::npos);  // in catalog
	CHECK(cfg.triggered_skills.size() == 1);
}

// --- Test: case-insensitive trigger matching ---
static void test_case_insensitive_trigger(const std::string& dir) {
	std::string home = dir + "/home5";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	std::string s = home + "/config/settings/pluto/skills/case-skill";
	ensure_dir(s);
	write_file(s + "/SKILL.md",
		"---\ntrigger: Docker\n---\nDocker skill.\n");

	pluto::Config cfg;
	pluto::ApplySkills(cfg);
	CHECK(cfg.triggered_skills.size() == 1);

	auto activated = pluto::InjectTriggeredSkills(cfg, "How do I use DOCKER?");
	CHECK(activated.size() == 1);
	CHECK_EQ(activated[0], "case-skill");
}

// --- Test: no skills directory ---
static void test_no_skills_dir(const std::string& dir) {
	std::string home = dir + "/home6";
	ensure_dir(home);
	setenv("HOME", home.c_str(), 1);

	pluto::Config cfg;
	pluto::ApplySkills(cfg);
	CHECK(cfg.system_prompt.find("<skills>") == std::string::npos);
	CHECK(cfg.triggered_skills.empty());
}

int main() {
	std::string tmpdir = make_tmpdir();
	std::string original_home = std::getenv("HOME") ? std::getenv("HOME") : "/boot/home";

	test_always_on_skill(tmpdir);
	test_triggered_skill(tmpdir);
	test_inject_on_match(tmpdir);
	test_mixed_skills(tmpdir);
	test_case_insensitive_trigger(tmpdir);
	test_no_skills_dir(tmpdir);

	setenv("HOME", original_home.c_str(), 1);
	rm_rf(tmpdir);

	if (g_failures > 0) {
		std::fprintf(stderr, "%d test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("test_skills: OK (6 tests)\n");
	return 0;
}
