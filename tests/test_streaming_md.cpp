// tests/test_streaming_md.cpp — StreamingMarkdown unit tests
//
// Tests the line buffering + code block state machine.
// No BTextView needed — pure logic.

#include "gui/StreamingMarkdown.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using pluto::gui::StreamingMarkdown;

static int g_pass = 0;
static int g_total = 0;

#define TEST(name) \
	do { ++g_total; std::printf("  %d. %s ... ", g_total, name); } while(0)
#define PASS() do { ++g_pass; std::printf("ok\n"); } while(0)
#define CHECK(cond) do { \
	if (!(cond)) { \
		std::printf("FAIL (%s:%d: %s)\n", __FILE__, __LINE__, #cond); \
		return; \
	} \
} while(0)

void test_single_line() {
	TEST("single complete line");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };
	sm.feed("hello world\n");
	CHECK(lines.size() == 1);
	CHECK(lines[0] == "hello world");
	CHECK(sm.line_buffer().empty());
	PASS();
}

void test_partial_then_complete() {
	TEST("partial chunk then completion");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };

	sm.feed("hel");
	CHECK(lines.empty());
	CHECK(sm.line_buffer() == "hel");

	sm.feed("lo world\n");
	CHECK(lines.size() == 1);
	CHECK(lines[0] == "hello world");
	CHECK(sm.line_buffer().empty());
	PASS();
}

void test_multiple_lines_one_chunk() {
	TEST("multiple lines in one chunk");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };

	sm.feed("line1\nline2\nline3\n");
	CHECK(lines.size() == 3);
	CHECK(lines[0] == "line1");
	CHECK(lines[1] == "line2");
	CHECK(lines[2] == "line3");
	PASS();
}

void test_crlf_stripped() {
	TEST("CRLF line endings stripped");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };

	sm.feed("windows\r\nlines\r\n");
	CHECK(lines.size() == 2);
	CHECK(lines[0] == "windows");
	CHECK(lines[1] == "lines");
	PASS();
}

void test_code_block_basic() {
	TEST("fenced code block emitted on close");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	std::string code_out;
	std::string lang_out;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };
	sm.on_code_block = [&](const std::string& c, const std::string& l) {
		code_out = c; lang_out = l;
	};

	sm.feed("before\n```cpp\nint x = 1;\nreturn x;\n```\nafter\n");
	CHECK(lines.size() == 2);
	CHECK(lines[0] == "before");
	CHECK(lines[1] == "after");
	CHECK(lang_out == "cpp");
	CHECK(code_out == "  int x = 1;\n  return x;\n");
	CHECK(!sm.in_code_block());
	PASS();
}

void test_code_block_no_lang() {
	TEST("code block without language hint");
	StreamingMarkdown sm;
	std::string code_out;
	std::string lang_out = "sentinel";
	sm.on_line = [](const std::string&) {};
	sm.on_code_block = [&](const std::string& c, const std::string& l) {
		code_out = c; lang_out = l;
	};

	sm.feed("```\nhello\n```\n");
	CHECK(lang_out.empty());
	CHECK(code_out == "  hello\n");
	PASS();
}

void test_code_block_streaming() {
	TEST("code block across multiple chunks");
	StreamingMarkdown sm;
	std::string code_out;
	std::string lang_out;
	sm.on_line = [](const std::string&) {};
	sm.on_code_block = [&](const std::string& c, const std::string& l) {
		code_out = c; lang_out = l;
	};

	sm.feed("```py");
	CHECK(sm.line_buffer() == "```py");
	CHECK(!sm.in_code_block());

	sm.feed("thon\n");
	CHECK(sm.in_code_block());
	CHECK(sm.code_lang() == "python");

	sm.feed("x = 1\n");
	sm.feed("y = 2\n");
	CHECK(sm.code_accum() == "  x = 1\n  y = 2\n");

	sm.feed("```\n");
	CHECK(!sm.in_code_block());
	CHECK(code_out == "  x = 1\n  y = 2\n");
	CHECK(lang_out == "python");
	PASS();
}

void test_flush_partial_line() {
	TEST("flush emits partial line");
	StreamingMarkdown sm;
	std::string partial;
	sm.on_line = [](const std::string&) {};
	sm.on_partial = [&](const std::string& p) { partial = p; };

	sm.feed("no newline at end");
	CHECK(sm.line_buffer() == "no newline at end");
	sm.flush();
	CHECK(partial == "no newline at end");
	CHECK(sm.line_buffer().empty());
	PASS();
}

void test_flush_unclosed_code_block() {
	TEST("flush emits unclosed code block");
	StreamingMarkdown sm;
	std::string code_out;
	sm.on_line = [](const std::string&) {};
	sm.on_code_block = [&](const std::string& c, const std::string&) {
		code_out = c;
	};

	sm.feed("```\nline1\nline2\n");
	CHECK(sm.in_code_block());
	sm.flush();
	CHECK(!sm.in_code_block());
	CHECK(code_out == "  line1\n  line2\n");
	PASS();
}

void test_flush_unclosed_with_partial() {
	TEST("flush code block with partial last line");
	StreamingMarkdown sm;
	std::string code_out;
	sm.on_line = [](const std::string&) {};
	sm.on_code_block = [&](const std::string& c, const std::string&) {
		code_out = c;
	};

	sm.feed("```\nfull line\npartial");
	CHECK(sm.in_code_block());
	sm.flush();
	CHECK(!sm.in_code_block());
	CHECK(code_out == "  full line\n  partial");
	PASS();
}

void test_reset() {
	TEST("reset clears all state");
	StreamingMarkdown sm;
	sm.on_line = [](const std::string&) {};
	sm.feed("```cpp\nsome code\npartial");
	CHECK(sm.in_code_block());
	CHECK(!sm.line_buffer().empty());
	CHECK(!sm.code_accum().empty());

	sm.reset();
	CHECK(!sm.in_code_block());
	CHECK(sm.line_buffer().empty());
	CHECK(sm.code_lang().empty());
	CHECK(sm.code_accum().empty());
	PASS();
}

void test_empty_lines() {
	TEST("empty lines emitted correctly");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };

	sm.feed("\n\ntext\n\n");
	CHECK(lines.size() == 4);
	CHECK(lines[0].empty());
	CHECK(lines[1].empty());
	CHECK(lines[2] == "text");
	CHECK(lines[3].empty());
	PASS();
}

void test_nested_backticks_in_line() {
	TEST("inline backticks don't trigger code block");
	StreamingMarkdown sm;
	std::vector<std::string> lines;
	sm.on_line = [&](const std::string& l) { lines.push_back(l); };

	sm.feed("use `code` here\n");
	sm.feed("and ``double`` too\n");
	CHECK(lines.size() == 2);
	CHECK(lines[0] == "use `code` here");
	CHECK(lines[1] == "and ``double`` too");
	CHECK(!sm.in_code_block());
	PASS();
}

int main() {
	std::printf("test_streaming_md\n");
	test_single_line();
	test_partial_then_complete();
	test_multiple_lines_one_chunk();
	test_crlf_stripped();
	test_code_block_basic();
	test_code_block_no_lang();
	test_code_block_streaming();
	test_flush_partial_line();
	test_flush_unclosed_code_block();
	test_flush_unclosed_with_partial();
	test_reset();
	test_empty_lines();
	test_nested_backticks_in_line();
	std::printf("%d/%d passed\n", g_pass, g_total);

	return g_pass == g_total ? 0 : 1;
}
