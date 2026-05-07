// tests/test_util.cpp — unit tests for util module

#include "../src/util.h"

#include <cassert>
#include <cstdio>
#include <string>

static int g_pass = 0;
static int g_total = 0;

#define TEST(name) \
	do { ++g_total; std::printf("  %d. %s ... ", g_total, name); } while(0)
#define PASS() do { ++g_pass; std::printf("ok\n"); } while(0)
#define ASSERT(cond) do { \
	if (!(cond)) { \
		std::printf("FAIL (%s:%d: %s)\n", __FILE__, __LINE__, #cond); \
		return; \
	} \
} while(0)

void test_home_dir() {
	TEST("HomeDir not empty");
	std::string h = pluto::HomeDir();
	ASSERT(!h.empty());
	ASSERT(h[0] == '/');
	PASS();
}

void test_settings_dir() {
	TEST("SettingsDir contains pluto");
	std::string s = pluto::SettingsDir();
	ASSERT(s.find("pluto") != std::string::npos);
	ASSERT(s.find(pluto::HomeDir()) == 0);
	PASS();
}

void test_slugify() {
	TEST("Slugify basic");
	ASSERT(pluto::Slugify("Hello World") == "hello-world");
	ASSERT(pluto::Slugify("BQuery Live Queries") == "bquery-live-queries");
	PASS();

	TEST("Slugify special chars");
	ASSERT(pluto::Slugify("foo@bar#baz") == "foobarbaz");
	ASSERT(pluto::Slugify("---") == "entry");
	ASSERT(pluto::Slugify("") == "entry");
	PASS();

	TEST("Slugify no trailing hyphens");
	ASSERT(pluto::Slugify("test - ") == "test");
	PASS();
}

void test_trim() {
	TEST("Trim whitespace");
	ASSERT(pluto::Trim("  hello  ") == "hello");
	ASSERT(pluto::Trim("\t\n foo \n\t") == "foo");
	ASSERT(pluto::Trim("") == "");
	ASSERT(pluto::Trim("   ") == "");
	ASSERT(pluto::Trim("no-trim") == "no-trim");
	PASS();
}

void test_pipe_guard() {
	TEST("PipeGuard runs command");
	pluto::PipeGuard pipe("echo hello", "r");
	ASSERT(pipe);
	char buf[64];
	ASSERT(std::fgets(buf, sizeof(buf), pipe.get()) != nullptr);
	std::string out = buf;
	ASSERT(out.find("hello") != std::string::npos);
	int rc = pipe.close();
	ASSERT(rc == 0);
	PASS();

	TEST("PipeGuard invalid command");
	pluto::PipeGuard bad("/nonexistent/cmd 2>/dev/null", "r");
	// popen may succeed even for bad commands (shell runs)
	// but close() will return non-zero
	if (bad) {
		int rc2 = bad.close();
		ASSERT(rc2 != 0);
	}
	PASS();

	TEST("PipeGuard double close safe");
	pluto::PipeGuard p2("echo test", "r");
	p2.close();
	int rc3 = p2.close();
	ASSERT(rc3 == -1);
	PASS();
}

int main() {
	std::printf("util tests:\n");
	test_home_dir();
	test_settings_dir();
	test_slugify();
	test_trim();
	test_pipe_guard();
	std::printf("\n  %d/%d passed\n", g_pass, g_total);
	return (g_pass == g_total) ? 0 : 1;
}
