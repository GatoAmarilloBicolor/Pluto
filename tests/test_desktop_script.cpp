// Test for desktop_script tool — exercises BMessage scripting against
// the Tracker (always running on Haiku) and validates response parsing.
//
// IMPORTANT: this test must run on a live Haiku desktop with Tracker active.

#include "../src/haiku/DesktopTools.h"
#include "../src/json.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

static pluto::ToolResult run(pluto::Tool& t, const std::string& json) {
	return t.execute(json);
}

static void test_suites() {
	auto tool = pluto::MakeDesktopScriptTool();

	// Get suites from Tracker — should list available scripting suites
	auto r = run(*tool, R"({"app":"application/x-vnd.Be-TRAK","action":"suites"})");
	CHECK(!r.is_error);
	CHECK(!r.output.empty());
	// Tracker's suites should mention "suite/vnd.Be-handler" or similar
	CHECK(r.output.find("suite") != std::string::npos
	   || r.output.find("Suites") != std::string::npos
	   || r.output.find("suites") != std::string::npos);
	std::fprintf(stderr, "  suites ok (%zu bytes)\n", r.output.size());
}

static void test_count_windows() {
	auto tool = pluto::MakeDesktopScriptTool();

	// Count Tracker windows — should be >= 1 (Desktop counts)
	auto r = run(*tool, R"({"app":"application/x-vnd.Be-TRAK","action":"count","property":"Window"})");
	CHECK(!r.is_error);
	CHECK(!r.output.empty());
	// Output should contain "result = <number>"
	CHECK(r.output.find("result") != std::string::npos);
	std::fprintf(stderr, "  count windows ok: %s", r.output.c_str());
}

static void test_get_window_title() {
	auto tool = pluto::MakeDesktopScriptTool();

	// Get title of first Tracker window (index 0)
	auto r = run(*tool, R"({"app":"application/x-vnd.Be-TRAK","action":"get","property":"Title","index":0})");
	// Might succeed or fail depending on window existence; just check no crash
	if (!r.is_error) {
		CHECK(!r.output.empty());
		std::fprintf(stderr, "  get title ok: %s", r.output.c_str());
	} else {
		std::fprintf(stderr, "  get title (expected err): %s\n", r.output.c_str());
	}
}

static void test_bad_app() {
	auto tool = pluto::MakeDesktopScriptTool();

	auto r = run(*tool, R"({"app":"application/x-vnd.nonexistent","action":"suites"})");
	CHECK(r.is_error);
	CHECK(r.output.find("not running") != std::string::npos);
	std::fprintf(stderr, "  bad app ok\n");
}

static void test_bad_action() {
	auto tool = pluto::MakeDesktopScriptTool();

	auto r = run(*tool, R"({"app":"application/x-vnd.Be-TRAK","action":"bogus"})");
	CHECK(r.is_error);
	CHECK(r.output.find("unknown action") != std::string::npos);
	std::fprintf(stderr, "  bad action ok\n");
}

static void test_missing_property() {
	auto tool = pluto::MakeDesktopScriptTool();

	auto r = run(*tool, R"({"app":"application/x-vnd.Be-TRAK","action":"get"})");
	CHECK(r.is_error);
	CHECK(r.output.find("property required") != std::string::npos);
	std::fprintf(stderr, "  missing property ok\n");
}

int main() {
	std::fprintf(stderr, "test_desktop_script\n");
	test_suites();
	test_count_windows();
	test_get_window_title();
	test_bad_app();
	test_bad_action();
	test_missing_property();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_desktop_script: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_desktop_script: %d failure(s)\n", g_failures);
	return 1;
}
