#include "Anthropic.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "../json.h"

namespace pluto {

namespace {

struct StreamState {
	std::string buffer;       // raw SSE byte accumulator
	std::string event_data;   // data field(s) of the current event
	StreamCallback cb;
	bool done = false;

	enum { NONE, TEXT_BLOCK, TOOL_USE_BLOCK } block_type = NONE;
	std::string tool_id;
	std::string tool_name;
	std::string tool_input;   // accumulates partial_json across deltas

	int input_tokens = 0;
	int output_tokens = 0;
};

std::string build_request_body(const CompletionRequest& req) {
	using namespace json;
	Value root;
	root.set("model", req.model);
	root.set("max_tokens", Value(req.max_tokens));
	root.set("stream", Value(true));
	if (!req.system.empty()) root.set("system", req.system);

	if (!req.tools.empty()) {
		Array tools;
		for (const auto& t : req.tools) {
			Value tv;
			tv.set("name", t.name);
			tv.set("description", t.description);
			Value schema(Object{});
			if (!t.input_schema_json.empty()) {
				try { schema = Value::parse(t.input_schema_json); }
				catch (...) { schema = Value(Object{}); }
			}
			tv.set("input_schema", schema);
			tools.push_back(tv);
		}
		root.set("tools", Value(std::move(tools)));
	}

	Array msgs;
	for (const auto& m : req.messages) {
		Value mv;
		mv.set("role", m.role);
		Array blocks;
		for (const auto& b : m.content) {
			Value bv;
			if (b.type == ContentBlock::TEXT) {
				bv.set("type", std::string("text"));
				bv.set("text", b.text);
			} else if (b.type == ContentBlock::TOOL_USE) {
				bv.set("type", std::string("tool_use"));
				bv.set("id", b.tool_id);
				bv.set("name", b.tool_name);
				Value input(Object{});
				if (!b.tool_input.empty()) {
					try { input = Value::parse(b.tool_input); }
					catch (...) { input = Value(Object{}); }
				}
				bv.set("input", input);
			} else if (b.type == ContentBlock::TOOL_RESULT) {
				bv.set("type", std::string("tool_result"));
				bv.set("tool_use_id", b.tool_id);
				bv.set("content", b.text);
				if (b.is_error) bv.set("is_error", Value(true));
			}
			blocks.push_back(bv);
		}
		mv.set("content", Value(std::move(blocks)));
		msgs.push_back(mv);
	}
	root.set("messages", Value(std::move(msgs)));
	return root.serialize();
}

void process_event(StreamState& st) {
	if (st.event_data.empty()) return;
	json::Value data;
	try { data = json::Value::parse(st.event_data); }
	catch (...) { return; }
	if (!data.is_obj()) return;

	std::string type = data.str("type");
	if (type.empty()) return;

	if (type == "message_start") {
		const json::Value* msg = data.find("message");
		if (msg && msg->is_obj()) {
			const json::Value* usage = msg->find("usage");
			if (usage && usage->is_obj()) {
				const json::Value* in = usage->find("input_tokens");
				if (in && in->is_num()) st.input_tokens = static_cast<int>(in->as_num());
				const json::Value* out = usage->find("output_tokens");
				if (out && out->is_num()) st.output_tokens = static_cast<int>(out->as_num());
			}
		}
	} else if (type == "content_block_start") {
		const json::Value* cb = data.find("content_block");
		if (!cb || !cb->is_obj()) return;
		std::string ct = cb->str("type");
		if (ct == "text") {
			st.block_type = StreamState::TEXT_BLOCK;
		} else if (ct == "tool_use") {
			st.block_type = StreamState::TOOL_USE_BLOCK;
			st.tool_id   = cb->str("id");
			st.tool_name = cb->str("name");
			st.tool_input.clear();
			StreamEvent ev;
			ev.type = StreamEvent::TOOL_USE_START;
			ev.tool_id = st.tool_id;
			ev.tool_name = st.tool_name;
			st.cb(ev);
		}
	} else if (type == "content_block_delta") {
		const json::Value* delta = data.find("delta");
		if (!delta || !delta->is_obj()) return;
		std::string dt = delta->str("type");
		if (dt == "text_delta") {
			std::string text = delta->str("text");
			if (!text.empty()) {
				StreamEvent ev;
				ev.type = StreamEvent::TEXT_DELTA;
				ev.text = std::move(text);
				st.cb(ev);
			}
		} else if (dt == "input_json_delta") {
			std::string partial = delta->str("partial_json");
			if (!partial.empty()) {
				st.tool_input += partial;
				StreamEvent ev;
				ev.type = StreamEvent::TOOL_USE_INPUT_DELTA;
				ev.tool_id = st.tool_id;
				ev.tool_input = partial;
				st.cb(ev);
			}
		}
	} else if (type == "content_block_stop") {
		if (st.block_type == StreamState::TOOL_USE_BLOCK) {
			StreamEvent ev;
			ev.type = StreamEvent::TOOL_USE_END;
			ev.tool_id = st.tool_id;
			ev.tool_name = st.tool_name;
			ev.tool_input = st.tool_input.empty() ? std::string("{}") : st.tool_input;
			st.cb(ev);
		}
		st.block_type = StreamState::NONE;
		st.tool_id.clear();
		st.tool_name.clear();
		st.tool_input.clear();
	} else if (type == "message_delta") {
		const json::Value* usage = data.find("usage");
		if (usage && usage->is_obj()) {
			const json::Value* out = usage->find("output_tokens");
			if (out && out->is_num()) st.output_tokens = static_cast<int>(out->as_num());
		}
		const json::Value* delta = data.find("delta");
		if (delta && delta->is_obj()) {
			std::string sr = delta->str("stop_reason");
			if (!sr.empty()) {
				StreamEvent ev;
				ev.type = StreamEvent::MESSAGE_END;
				ev.stop_reason = std::move(sr);
				ev.input_tokens = st.input_tokens;
				ev.output_tokens = st.output_tokens;
				st.cb(ev);
				st.done = true;
			}
		}
	} else if (type == "error") {
		std::string msg = "unknown error";
		const json::Value* err = data.find("error");
		if (err && err->is_obj()) msg = err->str("message");
		StreamEvent ev;
		ev.type = StreamEvent::ERROR;
		ev.error = std::move(msg);
		st.cb(ev);
		st.done = true;
	}
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* st = static_cast<StreamState*>(userdata);
	st->buffer.append(ptr, size * nmemb);
	for (;;) {
		size_t nl = st->buffer.find('\n');
		if (nl == std::string::npos) break;
		std::string line = st->buffer.substr(0, nl);
		st->buffer.erase(0, nl + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) {
			process_event(*st);
			st->event_data.clear();
			continue;
		}
		if (line.rfind("data:", 0) == 0) {
			size_t s = 5;
			if (s < line.size() && line[s] == ' ') ++s;
			if (!st->event_data.empty()) st->event_data += '\n';
			st->event_data.append(line, s, std::string::npos);
		}
		// event:/id:/retry: ignored
	}
	return size * nmemb;
}

}  // namespace

AnthropicProvider::AnthropicProvider(std::string api_key, std::string base_url)
	: fApiKey(std::move(api_key)), fBaseUrl(std::move(base_url)) {}

void AnthropicProvider::stream(const CompletionRequest& req, StreamCallback cb) {
	CURL* curl = curl_easy_init();
	if (!curl) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "curl_easy_init failed";
		cb(err);
		return;
	}

	StreamState st;
	st.cb = cb;

	std::string body;
	try {
		body = build_request_body(req);
	} catch (const std::exception& e) {
		curl_easy_cleanup(curl);
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = std::string("build_request_body: ") + e.what();
		cb(err);
		return;
	}

	std::string url = fBaseUrl + "/v1/messages";
	struct curl_slist* headers = nullptr;
	std::string auth = "x-api-key: " + fApiKey;
	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, "accept: text/event-stream");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "/system/data/ssl/CARootCertificates.pem");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "pluto/0.2.0 (Haiku)");

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = std::string("curl: ") + curl_easy_strerror(rc);
		cb(err);
		return;
	}
	if (http_code != 200) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "HTTP " + std::to_string(http_code) + ": " + st.buffer;
		cb(err);
		return;
	}
	if (!st.done) {
		StreamEvent ev;
		ev.type = StreamEvent::MESSAGE_END;
		ev.stop_reason = "end_turn";
		cb(ev);
	}
}

}  // namespace pluto
