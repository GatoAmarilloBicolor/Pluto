// Tests for syntax highlighting lexer.

#include "../src/gui/SyntaxHighlight.h"

#include <cstdio>
#include <string>

using pluto::gui::TokenType;
using pluto::gui::Token;
using pluto::gui::highlight;

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

static bool has_token(const std::vector<Token>& tokens, TokenType type,
                      const std::string& text) {
	for (const auto& t : tokens)
		if (t.type == type && t.text == text) return true;
	return false;
}

// Reconstruct source from tokens — must match original.
static std::string reconstruct(const std::vector<Token>& tokens) {
	std::string out;
	for (const auto& t : tokens) out += t.text;
	return out;
}

static void test_unknown_lang() {
	auto tokens = highlight("hello world", "unknown");
	CHECK(tokens.size() == 1);
	CHECK(tokens[0].type == TokenType::Default);
	CHECK(tokens[0].text == "hello world");
}

static void test_cpp_keywords() {
	auto tokens = highlight("  if (x) return 42;\n", "cpp");
	CHECK(has_token(tokens, TokenType::Keyword, "if"));
	CHECK(has_token(tokens, TokenType::Keyword, "return"));
	CHECK(has_token(tokens, TokenType::Number, "42"));
}

static void test_cpp_strings() {
	auto tokens = highlight("  std::string s = \"hello world\";\n", "cpp");
	CHECK(has_token(tokens, TokenType::String, "\"hello world\""));
	CHECK(has_token(tokens, TokenType::Type, "std::string"));
}

static void test_cpp_comments() {
	auto tokens = highlight("  // this is a comment\n  int x;\n", "cpp");
	CHECK(has_token(tokens, TokenType::Comment, "// this is a comment\n"));
	CHECK(has_token(tokens, TokenType::Type, "int"));
}

static void test_cpp_block_comment() {
	auto tokens = highlight("  /* multi\n  line */ int x;\n", "cpp");
	CHECK(has_token(tokens, TokenType::Comment, "/* multi\n  line */"));
	CHECK(has_token(tokens, TokenType::Type, "int"));
}

static void test_cpp_preprocessor() {
	auto tokens = highlight("#include <stdio.h>\n", "cpp");
	CHECK(has_token(tokens, TokenType::Preprocessor, "#include <stdio.h>\n"));
}

static void test_cpp_types() {
	auto tokens = highlight("  BView* view;\n", "cpp");
	CHECK(has_token(tokens, TokenType::Type, "BView"));
}

static void test_python_keywords() {
	auto tokens = highlight("  def foo():\n    return None\n", "python");
	CHECK(has_token(tokens, TokenType::Keyword, "def"));
	CHECK(has_token(tokens, TokenType::Keyword, "return"));
	CHECK(has_token(tokens, TokenType::Keyword, "None"));
}

static void test_python_comments() {
	auto tokens = highlight("  x = 5  # comment\n", "py");
	CHECK(has_token(tokens, TokenType::Comment, "# comment\n"));
	CHECK(has_token(tokens, TokenType::Number, "5"));
}

static void test_shell_keywords() {
	auto tokens = highlight("  if [ -f foo ]; then\n    echo ok\n  fi\n", "sh");
	CHECK(has_token(tokens, TokenType::Keyword, "if"));
	CHECK(has_token(tokens, TokenType::Keyword, "then"));
	CHECK(has_token(tokens, TokenType::Keyword, "fi"));
	CHECK(has_token(tokens, TokenType::Keyword, "echo"));
}

static void test_js_keywords() {
	auto tokens = highlight("  const x = async () => { return 42; };\n", "js");
	CHECK(has_token(tokens, TokenType::Keyword, "const"));
	CHECK(has_token(tokens, TokenType::Keyword, "async"));
	CHECK(has_token(tokens, TokenType::Keyword, "return"));
	CHECK(has_token(tokens, TokenType::Number, "42"));
}

static void test_hex_numbers() {
	auto tokens = highlight("  int x = 0xFF00;\n", "cpp");
	CHECK(has_token(tokens, TokenType::Number, "0xFF00"));
}

static void test_roundtrip() {
	std::string code = "  #include <cstdio>\n  int main() {\n"
	                   "    // hello\n    printf(\"test %d\\n\", 42);\n"
	                   "    return 0;\n  }\n";
	auto tokens = highlight(code, "cpp");
	CHECK(reconstruct(tokens) == code);
}

static void test_empty() {
	auto tokens = highlight("", "cpp");
	CHECK(tokens.empty());
}

int main() {
	test_unknown_lang();
	test_cpp_keywords();
	test_cpp_strings();
	test_cpp_comments();
	test_cpp_block_comment();
	test_cpp_preprocessor();
	test_cpp_types();
	test_python_keywords();
	test_python_comments();
	test_shell_keywords();
	test_js_keywords();
	test_hex_numbers();
	test_roundtrip();
	test_empty();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_syntax: OK (14 tests)\n");
		return 0;
	}
	std::fprintf(stderr, "test_syntax: %d failure(s)\n", g_failures);
	return 1;
}
