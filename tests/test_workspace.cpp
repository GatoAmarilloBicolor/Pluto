// Tests for workspace scoping, trust management, and access logging.
// Uses SetTrustedFileOverride / SetLogFileOverride so it doesn't
// touch the user's real settings.

#include "../src/workspace.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-ws-XXXXXX";
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

// ---- tests ----

static void test_WorkspaceAllows_inside() {
	std::printf("  WorkspaceAllows inside... ");

	// WorkspaceInit() has been called in main(), so WorkspaceRoot()
	// is the cwd of the test runner. Create a file in the workspace.
	std::string ws = pluto::WorkspaceRoot();
	CHECK(!ws.empty());

	// A path inside the workspace should be allowed.
	CHECK(pluto::WorkspaceAllows(ws + "/foo/bar.txt"));
	CHECK(pluto::WorkspaceAllows(ws));

	std::printf("ok\n");
}

static void test_WorkspaceAllows_outside() {
	std::printf("  WorkspaceAllows outside... ");

	std::string ws = pluto::WorkspaceRoot();
	CHECK(!ws.empty());

	// A path outside the workspace should not be allowed.
	CHECK(!pluto::WorkspaceAllows("/tmp/some-other-place/file.txt"));

	std::printf("ok\n");
}

static void test_trust_persistence() {
	std::printf("  trust persistence... ");

	std::string tmp = make_tmpdir();
	std::string trusted_file = tmp + "/trusted_workspaces";
	std::string other_dir = tmp + "/other_project";
	::mkdir(other_dir.c_str(), 0755);

	pluto::SetTrustedFileOverride(trusted_file);

	// Initially no trusted workspaces.
	auto list = pluto::ListTrustedWorkspaces();
	CHECK(list.empty());

	// Trust a directory.
	pluto::TrustWorkspace(other_dir);
	list = pluto::ListTrustedWorkspaces();
	CHECK(list.size() == 1);

	// Check IsWorkspaceTrusted.
	CHECK(pluto::IsWorkspaceTrusted(other_dir));

	// WorkspaceAllows should now pass for the trusted dir.
	CHECK(pluto::WorkspaceAllows(other_dir + "/subdir/file.cpp"));

	// Untrust.
	pluto::UntrustWorkspace(other_dir);
	list = pluto::ListTrustedWorkspaces();
	CHECK(list.empty());
	CHECK(!pluto::IsWorkspaceTrusted(other_dir));

	// After untrust, WorkspaceAllows should deny again.
	CHECK(!pluto::WorkspaceAllows(other_dir + "/subdir/file.cpp"));

	// Verify the file on disk is correct.
	std::string contents = read_file(trusted_file);
	CHECK(contents.empty());

	pluto::SetTrustedFileOverride("");
	rm_rf(tmp);
	std::printf("ok\n");
}

static void test_trust_no_duplicates() {
	std::printf("  trust no duplicates... ");

	std::string tmp = make_tmpdir();
	std::string trusted_file = tmp + "/trusted_workspaces";
	std::string dir = tmp + "/proj";
	::mkdir(dir.c_str(), 0755);

	pluto::SetTrustedFileOverride(trusted_file);

	pluto::TrustWorkspace(dir);
	pluto::TrustWorkspace(dir);
	pluto::TrustWorkspace(dir);
	auto list = pluto::ListTrustedWorkspaces();
	CHECK(list.size() == 1);

	pluto::SetTrustedFileOverride("");
	rm_rf(tmp);
	std::printf("ok\n");
}

static void test_LogAccess() {
	std::printf("  LogAccess... ");

	std::string tmp = make_tmpdir();
	std::string log_file = tmp + "/access.log";

	pluto::SetLogFileOverride(log_file);

	pluto::LogAccess("/foo/bar.txt", "read_file", true);
	pluto::LogAccess("/baz/qux.txt", "write_file", false);

	std::string contents = read_file(log_file);
	CHECK(contents.find("ALLOW") != std::string::npos);
	CHECK(contents.find("DENY") != std::string::npos);
	CHECK(contents.find("read_file") != std::string::npos);
	CHECK(contents.find("write_file") != std::string::npos);
	CHECK(contents.find("/foo/bar.txt") != std::string::npos);
	CHECK(contents.find("/baz/qux.txt") != std::string::npos);

	pluto::SetLogFileOverride("");
	rm_rf(tmp);
	std::printf("ok\n");
}

static void test_path_traversal_blocked() {
	std::printf("  path traversal blocked... ");

	std::string ws = pluto::WorkspaceRoot();
	CHECK(!ws.empty());

	// Ensure trusted file override is empty so no extra trust.
	std::string tmp = make_tmpdir();
	pluto::SetTrustedFileOverride(tmp + "/empty_trusted");

	// A path using .. to escape the workspace must be denied.
	// e.g. /boot/home/Desktop/pluto/../../../etc/shadow
	CHECK(!pluto::WorkspaceAllows(ws + "/../../../etc/shadow"));
	CHECK(!pluto::WorkspaceAllows(ws + "/.."));
	CHECK(!pluto::WorkspaceAllows(ws + "/../../../etc/passwd"));
	CHECK(!pluto::WorkspaceAllows(ws + "/subdir/../../.."));

	// But a normal path inside the workspace should still work.
	CHECK(pluto::WorkspaceAllows(ws + "/src/main.cpp"));
	CHECK(pluto::WorkspaceAllows(ws + "/./src/../src/main.cpp"));

	pluto::SetTrustedFileOverride("");
	rm_rf(tmp);
	std::printf("ok\n");
}

static void test_session_trust() {
	std::printf("  session trust... ");

	// Initially not session-trusted (depends on global state,
	// but for a fresh test process it should be false).
	// After TrustSession(), paths outside workspace should be allowed.
	std::string ws = pluto::WorkspaceRoot();
	CHECK(!ws.empty());

	// Before session trust, outside path is denied.
	// (We need to make sure the trusted file override is set to a
	//  non-existent file so we don't pick up real trusted workspaces.)
	std::string tmp = make_tmpdir();
	pluto::SetTrustedFileOverride(tmp + "/empty_trusted");

	CHECK(!pluto::WorkspaceAllows("/tmp/outside-path-test"));

	pluto::TrustSession();
	CHECK(pluto::IsSessionTrusted());
	CHECK(pluto::WorkspaceAllows("/tmp/outside-path-test"));

	pluto::SetTrustedFileOverride("");
	rm_rf(tmp);
	std::printf("ok\n");
}

int main() {
	std::printf("test_workspace\n");

	// Initialize workspace to cwd of test runner.
	pluto::WorkspaceInit();

	test_WorkspaceAllows_inside();
	test_WorkspaceAllows_outside();
	test_trust_persistence();
	test_trust_no_duplicates();
	test_LogAccess();
	test_path_traversal_blocked();
	test_session_trust();

	if (g_failures > 0) {
		std::fprintf(stderr, "%d test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all tests passed\n");
	return 0;
}
