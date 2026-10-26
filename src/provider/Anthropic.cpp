#include "Anthropic.h"
#include "../version.h"

#ifdef __HAIKU__
#include <OS.h>
#endif
#include <curl/curl.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utility>

#include "../cancel.h"
#include "../json.h"
#include "../log.h"

namespace pluto {

// RAII guards for libcurl resources — prevent leaks on exceptions.
struct CurlGuard {
	CURL* curl;
	explicit CurlGuard(CURL* c) : curl(c) {}
	~CurlGuard() { if (curl) curl_easy_cleanup(curl); }
	CurlGuard(const CurlGuard&) = delete;
	CurlGuard& operator=(const CurlGuard&) = delete;
};

struct HeadersGuard {
	curl_slist* headers = nullptr;
	~HeadersGuard() { if (headers) curl_slist_free_all(headers); }
};

struct StreamState {
	std::string buffer;
	std::string event_data;
	StreamCallback cb;
	bool done = false;
	bool cb_error = false;

	enum { NONE, TEXT_BLOCK, TOOL_USE_BLOCK } block_type = NONE;
	std::string tool_id;
	std::string tool_name;
	std::string tool_input;

	int input_tokens = 0;
	int output_tokens = 0;
	int cache_read = 0;
	int cache_creation = 0;
};

json::Value ephemeral_cache_control() {
	json::Value cc;
	cc.set("type", std::string("ephemeral"));
	return cc;
}

std::string build_request_body(const CompletionRequest& req) {
	using namespace json;
	Value root;
	root.set("model", req.model);
	root.set("max_tokens", Value(req.max_tokens));
	root.set("stream", Value(true));

	if (!req.system.empty()) {
		if (req.use_cache) {
			Array sys;
			Value block;
			block.set("type", std::string("text"));
			block.set("text", req.system);
			block.set("cache_control", ephemeral_cache_control());
			sys.push_back(std::move(block));
			root.set("system", Value(std::move(sys)));
		} else {
			root.set("system", req.system);
		}
	}

	if (!req.tools.empty()) {
		Array tools;
		for (const auto& t : req.tools) {
			Value tv;
			tv.set("name", t.name);
			tv.set("description", t.description);
			Value schema(Object{});
			if (!t.input_schema_json.empty()) {
				try { schema = Value::parse(t.input_schema_json); }
				catch (const std::exception&) { schema = Value(Object{}); }
			}
			tv.set("input_schema", schema);
			tools.push_back(tv);
		}
		if (req.use_cache && !tools.empty())
			tools.back().set("cache_control", ephemeral_cache_control());
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
				if (b.text.empty()) continue;
				bv.set("type", std::string("text"));
				bv.set("text", b.text);
			} else if (b.type == ContentBlock::IMAGE) {
				bv.set("type", std::string("image"));
				Value source;
				source.set("type", std::string("base64"));
				source.set("media_type", b.image_media_type);
				source.set("data", b.image_data);
				bv.set("source", source);
			} else if (b.type == ContentBlock::DOCUMENT) {
				bv.set("type", std::string("document"));
				Value source;
				source.set("type", std::string("base64"));
				source.set("media_type", b.doc_media_type);
				source.set("data", b.doc_data);
				bv.set("source", source);
			} else if (b.type == ContentBlock::AUDIO) {
				bv.set("type", std::string("text"));
				size_t kb = b.audio_data.size() / 1370;
				bv.set("text",
					std::string("[Audio file attached: ") + b.audio_format
					+ ", ~" + std::to_string(kb) + " KB. "
					"Note: the audio content cannot be heard by the model. "
					"The user intended to share this audio. "
					"Please acknowledge the audio file and ask the user "
					"to describe it if needed for the task.]");
			} else if (b.type == ContentBlock::TOOL_USE) {
				bv.set("type", std::string("tool_use"));
				bv.set("id", b.tool_id);
				bv.set("name", b.tool_name);
				Value input(Object{});
				if (!b.tool_input.empty()) {
					try { input = Value::parse(b.tool_input); }
					catch (const std::exception&) { input = Value(Object{}); }
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
		if (m.role == "user") {
			bool has_text = false;
			for (const auto& bv : blocks) {
				const Value* t = bv.find("type");
				const Value* txt = bv.find("text");
				if (t && t->is_str() && t->as_str() == "text"
				    && txt && txt->is_str() && !txt->as_str().empty()) {
					has_text = true;
					break;
				}
			}
			if (!has_text) {
				Value placeholder;
				placeholder.set("type", std::string("text"));
				placeholder.set("text", std::string("(see attached media)"));
				blocks.insert(blocks.begin(), std::move(placeholder));
			}
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
	catch (const std::exception&) { return; }
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
				const json::Value* cr = usage->find("cache_read_input_tokens");
				if (cr && cr->is_num()) st.cache_read = static_cast<int>(cr->as_num());
				const json::Value* cc = usage->find("cache_creation_input_tokens");
				if (cc && cc->is_num()) st.cache_creation = static_cast<int>(cc->as_num());
			}
		}
	} else if (type == "content_block_start") {
		const json::Value* cb = data.find("content_block");
		if (!cb || !cb->is_obj()) return;
		std::string ct = cb->str("type");
		if (ct == "text") {
			st.block_type = StreamState::TEXT_BLOCK;
		} else if (ct == "image") {
			st.block_type = StreamState::NONE;
			const json::Value* source = cb->find("source");
			if (source && source->is_obj()) {
				StreamEvent ev;
				ev.type = StreamEvent::IMAGE_BLOCK;
				ev.image_data = source->str("data");
				ev.image_media_type = source->str("media_type");
				if (ev.image_media_type.empty())
					ev.image_media_type = "image/png";
				st.cb(ev);
			}
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
				ev.cache_read_tokens = st.cache_read;
				ev.cache_creation_tokens = st.cache_creation;
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

int xferinfo_cb(void* userdata,
                curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
	auto* flag = static_cast<std::atomic<bool>*>(userdata);
	if (flag && flag->load(std::memory_order_relaxed)) return 1;
	return IsCancelRequested() ? 1 : 0;
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* st = static_cast<StreamState*>(userdata);
	if (st->cb_error) return 0;
	try {
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
		}
	} catch (const std::exception&) {
		st->cb_error = true;
		return 0;
	}
	return size * nmemb;
}

AnthropicProvider::AnthropicProvider(std::string token, AuthType auth_type,
                                     std::string base_url)
	: fToken(std::move(token)), fAuthType(auth_type),
	  fBaseUrl(std::move(base_url)) {}

void AnthropicProvider::stream(const CompletionRequest& req, StreamCallback cb) {
	if (req.model.find("gpt-image") != std::string::npos
	    || req.model.find("dall-e") != std::string::npos) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "Image generation model '" + req.model
			+ "' requires the OpenAI provider. Switch to OpenAI in Settings.";
		cb(err);
		return;
	}

	std::string body;
	try {
		body = build_request_body(req);
	} catch (const std::exception& e) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = std::string("build_request_body: ") + e.what();
		cb(err);
		return;
	}

	const std::string url = fBaseUrl + "/v1/messages";
	log_debug("anthropic", "POST %s (%zu bytes, %zu tools, model=%s)",
		url.c_str(), body.size(), req.tools.size(), req.model.c_str());

	const int kMaxAttempts = 8;
	long retry_after_secs = 0;

	for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
		CURL* curl = curl_easy_init();
		if (!curl) {
			StreamEvent err;
			err.type = StreamEvent::ERROR;
			err.error = "curl_easy_init failed";
			cb(err);
			return;
		}
		CurlGuard curl_guard(curl);

		StreamState st;
		st.cb = cb;
		retry_after_secs = 0;

		HeadersGuard hg;
		struct curl_slist*& headers = hg.headers;
		if (fAuthType == AuthType::OAUTH) {
			if (!fToken.empty()) {
				std::string auth = "Authorization: Bearer " + fToken;
				headers = curl_slist_append(headers, auth.c_str());
			}
		} else if (!fToken.empty()) {
			std::string auth = "x-api-key: " + fToken;
			headers = curl_slist_append(headers, auth.c_str());
		}
		if (fAuthType == AuthType::OAUTH)
			headers = curl_slist_append(headers,
				"anthropic-beta: oauth-2025-04-20,prompt-caching-2024-07-31");
		else
			headers = curl_slist_append(headers,
				"anthropic-beta: prompt-caching-2024-07-31");
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
		curl_easy_setopt(curl, CURLOPT_CAINFO,
			"/system/data/ssl/CARootCertificates.pem");
		curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, req.cancel);

		auto header_cb = [](char* buf, size_t sz, size_t n, void* ud) -> size_t {
			size_t len = sz * n;
			auto* out = static_cast<long*>(ud);
			if (len > 13 && strncasecmp(buf, "retry-after:", 12) == 0) {
				*out = std::atol(buf + 12);
				if (*out <= 0) *out = 1;
			}
			return len;
		};
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +header_cb);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &retry_after_secs);

		CURLcode rc = curl_easy_perform(curl);
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (rc != CURLE_OK) {
			if (rc == CURLE_ABORTED_BY_CALLBACK) {
				StreamEvent err;
				err.type = StreamEvent::ERROR;
				err.error = "cancelled";
				cb(err);
				return;
			}
			bool pre_stream = (http_code == 0);
			bool conn_err =
				rc == CURLE_COULDNT_CONNECT ||
				rc == CURLE_COULDNT_RESOLVE_HOST ||
				rc == CURLE_OPERATION_TIMEDOUT;
			if (pre_stream && conn_err && attempt < kMaxAttempts) {
				int64_t delay = 1000000LL * (1LL << (attempt - 1));
				log_warn("curl", "%s, retrying in %llds",
					curl_easy_strerror(rc),
					(long long)(delay / 1000000));
				StreamEvent info;
				info.type = StreamEvent::RETRY_INFO;
				info.text = std::string(curl_easy_strerror(rc))
					+ ", retrying in " + std::to_string(delay / 1000000) + "s...";
				cb(info);
#ifdef __HAIKU__
				snooze(delay);
#else
				usleep(static_cast<useconds_t>(delay));
#endif
				continue;
			}
			StreamEvent err;
			err.type = StreamEvent::ERROR;
			err.error = std::string("curl: ") + curl_easy_strerror(rc);
			cb(err);
			return;
		}

		if (http_code == 200) {
			if (!st.done) {
				StreamEvent ev;
				ev.type = StreamEvent::MESSAGE_END;
				ev.stop_reason = "end_turn";
				cb(ev);
			}
			if (st.cache_read > 0 || st.cache_creation > 0) {
				log_debug("cache", "read=%d created=%d",
					st.cache_read, st.cache_creation);
			}
			return;
		}

		bool retryable = (http_code == 429 || http_code >= 500);
		if (retryable && attempt < kMaxAttempts) {
			long backoff;
			if (retry_after_secs > 0) {
				backoff = retry_after_secs;
				if (backoff > 300) backoff = 300;
			} else if (http_code == 429) {
				backoff = 4L * (1L << (attempt - 1));
				if (backoff > 60) backoff = 60;
			} else {
				backoff = 1L << (attempt - 1);
				if (backoff > 30) backoff = 30;
			}
			log_warn("anthropic", "HTTP %ld (retry-after=%ld), body: %.512s",
				http_code, retry_after_secs, st.buffer.c_str());
			StreamEvent info;
			info.type = StreamEvent::RETRY_INFO;
			info.text = "HTTP " + std::to_string(http_code)
				+ ", retrying in " + std::to_string(backoff) + "s...";
			cb(info);
#ifdef __HAIKU__
			snooze(backoff * 1000000LL);
#else
			usleep(static_cast<useconds_t>(backoff * 1000000LL));
#endif
			continue;
		}

		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "HTTP " + std::to_string(http_code) + ": " + st.buffer;
		cb(err);
		return;
	}
}

}  // namespace pluto
