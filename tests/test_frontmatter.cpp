// tests/test_frontmatter.cpp — unit tests for frontmatter parser

#include "../src/frontmatter.h"

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

void test_parse_basic() {
	TEST("ParseFrontmatter basic");
	std::string content =
		"---\n"
		"topic: My Topic\n"
		"tags: foo, bar, baz\n"
		"source: https://example.com\n"
		"---\n"
		"Body content here.\n";

	auto fm = pluto::ParseFrontmatter(content);
	ASSERT(fm.get("topic") == "My Topic");
	ASSERT(fm.get("source") == "https://example.com");
	ASSERT(fm.body == "Body content here.\n");
	PASS();
}

void test_parse_tags_list() {
	TEST("ParseFrontmatter get_list");
	std::string content =
		"---\n"
		"tags: alpha, beta, gamma\n"
		"---\n"
		"Body.\n";

	auto fm = pluto::ParseFrontmatter(content);
	auto tags = fm.get_list("tags");
	ASSERT(tags.size() == 3);
	ASSERT(tags[0] == "alpha");
	ASSERT(tags[1] == "beta");
	ASSERT(tags[2] == "gamma");
	PASS();
}

void test_no_frontmatter() {
	TEST("ParseFrontmatter no frontmatter");
	std::string content = "Just plain text.\nNo frontmatter here.\n";
	auto fm = pluto::ParseFrontmatter(content);
	ASSERT(fm.fields.empty());
	ASSERT(fm.body == content);
	PASS();
}

void test_has_field() {
	TEST("ParseFrontmatter has()");
	std::string content =
		"---\n"
		"name: test\n"
		"---\n"
		"Body.\n";

	auto fm = pluto::ParseFrontmatter(content);
	ASSERT(fm.has("name"));
	ASSERT(!fm.has("missing"));
	PASS();
}

void test_missing_field_returns_empty() {
	TEST("ParseFrontmatter get() missing field");
	std::string content = "---\ntopic: Hello\n---\nBody.\n";
	auto fm = pluto::ParseFrontmatter(content);
	ASSERT(fm.get("missing") == "");
	ASSERT(fm.get_list("missing").empty());
	PASS();
}

void test_split_csv() {
	TEST("SplitCsv basic");
	auto r = pluto::SplitCsv("  a , b , c  ");
	ASSERT(r.size() == 3);
	ASSERT(r[0] == "a");
	ASSERT(r[1] == "b");
	ASSERT(r[2] == "c");
	PASS();

	TEST("SplitCsv empty entries skipped");
	auto r2 = pluto::SplitCsv("a,,b,  ,c");
	ASSERT(r2.size() == 3);
	PASS();
}

int main() {
	std::printf("frontmatter tests:\n");
	test_parse_basic();
	test_parse_tags_list();
	test_no_frontmatter();
	test_has_field();
	test_missing_field_returns_empty();
	test_split_csv();
	std::printf("\n  %d/%d passed\n", g_pass, g_total);
	return (g_pass == g_total) ? 0 : 1;
}
