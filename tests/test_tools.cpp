// Integration test for the built-in tools (read_file, write_file,
// edit_file, bash) on a private tmpdir.

#include "../src/tools/Builtin.h"
#include "../src/tools/Tool.h"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using pluto::Tool;
using pluto::ToolResult;

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-tools-XXXXXX";
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
	std::ostringstream os;
	os << f.rdbuf();
	return os.str();
}

static std::string json_escape(const std::string& s) {
	std::string out;
	for (char c : s) {
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:   out += c;
		}
	}
	return out;
}

static void test_write_then_read(const std::string& dir) {
	auto wt = pluto::MakeWriteFileTool();
	std::string path = dir + "/hello.txt";
	std::string content = "first line\nsecond line\n";

	std::string in = R"({"path":")" + json_escape(path)
	               + R"(","content":")" + json_escape(content) + R"("})";
	ToolResult r = wt->execute(in);
	CHECK(!r.is_error);

	std::string on_disk = read_file(path);
	CHECK(on_disk == content);

	auto rt = pluto::MakeReadFileTool();
	std::string in2 = R"({"path":")" + json_escape(path) + R"("})";
	ToolResult r2 = rt->execute(in2);
	CHECK(!r2.is_error);
	CHECK(r2.output == content);
}

static void test_edit(const std::string& dir) {
	auto et = pluto::MakeEditFileTool();
	std::string path = dir + "/hello.txt";

	std::string in = R"({"path":")" + json_escape(path)
	               + R"(","old_string":"first line","new_string":"FIRST LINE"})";
	ToolResult r = et->execute(in);
	CHECK(!r.is_error);

	std::string on_disk = read_file(path);
	CHECK(on_disk == "FIRST LINE\nsecond line\n");
}

static void test_edit_unique_required(const std::string& dir) {
	auto wt = pluto::MakeWriteFileTool();
	std::string path = dir + "/dup.txt";
	std::string content = "alpha alpha alpha\n";
	std::string in = R"({"path":")" + json_escape(path)
	               + R"(","content":")" + json_escape(content) + R"("})";
	wt->execute(in);

	auto et = pluto::MakeEditFileTool();
	std::string ein = R"({"path":")" + json_escape(path)
	                + R"(","old_string":"alpha","new_string":"beta"})";
	ToolResult r = et->execute(ein);
	// edit_file should refuse on non-unique match
	CHECK(r.is_error);
}

static void test_bash(const std::string& dir) {
	auto bt = pluto::MakeBashTool();
	std::string in = R"({"command":"echo hello-from-bash && pwd","cwd":")"
	               + json_escape(dir) + R"("})";
	ToolResult r = bt->execute(in);
	CHECK(!r.is_error);
	CHECK(r.output.find("hello-from-bash") != std::string::npos);
}

int main() {
	std::string tmp = make_tmpdir();

	test_write_then_read(tmp);
	test_edit(tmp);
	test_edit_unique_required(tmp);
	test_bash(tmp);

	rm_rf(tmp);

	if (g_failures == 0) {
		std::fprintf(stderr, "test_tools: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_tools: %d failure(s)\n", g_failures);
	return 1;
}
