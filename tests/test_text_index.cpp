// tests/test_text_index.cpp — unit tests for text_index module

#include "../src/text_index.h"

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

void test_to_lower() {
	TEST("ToLower basic");
	ASSERT(pluto::ToLower("Hello World") == "hello world");
	ASSERT(pluto::ToLower("ABC123") == "abc123");
	ASSERT(pluto::ToLower("") == "");
	PASS();
}

void test_tokenize() {
	TEST("Tokenize splits on non-alnum");
	auto t = pluto::Tokenize("Hello, World! foo_bar");
	ASSERT(t.size() == 3);
	ASSERT(t[0] == "hello");
	ASSERT(t[1] == "world");
	ASSERT(t[2] == "foo_bar");
	PASS();

	TEST("Tokenize empty input");
	auto e = pluto::Tokenize("");
	ASSERT(e.empty());
	PASS();

	TEST("Tokenize preserves underscores");
	auto u = pluto::Tokenize("my_var_name");
	ASSERT(u.size() == 1);
	ASSERT(u[0] == "my_var_name");
	PASS();
}

void test_build_idf() {
	TEST("BuildIdf with 2 documents");
	std::vector<std::unordered_set<std::string>> docs = {
		{"hello", "world", "foo"},
		{"hello", "bar", "baz"}
	};
	auto idf = pluto::BuildIdf(docs);
	// "hello" appears in both docs: idf = log(3/2) ≈ 0.405
	// "world" appears in 1 doc: idf = log(3/1) ≈ 1.099
	ASSERT(idf.count("hello") > 0);
	ASSERT(idf.count("world") > 0);
	ASSERT(idf["hello"] < idf["world"]);  // more common = lower IDF
	PASS();

	TEST("BuildIdf empty");
	std::vector<std::unordered_set<std::string>> empty;
	auto idf2 = pluto::BuildIdf(empty);
	ASSERT(idf2.empty());
	PASS();
}

void test_score_document() {
	TEST("ScoreDocument matches fields by weight");
	std::vector<std::unordered_set<std::string>> docs = {
		{"bquery", "filesystem", "query"},
		{"bwindow", "layout", "view"}
	};
	auto idf = pluto::BuildIdf(docs);

	std::vector<pluto::ScoredField> fields = {
		{"BQuery filesystem query", 4.0},
		{"Some body text about BFS", 1.0}
	};
	auto query = pluto::Tokenize("bquery filesystem");
	double score = pluto::ScoreDocument(fields, query, idf);
	ASSERT(score > 0.0);
	PASS();

	TEST("ScoreDocument no match returns 0");
	auto query2 = pluto::Tokenize("unrelated topic");
	double score2 = pluto::ScoreDocument(fields, query2, idf);
	ASSERT(score2 == 0.0);
	PASS();
}

int main() {
	std::printf("text_index tests:\n");
	test_to_lower();
	test_tokenize();
	test_build_idf();
	test_score_document();
	std::printf("\n  %d/%d passed\n", g_pass, g_total);
	return (g_pass == g_total) ? 0 : 1;
}
