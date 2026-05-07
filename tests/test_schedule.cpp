// Tests for pluto schedule: save, load, list, remove, launch_daemon
// job generation.

#include "../src/schedule.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using pluto::Schedule;

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
	char tmpl[] = "/tmp/pluto-test-sched-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) { std::fprintf(stderr, "mkdtemp failed\n"); std::exit(2); }
	return std::string(d);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	std::system(cmd.c_str());
}

static std::string read_file(const std::string& path) {
	std::ifstream f(path);
	if (!f) return {};
	std::ostringstream os;
	os << f.rdbuf();
	return os.str();
}

static void test_save_and_load(const std::string& dir) {
	// Override HOME so schedules go into our tmpdir.
	std::string sched_dir = dir + "/config/settings/pluto/schedules";
	std::string cmd = "mkdir -p '" + sched_dir + "'";
	std::system(cmd.c_str());

	setenv("HOME", dir.c_str(), 1);

	Schedule s;
	s.name = "daily-review";
	s.prompt_file = "/boot/home/prompts/review.md";
	s.event = "boot";
	s.interval_mins = 60;
	s.model = "claude-sonnet-4-6";
	s.enabled = true;

	std::string error;
	CHECK(pluto::SaveSchedule(s, error));

	// Verify the file exists and is readable.
	std::string conf = pluto::SchedulesDir() + "/daily-review.conf";
	std::string content = read_file(conf);
	CHECK(content.find("prompt_file = /boot/home/prompts/review.md") != std::string::npos);
	CHECK(content.find("event = boot") != std::string::npos);
	CHECK(content.find("interval = 60") != std::string::npos);
	CHECK(content.find("model = claude-sonnet-4-6") != std::string::npos);
	CHECK(content.find("enabled = true") != std::string::npos);

	// Load it back.
	Schedule loaded;
	CHECK(pluto::LoadSchedule("daily-review", loaded));
	CHECK_EQ(loaded.name, "daily-review");
	CHECK_EQ(loaded.prompt_file, "/boot/home/prompts/review.md");
	CHECK_EQ(loaded.event, "boot");
	CHECK(loaded.interval_mins == 60);
	CHECK_EQ(loaded.model, "claude-sonnet-4-6");
	CHECK(loaded.enabled == true);
}

static void test_list(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);

	// Save a second schedule.
	Schedule s2;
	s2.name = "code-lint";
	s2.prompt_file = "/boot/home/prompts/lint.md";
	s2.enabled = false;
	std::string error;
	CHECK(pluto::SaveSchedule(s2, error));

	auto scheds = pluto::ListSchedules();
	CHECK(scheds.size() == 2);
	// Sorted alphabetically.
	CHECK_EQ(scheds[0].name, "code-lint");
	CHECK_EQ(scheds[1].name, "daily-review");
}

static void test_remove(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);

	std::string error;
	CHECK(pluto::RemoveSchedule("code-lint", error));

	Schedule s;
	CHECK(!pluto::LoadSchedule("code-lint", s));

	auto scheds = pluto::ListSchedules();
	CHECK(scheds.size() == 1);
	CHECK_EQ(scheds[0].name, "daily-review");
}

static void test_launch_job(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);

	Schedule s;
	CHECK(pluto::LoadSchedule("daily-review", s));

	std::string error;
	CHECK(pluto::InstallLaunchJob(s, error));

	// The script should exist because interval > 0.
	std::string script_path = pluto::SchedulesDir() + "/scripts/daily-review.sh";
	std::string script = read_file(script_path);
	CHECK(!script.empty());
	CHECK(script.find("while true; do") != std::string::npos);
	CHECK(script.find("agent run") != std::string::npos);
	CHECK(script.find("sleep 3600") != std::string::npos);

	// The launch_daemon job should exist.
	std::string job_path = dir + "/config/settings/launch/pluto-daily-review";
	std::string job = read_file(job_path);
	CHECK(!job.empty());
	CHECK(job.find("x-vnd.pluto-schedule-daily-review") != std::string::npos);
	CHECK(job.find("on initial_volumes_mounted") != std::string::npos);
}

static void test_not_found() {
	Schedule s;
	CHECK(!pluto::LoadSchedule("nonexistent-12345", s));
}

int main() {
	std::string tmp = make_tmpdir();
	std::string orig_home = std::getenv("HOME") ? std::getenv("HOME") : "/boot/home";

	test_save_and_load(tmp);
	test_list(tmp);
	test_remove(tmp);
	test_launch_job(tmp);
	test_not_found();

	// Restore HOME.
	setenv("HOME", orig_home.c_str(), 1);
	rm_rf(tmp);

	if (g_failures == 0) {
		std::fprintf(stderr, "test_schedule: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_schedule: %d failure(s)\n", g_failures);
	return 1;
}
