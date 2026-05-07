// Test for the SSE stream parser used in Anthropic provider.
// We simulate raw SSE byte sequences and verify that the correct
// StreamEvents are emitted.

#include "../src/provider/Provider.h"
#include "../src/json.h"

#include <cstdio>
#include <string>
#include <vector>

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

using pluto::StreamEvent;

// Re-implement the core SSE line parser in isolation so we can test it
// without pulling in curl. This mirrors the logic in Anthropic.cpp's
// write_cb + process_event but in a self-contained form.

struct TestStreamState {
	std::string buffer;
	std::string event_data;
	std::vector<StreamEvent> events;

	void process_event() {
		if (event_data.empty()) return;
		pluto::json::Value data;
		try { data = pluto::json::Value::parse(event_data); }
		catch (...) { return; }
		if (!data.is_obj()) return;

		std::string type = data.str("type");
		if (type.empty()) return;

		StreamEvent ev;

		if (type == "message_start") {
			// skip for test simplicity
		} else if (type == "content_block_start") {
			const auto* cb = data.find("content_block");
			if (!cb || !cb->is_obj()) return;
			std::string ct = cb->str("type");
			if (ct == "tool_use") {
				ev.type = StreamEvent::TOOL_USE_START;
				ev.tool_id = cb->str("id");
				ev.tool_name = cb->str("name");
				events.push_back(ev);
			}
		} else if (type == "content_block_delta") {
			const auto* delta = data.find("delta");
			if (!delta || !delta->is_obj()) return;
			std::string dt = delta->str("type");
			if (dt == "text_delta") {
				ev.type = StreamEvent::TEXT_DELTA;
				ev.text = delta->str("text");
				events.push_back(ev);
			}
		} else if (type == "message_delta") {
			const auto* delta = data.find("delta");
			if (delta && delta->is_obj()) {
				std::string sr = delta->str("stop_reason");
				if (!sr.empty()) {
					ev.type = StreamEvent::MESSAGE_END;
					ev.stop_reason = sr;
					events.push_back(ev);
				}
			}
		} else if (type == "error") {
			ev.type = StreamEvent::ERROR;
			const auto* err = data.find("error");
			if (err && err->is_obj())
				ev.error = err->str("message");
			else
				ev.error = "unknown";
			events.push_back(ev);
		}
	}

	void feed(const std::string& chunk) {
		buffer += chunk;
		for (;;) {
			size_t nl = buffer.find('\n');
			if (nl == std::string::npos) break;
			std::string line = buffer.substr(0, nl);
			buffer.erase(0, nl + 1);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty()) {
				process_event();
				event_data.clear();
				continue;
			}
			if (line.rfind("data:", 0) == 0) {
				size_t s = 5;
				if (s < line.size() && line[s] == ' ') ++s;
				if (!event_data.empty()) event_data += '\n';
				event_data.append(line, s, std::string::npos);
			}
		}
	}
};

static void test_text_delta() {
	TestStreamState st;

	// Simulate a simple text delta event
	st.feed(
		"data: {\"type\":\"content_block_delta\","
		"\"delta\":{\"type\":\"text_delta\",\"text\":\"hello\"}}\n\n"
	);

	CHECK_EQ(st.events.size(), size_t(1));
	CHECK(st.events[0].type == StreamEvent::TEXT_DELTA);
	CHECK_EQ(st.events[0].text, std::string("hello"));
}

static void test_multi_chunk() {
	TestStreamState st;

	// Data split across two feed() calls
	st.feed("data: {\"type\":\"content_block_del");
	CHECK_EQ(st.events.size(), size_t(0));  // not complete yet

	st.feed("ta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"world\"}}\n\n");
	CHECK_EQ(st.events.size(), size_t(1));
	CHECK_EQ(st.events[0].text, std::string("world"));
}

static void test_message_end() {
	TestStreamState st;
	st.feed(
		"data: {\"type\":\"message_delta\","
		"\"delta\":{\"stop_reason\":\"end_turn\"}}\n\n"
	);
	CHECK_EQ(st.events.size(), size_t(1));
	CHECK(st.events[0].type == StreamEvent::MESSAGE_END);
	CHECK_EQ(st.events[0].stop_reason, std::string("end_turn"));
}

static void test_tool_use_start() {
	TestStreamState st;
	st.feed(
		"data: {\"type\":\"content_block_start\","
		"\"content_block\":{\"type\":\"tool_use\","
		"\"id\":\"tu_1\",\"name\":\"read_file\"}}\n\n"
	);
	CHECK_EQ(st.events.size(), size_t(1));
	CHECK(st.events[0].type == StreamEvent::TOOL_USE_START);
	CHECK_EQ(st.events[0].tool_name, std::string("read_file"));
	CHECK_EQ(st.events[0].tool_id, std::string("tu_1"));
}

static void test_error_event() {
	TestStreamState st;
	st.feed(
		"data: {\"type\":\"error\","
		"\"error\":{\"type\":\"overloaded\","
		"\"message\":\"too many requests\"}}\n\n"
	);
	CHECK_EQ(st.events.size(), size_t(1));
	CHECK(st.events[0].type == StreamEvent::ERROR);
	CHECK_EQ(st.events[0].error, std::string("too many requests"));
}

static void test_sequence() {
	TestStreamState st;
	// text → tool → text → end
	st.feed(
		"data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"one\"}}\n\n"
		"data: {\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"id\":\"t1\",\"name\":\"bash\"}}\n\n"
		"data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"two\"}}\n\n"
		"data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"}}\n\n"
	);
	CHECK_EQ(st.events.size(), size_t(4));
	CHECK(st.events[0].type == StreamEvent::TEXT_DELTA);
	CHECK(st.events[1].type == StreamEvent::TOOL_USE_START);
	CHECK(st.events[2].type == StreamEvent::TEXT_DELTA);
	CHECK(st.events[3].type == StreamEvent::MESSAGE_END);
	CHECK_EQ(st.events[3].stop_reason, std::string("tool_use"));
}

static void test_crlf() {
	TestStreamState st;
	// Lines terminated with \r\n instead of \n
	st.feed(
		"data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"cr\"}}\r\n\r\n"
	);
	CHECK_EQ(st.events.size(), size_t(1));
	CHECK_EQ(st.events[0].text, std::string("cr"));
}

int main() {
	test_text_delta();
	test_multi_chunk();
	test_message_end();
	test_tool_use_start();
	test_error_event();
	test_sequence();
	test_crlf();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_sse: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_sse: %d failure(s)\n", g_failures);
	return 1;
}
