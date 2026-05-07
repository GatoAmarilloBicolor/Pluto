// Tests for image (vision) support: base64 encoding, content block
// creation, and provider serialization of IMAGE blocks.

#include "../src/base64.h"
#include "../src/json.h"
#include "../src/provider/Provider.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace pluto;
using pluto::base64_encode;

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	if ((a) != (b)) { \
		std::fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", \
			__FILE__, __LINE__, \
			std::string(a).c_str(), std::string(b).c_str()); \
		++g_failures; \
	} \
} while (0)

// --- base64 tests ---

static void test_base64_empty() {
	CHECK_EQ(base64_encode(""), "");
}

static void test_base64_one_byte() {
	CHECK_EQ(base64_encode("A"), "QQ==");
}

static void test_base64_two_bytes() {
	CHECK_EQ(base64_encode("AB"), "QUI=");
}

static void test_base64_three_bytes() {
	CHECK_EQ(base64_encode("ABC"), "QUJD");
}

static void test_base64_hello() {
	CHECK_EQ(base64_encode("Hello, World!"), "SGVsbG8sIFdvcmxkIQ==");
}

static void test_base64_binary() {
	// Test with null bytes and high bytes
	std::string bin = {'\x00', '\x01', '\x02', '\xff', '\xfe', '\xfd'};
	std::string result = base64_encode(bin.data(), bin.size());
	CHECK_EQ(result, "AAEC//79");
}

static void test_base64_padding() {
	// RFC 4648 test vectors
	CHECK_EQ(base64_encode("f"),      "Zg==");
	CHECK_EQ(base64_encode("fo"),     "Zm8=");
	CHECK_EQ(base64_encode("foo"),    "Zm9v");
	CHECK_EQ(base64_encode("foob"),   "Zm9vYg==");
	CHECK_EQ(base64_encode("fooba"),  "Zm9vYmE=");
	CHECK_EQ(base64_encode("foobar"), "Zm9vYmFy");
}

// --- ContentBlock IMAGE tests ---

static void test_image_content_block() {
	ContentBlock b;
	b.type = ContentBlock::IMAGE;
	b.image_media_type = "image/png";
	b.image_data = "iVBORw0KGgo=";

	CHECK(b.type == ContentBlock::IMAGE);
	CHECK_EQ(b.image_media_type, "image/png");
	CHECK_EQ(b.image_data, "iVBORw0KGgo=");
}

static void test_message_with_text_and_image() {
	Message m;
	m.role = "user";

	ContentBlock text;
	text.type = ContentBlock::TEXT;
	text.text = "What is in this image?";
	m.content.push_back(std::move(text));

	ContentBlock img;
	img.type = ContentBlock::IMAGE;
	img.image_media_type = "image/jpeg";
	img.image_data = "base64data";
	m.content.push_back(std::move(img));

	CHECK(m.content.size() == 2);
	CHECK(m.content[0].type == ContentBlock::TEXT);
	CHECK(m.content[1].type == ContentBlock::IMAGE);
	CHECK_EQ(m.content[1].image_media_type, "image/jpeg");
}

// --- Session serialization roundtrip ---

static void test_image_json_roundtrip() {
	// Simulate what SessionStore does: serialize IMAGE block to JSON
	// and parse it back.
	json::Value bv;
	bv.set("type", std::string("image"));
	bv.set("media_type", std::string("image/png"));
	bv.set("data", std::string("dGVzdA=="));

	std::string serialized = bv.serialize();
	json::Value parsed = json::Value::parse(serialized);

	CHECK_EQ(parsed.str("type"), "image");
	CHECK_EQ(parsed.str("media_type"), "image/png");
	CHECK_EQ(parsed.str("data"), "dGVzdA==");
}

static void test_image_in_message_json() {
	// Build a message with mixed text + image, serialize, parse back.
	json::Value mv;
	mv.set("role", std::string("user"));

	json::Array blocks;

	json::Value tb;
	tb.set("type", std::string("text"));
	tb.set("text", std::string("describe this"));
	blocks.push_back(std::move(tb));

	json::Value ib;
	ib.set("type", std::string("image"));
	ib.set("media_type", std::string("image/webp"));
	ib.set("data", std::string("AAEC"));
	blocks.push_back(std::move(ib));

	mv.set("content", json::Value(std::move(blocks)));

	std::string s = mv.serialize();
	json::Value parsed = json::Value::parse(s);
	CHECK_EQ(parsed.str("role"), "user");

	const json::Value* content = parsed.find("content");
	CHECK(content != nullptr);
	CHECK(content->is_arr());
	CHECK(content->as_arr().size() == 2);
	CHECK_EQ(content->as_arr()[0].str("type"), "text");
	CHECK_EQ(content->as_arr()[1].str("type"), "image");
	CHECK_EQ(content->as_arr()[1].str("media_type"), "image/webp");
	CHECK_EQ(content->as_arr()[1].str("data"), "AAEC");
}

int main() {
	test_base64_empty();
	test_base64_one_byte();
	test_base64_two_bytes();
	test_base64_three_bytes();
	test_base64_hello();
	test_base64_binary();
	test_base64_padding();
	test_image_content_block();
	test_message_with_text_and_image();
	test_image_json_roundtrip();
	test_image_in_message_json();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_image: OK (11 tests)\n");
		return 0;
	}
	std::fprintf(stderr, "test_image: %d failure(s)\n", g_failures);
	return 1;
}
