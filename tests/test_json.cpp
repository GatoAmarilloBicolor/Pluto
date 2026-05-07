// Minimal self-contained test runner for src/json.cpp. No framework.
// Usage: make check

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

#define CHECK_EQ(a, b) do { \
	auto _a = (a); auto _b = (b); \
	if (!(_a == _b)) { \
		std::fprintf(stderr, "FAIL %s:%d: " #a " == " #b "\n", \
			__FILE__, __LINE__); \
		++g_failures; \
	} \
} while (0)

using pluto::json::Value;

static void test_primitives() {
	CHECK(Value::parse("null").is_null());
	CHECK(Value::parse("true").is_bool());
	CHECK(Value::parse("true").as_bool() == true);
	CHECK(Value::parse("false").as_bool() == false);
	CHECK(Value::parse("42").is_num());
	CHECK_EQ(Value::parse("42").as_num(), 42.0);
	CHECK_EQ(Value::parse("-3.5").as_num(), -3.5);
	CHECK(Value::parse("\"hello\"").is_str());
	CHECK_EQ(Value::parse("\"hello\"").as_str(), std::string("hello"));
}

static void test_escapes() {
	auto v = Value::parse("\"a\\nb\\tc\"");
	CHECK(v.is_str());
	CHECK_EQ(v.as_str(), std::string("a\nb\tc"));

	auto q = Value::parse("\"quote\\\"inside\"");
	CHECK(q.is_str());
	CHECK_EQ(q.as_str(), std::string("quote\"inside"));
}

static void test_object() {
	auto v = Value::parse(R"({"name":"bob","age":30,"active":true})");
	CHECK(v.is_obj());
	CHECK_EQ(v.str("name"), std::string("bob"));

	const Value* age = v.find("age");
	CHECK(age && age->is_num());
	CHECK_EQ(age->as_num(), 30.0);

	const Value* missing = v.find("nope");
	CHECK(missing == nullptr);

	CHECK_EQ(v.str("nonexistent"), std::string());
}

static void test_array() {
	auto v = Value::parse(R"([1,2,3,"four"])");
	CHECK(v.is_arr());
	CHECK_EQ(v.as_arr().size(), size_t(4));
	CHECK_EQ(v.as_arr()[0].as_num(), 1.0);
	CHECK(v.as_arr()[3].is_str());
	CHECK_EQ(v.as_arr()[3].as_str(), std::string("four"));
}

static void test_nested() {
	auto v = Value::parse(
		R"({"tools":[{"name":"read","schema":{"type":"object"}}]})");
	CHECK(v.is_obj());
	const Value* tools = v.find("tools");
	CHECK(tools && tools->is_arr());
	CHECK_EQ(tools->as_arr().size(), size_t(1));
	const Value& first = tools->as_arr()[0];
	CHECK_EQ(first.str("name"), std::string("read"));
	const Value* schema = first.find("schema");
	CHECK(schema && schema->is_obj());
	CHECK_EQ(schema->str("type"), std::string("object"));
}

static void test_roundtrip() {
	Value root;
	root.set("model", std::string("claude-opus-4-5"));
	root.set("max_tokens", Value(4096));
	root.set("stream", Value(true));

	pluto::json::Array arr;
	arr.push_back(Value(std::string("a")));
	arr.push_back(Value(std::string("b")));
	root.set("items", Value(std::move(arr)));

	std::string serialized = root.serialize();
	auto reparsed = Value::parse(serialized);
	CHECK(reparsed.is_obj());
	CHECK_EQ(reparsed.str("model"), std::string("claude-opus-4-5"));
	const Value* items = reparsed.find("items");
	CHECK(items && items->is_arr());
	CHECK_EQ(items->as_arr().size(), size_t(2));
}

static void test_whitespace() {
	auto v = Value::parse(" \n\t{ \"k\"\n:  \"v\"  }\n");
	CHECK(v.is_obj());
	CHECK_EQ(v.str("k"), std::string("v"));
}

static void test_unicode_bmp() {
	// \u00e9 = é (2-byte UTF-8)
	auto v = Value::parse("\"caf\\u00e9\"");
	CHECK_EQ(v.as_str(), std::string("caf\xc3\xa9"));
	// \u4e16 = 世 (3-byte UTF-8)
	auto w = Value::parse("\"\\u4e16\"");
	CHECK_EQ(w.as_str(), std::string("\xe4\xb8\x96"));
}

static void test_unicode_surrogate_pair() {
	// \uD83D\uDE00 = 😀 (U+1F600, 4-byte UTF-8)
	auto v = Value::parse("\"\\uD83D\\uDE00\"");
	CHECK_EQ(v.as_str(), std::string("\xf0\x9f\x98\x80"));
	// \uD83C\uDF4E = 🍎 (U+1F34E)
	auto w = Value::parse("\"\\uD83C\\uDF4E\"");
	CHECK_EQ(w.as_str(), std::string("\xf0\x9f\x8d\x8e"));
}

static void test_unicode_bad_surrogate() {
	// Lone high surrogate should fail.
	bool threw = false;
	try { Value::parse("\"\\uD83D\""); }
	catch (...) { threw = true; }
	CHECK(threw);
	// High surrogate without low surrogate.
	threw = false;
	try { Value::parse("\"\\uD83Dabc\""); }
	catch (...) { threw = true; }
	CHECK(threw);
}

static void test_deep_nesting() {
	// 300 levels should fail (limit is 256).
	std::string deep(300, '[');
	deep += "1";
	deep += std::string(300, ']');
	bool threw = false;
	try { Value::parse(deep); }
	catch (...) { threw = true; }
	CHECK(threw);

	// 100 levels should succeed.
	std::string ok(100, '[');
	ok += "1";
	ok += std::string(100, ']');
	threw = false;
	try {
		auto v = Value::parse(ok);
		CHECK(v.is_arr());
	} catch (...) { threw = true; }
	CHECK(!threw);
}

int main() {
	test_primitives();
	test_escapes();
	test_object();
	test_array();
	test_nested();
	test_roundtrip();
	test_whitespace();
	test_unicode_bmp();
	test_unicode_surrogate_pair();
	test_unicode_bad_surrogate();
	test_deep_nesting();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_json: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_json: %d failure(s)\n", g_failures);
	return 1;
}
