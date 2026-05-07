// tests/test_history.cpp — libedit history integration tests
//
// Tests that the editline history subsystem can:
// 1. Initialize and finalize without crashing
// 2. Add entries and retrieve them
// 3. Save to and load from a file
// 4. Respect the max size limit
// 5. Handle empty/missing history files gracefully

#include <histedit.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

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

static std::string tmp_path() {
	char tmpl[] = "/tmp/pluto-test-hist-XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd >= 0) close(fd);
	unlink(tmpl);  // we want the path, not the file
	return tmpl;
}

// 1. Init and finalize
void test_init_finalize() {
	TEST("init and finalize");
	HistEvent hev;
	History* h = history_init();
	ASSERT(h != nullptr);
	history(h, &hev, H_SETSIZE, 100);
	history_end(h);
	PASS();
}

// 2. Add entries and walk them
void test_add_and_walk() {
	TEST("add entries and walk");
	HistEvent hev;
	History* h = history_init();
	ASSERT(h != nullptr);
	history(h, &hev, H_SETSIZE, 100);

	history(h, &hev, H_ENTER, "first command");
	history(h, &hev, H_ENTER, "second command");
	history(h, &hev, H_ENTER, "third command");

	// Walk from newest to oldest.
	int rc = history(h, &hev, H_FIRST);
	ASSERT(rc >= 0);
	// H_FIRST returns the most recent entry.
	std::string newest(hev.str);
	// Trim trailing newline if present.
	while (!newest.empty() && (newest.back() == '\n' || newest.back() == '\r'))
		newest.pop_back();
	ASSERT(newest == "third command");

	rc = history(h, &hev, H_NEXT);
	ASSERT(rc >= 0);
	std::string mid(hev.str);
	while (!mid.empty() && (mid.back() == '\n' || mid.back() == '\r'))
		mid.pop_back();
	ASSERT(mid == "second command");

	rc = history(h, &hev, H_NEXT);
	ASSERT(rc >= 0);
	std::string oldest(hev.str);
	while (!oldest.empty() && (oldest.back() == '\n' || oldest.back() == '\r'))
		oldest.pop_back();
	ASSERT(oldest == "first command");

	history_end(h);
	PASS();
}

// 3. Save and load roundtrip
void test_save_load() {
	TEST("save and load roundtrip");
	std::string path = tmp_path();
	HistEvent hev;

	// Save
	{
		History* h = history_init();
		ASSERT(h != nullptr);
		history(h, &hev, H_SETSIZE, 100);
		history(h, &hev, H_ENTER, "alpha");
		history(h, &hev, H_ENTER, "beta");
		history(h, &hev, H_ENTER, "gamma");
		int rc = history(h, &hev, H_SAVE, path.c_str());
		ASSERT(rc >= 0);
		history_end(h);
	}

	// Load into a fresh history
	{
		History* h = history_init();
		ASSERT(h != nullptr);
		history(h, &hev, H_SETSIZE, 100);
		int rc = history(h, &hev, H_LOAD, path.c_str());
		ASSERT(rc >= 0);

		// Verify: newest should be "gamma"
		rc = history(h, &hev, H_FIRST);
		ASSERT(rc >= 0);
		std::string s(hev.str);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		ASSERT(s == "gamma");

		history_end(h);
	}

	unlink(path.c_str());
	PASS();
}

// 4. Size limit respected
void test_size_limit() {
	TEST("size limit respected");
	HistEvent hev;
	History* h = history_init();
	ASSERT(h != nullptr);
	history(h, &hev, H_SETSIZE, 3);

	history(h, &hev, H_ENTER, "one");
	history(h, &hev, H_ENTER, "two");
	history(h, &hev, H_ENTER, "three");
	history(h, &hev, H_ENTER, "four");  // should evict "one"

	// Count entries
	int count = 0;
	for (int rc = history(h, &hev, H_FIRST); rc >= 0;
	     rc = history(h, &hev, H_NEXT))
		++count;
	ASSERT(count == 3);

	// Oldest should be "two" (not "one")
	history(h, &hev, H_LAST);
	std::string oldest(hev.str);
	while (!oldest.empty() && (oldest.back() == '\n' || oldest.back() == '\r'))
		oldest.pop_back();
	ASSERT(oldest == "two");

	history_end(h);
	PASS();
}

// 5. Load from nonexistent file: should not crash
void test_load_missing() {
	TEST("load from nonexistent file");
	HistEvent hev;
	History* h = history_init();
	ASSERT(h != nullptr);
	history(h, &hev, H_SETSIZE, 100);
	// Loading a missing file should fail gracefully (rc < 0), not crash.
	history(h, &hev, H_LOAD, "/tmp/pluto-no-such-history-file-xyz");
	history_end(h);
	PASS();
}

// 6. EditLine init/end smoke test
void test_editline_init() {
	TEST("EditLine init and end");
	// We can't fully test interactive line editing, but we can verify
	// that init/end don't crash.
	EditLine* el = el_init("pluto-test", stdin, stdout, stderr);
	ASSERT(el != nullptr);
	el_set(el, EL_EDITOR, "emacs");
	el_end(el);
	PASS();
}

// 7. History with special characters
void test_special_chars() {
	TEST("history with special characters");
	std::string path = tmp_path();
	HistEvent hev;

	{
		History* h = history_init();
		ASSERT(h != nullptr);
		history(h, &hev, H_SETSIZE, 100);
		history(h, &hev, H_ENTER, "echo 'hello world'");
		history(h, &hev, H_ENTER, "grep -rn \"pattern\" src/");
		history(h, &hev, H_ENTER, "@/boot/home/file.cpp");
		int rc = history(h, &hev, H_SAVE, path.c_str());
		ASSERT(rc >= 0);
		history_end(h);
	}

	{
		History* h = history_init();
		ASSERT(h != nullptr);
		history(h, &hev, H_SETSIZE, 100);
		int rc = history(h, &hev, H_LOAD, path.c_str());
		ASSERT(rc >= 0);

		rc = history(h, &hev, H_FIRST);
		ASSERT(rc >= 0);
		std::string s(hev.str);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		ASSERT(s == "@/boot/home/file.cpp");

		history_end(h);
	}

	unlink(path.c_str());
	PASS();
}

int main() {
	std::printf("test_history\n");
	test_init_finalize();
	test_add_and_walk();
	test_save_load();
	test_size_limit();
	test_load_missing();
	test_editline_init();
	test_special_chars();
	std::printf("%d/%d passed\n", g_pass, g_total);
	return g_pass == g_total ? 0 : 1;
}
