#include "markdown.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace pluto;

// Helper: strip all ANSI escape sequences for content comparison.
static std::string strip_ansi(const std::string& s) {
	std::string out;
	size_t i = 0;
	while (i < s.size()) {
		if (s[i] == '\x1b') {
			// Skip until 'm' (end of SGR sequence).
			while (i < s.size() && s[i] != 'm') ++i;
			if (i < s.size()) ++i;  // skip the 'm'
		} else {
			out += s[i++];
		}
	}
	return out;
}

// Helper: check that a string contains a specific ANSI code.
static bool has_ansi(const std::string& s, const char* code) {
	return s.find(code) != std::string::npos;
}

int main() {
	// --- Test 1: plain text passes through ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("hello world\n");
		assert(strip_ansi(out) == "hello world\n");
	}

	// --- Test 2: bold ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("this is **bold** text\n");
		assert(strip_ansi(out) == "this is bold text\n");
		assert(has_ansi(out, "\x1b[1m"));   // bold on
		assert(has_ansi(out, "\x1b[22m"));  // bold off
	}

	// --- Test 3: italic ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("this is *italic* text\n");
		assert(strip_ansi(out) == "this is italic text\n");
		assert(has_ansi(out, "\x1b[3m"));   // italic on
		assert(has_ansi(out, "\x1b[23m"));  // italic off
	}

	// --- Test 4: inline code ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("run `make install`\n");
		assert(strip_ansi(out) == "run make install\n");
		assert(has_ansi(out, "\x1b[36m"));  // cyan
	}

	// --- Test 5: header ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("## Section Title\n");
		assert(strip_ansi(out) == "Section Title\n");
		assert(has_ansi(out, "\x1b[1m"));  // bold
		assert(has_ansi(out, "\x1b[4m"));  // underline (level <= 2)
	}

	// --- Test 6: code block ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("```cpp\nint x = 1;\n```\n");
		std::string plain = strip_ansi(out);
		// Should contain the code line indented.
		assert(plain.find("  int x = 1;") != std::string::npos);
	}

	// --- Test 7: horizontal rule ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("---\n");
		std::string plain = strip_ansi(out);
		// Should contain box-drawing characters.
		assert(plain.find("\xe2\x94\x80") != std::string::npos);
	}

	// --- Test 8: unordered list ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("- first item\n- second item\n");
		std::string plain = strip_ansi(out);
		assert(plain.find("\xe2\x80\xa2 first item") != std::string::npos);   // bullet
		assert(plain.find("\xe2\x80\xa2 second item") != std::string::npos);
	}

	// --- Test 9: ordered list ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("1. first\n2. second\n");
		std::string plain = strip_ansi(out);
		assert(plain.find("1. first") != std::string::npos);
		assert(plain.find("2. second") != std::string::npos);
	}

	// --- Test 10: blockquote ---
	{
		TerminalMarkdown md;
		std::string out = md.Feed("> quoted text\n");
		std::string plain = strip_ansi(out);
		assert(plain.find("\xe2\x94\x82 quoted text") != std::string::npos);
	}

	// --- Test 11: streaming (partial lines) ---
	{
		TerminalMarkdown md;
		std::string out1 = md.Feed("hel");
		assert(out1.empty());  // no newline yet, buffered
		std::string out2 = md.Feed("lo\n");
		assert(strip_ansi(out2) == "hello\n");
	}

	// --- Test 12: flush partial line ---
	{
		TerminalMarkdown md;
		md.Feed("partial");
		std::string out = md.Flush();
		assert(strip_ansi(out) == "partial");
	}

	// --- Test 13: reset clears state ---
	{
		TerminalMarkdown md;
		md.Feed("```\ncode line");
		md.Reset();
		std::string out = md.Feed("normal text\n");
		// Should NOT be in code block mode after reset.
		assert(strip_ansi(out) == "normal text\n");
	}

	std::printf("all markdown tests passed\n");
	return 0;
}
