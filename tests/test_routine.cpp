// Tests for routine engine: parse, list, template expansion, enable/disable, log.

#include "../src/routine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

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
	char tmpl[] = "/tmp/pluto-test-routine-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) { std::fprintf(stderr, "mkdtemp failed\n"); std::exit(2); }
	return std::string(d);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	std::system(cmd.c_str());
}

static void write_file(const std::string& path, const std::string& content) {
	std::ofstream f(path);
	f << content;
}

static void test_parse_with_frontmatter(const std::string& dir) {
	std::string path = dir + "/review.md";
	write_file(path,
		"---\n"
		"name: daily-review\n"
		"schedule: manual\n"
		"model: claude-sonnet-4-6\n"
		"tools: read_file, grep\n"
		"notify: true\n"
		"enabled: true\n"
		"---\n"
		"Review all changed files since yesterday.\n"
		"Date: {{date}}\n");

	pluto::Routine r;
	CHECK(pluto::ParseRoutine(path, r));
	CHECK_EQ(r.name, "daily-review");
	CHECK_EQ(r.schedule, "manual");
	CHECK_EQ(r.model, "claude-sonnet-4-6");
	CHECK(r.tools.size() == 2);
	CHECK_EQ(r.tools[0], "read_file");
	CHECK_EQ(r.tools[1], "grep");
	CHECK(r.notify == true);
	CHECK(r.enabled == true);
	CHECK(r.body.find("Review all changed files") != std::string::npos);
	CHECK(r.body.find("{{date}}") != std::string::npos);
}

static void test_parse_without_frontmatter(const std::string& dir) {
	std::string path = dir + "/simple.md";
	write_file(path, "Just a simple prompt with no frontmatter.");

	pluto::Routine r;
	CHECK(pluto::ParseRoutine(path, r));
	CHECK_EQ(r.name, "simple");
	CHECK_EQ(r.schedule, "manual");
	CHECK(r.model.empty());
	CHECK(r.tools.empty());
	CHECK(r.body.find("simple prompt") != std::string::npos);
}

static void test_expand_template() {
	std::string body = "Today is {{date}}. Files: {{changed_files}}";
	std::string expanded = pluto::ExpandTemplate(body, "a.cpp, b.h");
	CHECK(expanded.find("{{date}}") == std::string::npos);
	CHECK(expanded.find("a.cpp, b.h") != std::string::npos);
	CHECK(expanded.find("Today is 202") != std::string::npos);
}

static void test_enable_disable(const std::string& dir) {
	std::string path = dir + "/review.md";
	std::string error;

	CHECK(pluto::SetRoutineEnabled(path, false, error));

	pluto::Routine r;
	CHECK(pluto::ParseRoutine(path, r));
	CHECK(r.enabled == false);

	CHECK(pluto::SetRoutineEnabled(path, true, error));
	CHECK(pluto::ParseRoutine(path, r));
	CHECK(r.enabled == true);
}

static void test_log(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	std::string log_dir = dir + "/config/settings/pluto/routine-logs";
	std::string cmd = "mkdir -p '" + log_dir + "'";
	std::system(cmd.c_str());

	pluto::LogRoutineRun("test-routine", "Output line 1\nOutput line 2");
	pluto::LogRoutineRun("test-routine", "Second run output");

	std::string log = pluto::ReadRoutineLog("test-routine");
	CHECK(log.find("Output line 1") != std::string::npos);
	CHECK(log.find("Second run output") != std::string::npos);
	CHECK(log.find("===") != std::string::npos);
}

static void test_list(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	std::string rdir = dir + "/config/settings/pluto/routines";
	std::string cmd = "mkdir -p '" + rdir + "'";
	std::system(cmd.c_str());

	write_file(rdir + "/alpha.md", "---\nname: alpha\nschedule: manual\n---\nAlpha prompt");
	write_file(rdir + "/beta.md", "---\nname: beta\nschedule: on_boot\n---\nBeta prompt");

	auto routines = pluto::ListRoutines();
	CHECK(routines.size() == 2);
	CHECK_EQ(routines[0].name, "alpha");
	CHECK_EQ(routines[1].name, "beta");
	CHECK_EQ(routines[1].schedule, "on_boot");
}

static std::string read_all(const std::string& path) {
	std::ifstream f(path);
	if (!f) return {};
	std::ostringstream os;
	os << f.rdbuf();
	return os.str();
}

static void test_install_on_boot_job(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	std::string rdir = dir + "/config/settings/pluto/routines";
	std::string cmd = "mkdir -p '" + rdir + "'";
	std::system(cmd.c_str());

	write_file(rdir + "/boot-task.md",
		"---\n"
		"name: boot-task\n"
		"schedule: on_boot\n"
		"model: claude-sonnet-4-6\n"
		"enabled: true\n"
		"---\n"
		"Run a system check on boot.\n");

	pluto::Routine r;
	CHECK(pluto::ParseRoutine(rdir + "/boot-task.md", r));

	std::string error;
	CHECK(pluto::InstallRoutineJob(r, error));

	// Verify job file was created.
	std::string job_path = dir + "/config/settings/launch/pluto-routine-boot-task";
	std::string content = read_all(job_path);
	CHECK(!content.empty());
	CHECK(content.find("x-vnd.pluto-routine-boot-task") != std::string::npos);
	CHECK(content.find("initial_volumes_mounted") != std::string::npos);
	CHECK(content.find("pluto routine run 'boot-task'") != std::string::npos);

	// Uninstall and verify removed.
	CHECK(pluto::UninstallRoutineJob("boot-task", error));
	CHECK(read_all(job_path).empty());
}

static void test_install_manual_fails(const std::string& /*dir*/) {
	pluto::Routine r;
	r.name = "manual-task";
	r.schedule = "manual";
	r.enabled = true;

	std::string error;
	CHECK(!pluto::InstallRoutineJob(r, error));
	CHECK(error.find("manual") != std::string::npos);
}

static void test_install_periodic_job(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);

	pluto::Routine r;
	r.name = "periodic-task";
	r.schedule = "every 30m";
	r.enabled = true;

	std::string error;
	CHECK(pluto::InstallRoutineJob(r, error));

	// Verify script was created.
	std::string script = dir + "/config/settings/pluto/routines/scripts/periodic-task.sh";
	std::string scontent = read_all(script);
	CHECK(!scontent.empty());
	CHECK(scontent.find("sleep 1800") != std::string::npos);
	CHECK(scontent.find("pluto routine run 'periodic-task'") != std::string::npos);

	// Verify job file was created.
	std::string job = dir + "/config/settings/launch/pluto-routine-periodic-task";
	std::string jcontent = read_all(job);
	CHECK(!jcontent.empty());
	CHECK(jcontent.find(script) != std::string::npos);

	CHECK(pluto::UninstallRoutineJob("periodic-task", error));
}

static void test_enable_installs_job(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	std::string rdir = dir + "/config/settings/pluto/routines";
	std::string cmd = "mkdir -p '" + rdir + "'";
	std::system(cmd.c_str());

	write_file(rdir + "/auto.md",
		"---\n"
		"name: auto\n"
		"schedule: on_boot\n"
		"enabled: false\n"
		"---\n"
		"Auto task.\n");

	std::string error;
	// Enable should install job.
	CHECK(pluto::SetRoutineEnabled(rdir + "/auto.md", true, error));
	std::string job = dir + "/config/settings/launch/pluto-routine-auto";
	CHECK(!read_all(job).empty());

	// Disable should remove job.
	CHECK(pluto::SetRoutineEnabled(rdir + "/auto.md", false, error));
	CHECK(read_all(job).empty());
}

int main() {
	std::string tmp = make_tmpdir();
	std::string orig_home = std::getenv("HOME") ? std::getenv("HOME") : "/boot/home";

	test_parse_with_frontmatter(tmp);
	test_parse_without_frontmatter(tmp);
	test_expand_template();
	test_enable_disable(tmp);
	test_log(tmp);
	test_list(tmp);
	test_install_on_boot_job(tmp);
	test_install_manual_fails(tmp);
	test_install_periodic_job(tmp);
	test_enable_installs_job(tmp);

	setenv("HOME", orig_home.c_str(), 1);
	rm_rf(tmp);

	if (g_failures == 0) {
		std::fprintf(stderr, "test_routine: OK (10 tests)\n");
		return 0;
	}
	std::fprintf(stderr, "test_routine: %d failure(s)\n", g_failures);
	return 1;
}
