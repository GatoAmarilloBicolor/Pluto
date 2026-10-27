// OpenAI-compatible streaming provider.
// Maps between pluto's Anthropic-shaped ContentBlock model and the
// OpenAI /v1/chat/completions SSE format.

#include "OpenAICompat.h"
#include "../version.h"

#ifdef __HAIKU__
#include <OS.h>
#endif
#include <curl/curl.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utility>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "../cancel.h"
#include "../json.h"

namespace pluto {

namespace {

// RAII guards for libcurl resources.
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

// ----------------------------------------------------------------
// Request body builder (OpenAI format)
// ----------------------------------------------------------------

// OpenAI reasoning models (o1, o3, o4 families) require
// max_completion_tokens instead of max_tokens and reject the latter
// with HTTP 400. They also don't support stream_options.
bool is_reasoning_model(const std::string& model) {
	// Match: o1, o1-mini, o1-preview, o3, o3-mini, o4-mini, etc.
	if (model.size() >= 2 && model[0] == 'o' && model[1] >= '0' && model[1] <= '9')
		return true;
	// Also match vendor-prefixed IDs like "openai/o3-mini".
	auto slash = model.rfind('/');
	if (slash != std::string::npos && slash + 2 < model.size()) {
		char a = model[slash + 1], b = model[slash + 2];
		if (a == 'o' && b >= '0' && b <= '9') return true;
	}
	return false;
}

std::string build_oai_body(const CompletionRequest& req) {
	using namespace json;
	Value root;
	root.set("model", req.model);
	root.set("stream", Value(true));

	bool reasoning = is_reasoning_model(req.model);
	if (reasoning)
		root.set("max_completion_tokens", Value(req.max_tokens));
	else
		root.set("max_tokens", Value(req.max_tokens));

	// Ask the server to include usage in the stream.
	// Reasoning models don't support stream_options.
	if (!reasoning) {
		Value so;
		so.set("include_usage", Value(true));
		root.set("stream_options", so);
	}

	// --- Messages ---
	Array msgs;

	// System prompt → first message with role "system".
	if (!req.system.empty()) {
		Value sm;
		sm.set("role", std::string("system"));
		sm.set("content", req.system);
		msgs.push_back(std::move(sm));
	}

	for (const auto& m : req.messages) {
		bool has_tool_use = false;
		bool has_tool_result = false;
		for (const auto& b : m.content) {
			if (b.type == ContentBlock::TOOL_USE)    has_tool_use = true;
			if (b.type == ContentBlock::TOOL_RESULT)  has_tool_result = true;
		}

		if (m.role == "assistant") {
			Value mv;
			mv.set("role", std::string("assistant"));

			std::string text;
			for (const auto& b : m.content)
				if (b.type == ContentBlock::TEXT) text += b.text;
			mv.set("content", text.empty() ? Value(nullptr) : Value(text));

			if (has_tool_use) {
				Array tcs;
				for (const auto& b : m.content) {
					if (b.type != ContentBlock::TOOL_USE) continue;
					Value tc;
					tc.set("id", b.tool_id);
					tc.set("type", std::string("function"));
					Value fn;
					fn.set("name", b.tool_name);
					fn.set("arguments", b.tool_input.empty()
						? std::string("{}") : b.tool_input);
					tc.set("function", fn);
					tcs.push_back(std::move(tc));
				}
				mv.set("tool_calls", Value(std::move(tcs)));
			}
			msgs.push_back(std::move(mv));

		} else if (m.role == "user" && has_tool_result) {
			// Each tool_result becomes a separate "tool" message.
			for (const auto& b : m.content) {
				if (b.type == ContentBlock::TOOL_RESULT) {
					Value tv;
					tv.set("role", std::string("tool"));
					tv.set("tool_call_id", b.tool_id);
					tv.set("content", b.text);
					msgs.push_back(std::move(tv));
				} else if (b.type == ContentBlock::TEXT && !b.text.empty()) {
					Value uv;
					uv.set("role", std::string("user"));
					uv.set("content", b.text);
					msgs.push_back(std::move(uv));
				}
			}

		} else {
			// Normal user or assistant message.
			// If it contains media (images, audio, PDF), use multi-part content.
			bool has_media = false;
			for (const auto& b : m.content)
				if (b.type == ContentBlock::IMAGE
				    || b.type == ContentBlock::AUDIO
				    || b.type == ContentBlock::DOCUMENT)
					{ has_media = true; break; }

			Value mv;
			mv.set("role", m.role);
			if (has_media) {
				Array parts;
				for (const auto& b : m.content) {
					if (b.type == ContentBlock::TEXT && !b.text.empty()) {
						Value tv;
						tv.set("type", std::string("text"));
						tv.set("text", b.text);
						parts.push_back(std::move(tv));
					} else if (b.type == ContentBlock::IMAGE) {
						Value iv;
						iv.set("type", std::string("image_url"));
						Value url_obj;
						url_obj.set("url", std::string("data:")
							+ b.image_media_type + ";base64," + b.image_data);
						iv.set("image_url", url_obj);
						parts.push_back(std::move(iv));
					} else if (b.type == ContentBlock::AUDIO) {
						// OpenAI audio input (gpt-4o-audio models).
						Value av;
						av.set("type", std::string("input_audio"));
						Value audio_obj;
						audio_obj.set("data", b.audio_data);
						audio_obj.set("format", b.audio_format);
						av.set("input_audio", audio_obj);
						parts.push_back(std::move(av));
					} else if (b.type == ContentBlock::DOCUMENT) {
						// PDF as text note — most OpenAI-compat servers
						// don't support document blocks natively.
						Value tv;
						tv.set("type", std::string("text"));
						tv.set("text", std::string("[PDF document attached, ")
							+ std::to_string(b.doc_data.size() / 1370)
							+ " KB]");
						parts.push_back(std::move(tv));
					}
				}
				// Ensure at least one text part exists.
				if (parts.empty() || std::none_of(parts.begin(), parts.end(),
				    [](const Value& v) {
				        const Value* t = v.find("type");
				        return t && t->is_str() && t->as_str() == "text";
				    })) {
					Value tv;
					tv.set("type", std::string("text"));
					tv.set("text", std::string("(see attached media)"));
					parts.insert(parts.begin(), std::move(tv));
				}
				mv.set("content", Value(std::move(parts)));
			} else {
				std::string text;
				for (const auto& b : m.content)
					if (b.type == ContentBlock::TEXT) text += b.text;
				mv.set("content", text);
			}
			msgs.push_back(std::move(mv));
		}
	}
	root.set("messages", Value(std::move(msgs)));

	// --- Tools ---
	if (!req.tools.empty()) {
		Array tools;
		for (const auto& t : req.tools) {
			Value tv;
			tv.set("type", std::string("function"));
			Value fn;
			fn.set("name", t.name);
			fn.set("description", t.description);
			Value params(Object{});
			if (!t.input_schema_json.empty()) {
				try { params = Value::parse(t.input_schema_json); }
				catch (const std::exception&) { params = Value(Object{}); }
			}
			fn.set("parameters", params);
			tv.set("function", fn);
			tools.push_back(std::move(tv));
		}
		root.set("tools", Value(std::move(tools)));
	}

	return root.serialize();
}

// ----------------------------------------------------------------
// SSE stream state + event parser (OpenAI format)
// ----------------------------------------------------------------

struct OaiState {
	std::string buffer;
	std::string event_data;
	std::string raw_body;   // full response body for error reporting
	StreamCallback cb;
	bool done = false;
	bool cb_error = false;  // set if cb throws — aborts curl safely

	// Active tool call accumulation.
	std::string tool_id;
	std::string tool_name;
	std::string tool_args;
	bool in_tool = false;

	int prompt_tokens = 0;
	int completion_tokens = 0;

	std::string stop_reason;  // set on finish_reason, used at [DONE]
};

void end_tool_call(OaiState& st) {
	if (!st.in_tool) return;
	StreamEvent ev;
	ev.type = StreamEvent::TOOL_USE_END;
	ev.tool_id   = st.tool_id;
	ev.tool_name = st.tool_name;
	ev.tool_input = st.tool_args.empty() ? std::string("{}") : st.tool_args;
	st.cb(ev);
	st.in_tool = false;
	st.tool_id.clear();
	st.tool_name.clear();
	st.tool_args.clear();
}

void process_oai_event(OaiState& st) {
	if (st.event_data.empty()) return;

	if (st.event_data == "[DONE]") {
		end_tool_call(st);
		if (!st.done) {
			StreamEvent ev;
			ev.type = StreamEvent::MESSAGE_END;
			ev.stop_reason = st.stop_reason.empty()
				? "end_turn" : st.stop_reason;
			ev.input_tokens  = st.prompt_tokens;
			ev.output_tokens = st.completion_tokens;
			st.cb(ev);
			st.done = true;
		}
		return;
	}

	json::Value data;
	try { data = json::Value::parse(st.event_data); }
	catch (const std::exception&) { return; }
	if (!data.is_obj()) return;

	// Usage (may appear in any chunk; some providers send a trailing
	// usage-only chunk before [DONE]).
	const json::Value* usage = data.find("usage");
	if (usage && usage->is_obj()) {
		const json::Value* pt = usage->find("prompt_tokens");
		if (pt && pt->is_num()) st.prompt_tokens = static_cast<int>(pt->as_num());
		const json::Value* ct = usage->find("completion_tokens");
		if (ct && ct->is_num()) st.completion_tokens = static_cast<int>(ct->as_num());
	}

	// Error?
	const json::Value* error = data.find("error");
	if (error && error->is_obj()) {
		StreamEvent ev;
		ev.type  = StreamEvent::ERROR;
		ev.error = error->str("message");
		if (ev.error.empty()) ev.error = "unknown error";
		st.cb(ev);
		st.done = true;
		return;
	}

	const json::Value* choices = data.find("choices");
	if (!choices || !choices->is_arr() || choices->as_arr().empty()) return;
	const json::Value& choice = choices->as_arr()[0];

	// finish_reason
	std::string finish_reason;
	const json::Value* fr = choice.find("finish_reason");
	if (fr && fr->is_str()) finish_reason = fr->as_str();

	// Delta
	const json::Value* delta = choice.find("delta");
	if (delta && delta->is_obj()) {
		// Text content
		std::string content = delta->str("content");
		if (!content.empty()) {
			StreamEvent ev;
			ev.type = StreamEvent::TEXT_DELTA;
			ev.text = std::move(content);
			st.cb(ev);
		}

		// Content array (multi-part): may contain image blocks.
		const json::Value* content_arr = delta->find("content");
		if (content_arr && content_arr->is_arr()) {
			for (const auto& part : content_arr->as_arr()) {
				if (!part.is_obj()) continue;
				std::string ptype = part.str("type");
				if (ptype == "image_url") {
					const json::Value* url_obj = part.find("image_url");
					if (url_obj && url_obj->is_obj()) {
						std::string url = url_obj->str("url");
						// data:image/png;base64,... format
						if (url.rfind("data:", 0) == 0) {
							size_t semi = url.find(';');
							size_t comma = url.find(',');
							if (semi != std::string::npos && comma != std::string::npos) {
								StreamEvent ev;
								ev.type = StreamEvent::IMAGE_BLOCK;
								ev.image_media_type = url.substr(5, semi - 5);
								ev.image_data = url.substr(comma + 1);
								st.cb(ev);
							}
						}
					}
				}
			}
		}

		// Tool calls
		const json::Value* tcs = delta->find("tool_calls");
		if (tcs && tcs->is_arr()) {
			for (const auto& tc : tcs->as_arr()) {
				std::string tc_id = tc.str("id");
				const json::Value* fn = tc.find("function");
				if (!fn || !fn->is_obj()) continue;
				std::string fn_name = fn->str("name");
				std::string fn_args = fn->str("arguments");

				if (!tc_id.empty() && !fn_name.empty()) {
					// New tool call — finish any prior one first.
					end_tool_call(st);
					st.tool_id   = tc_id;
					st.tool_name = fn_name;
					st.tool_args = fn_args;
					st.in_tool = true;

					StreamEvent ev;
					ev.type      = StreamEvent::TOOL_USE_START;
					ev.tool_id   = tc_id;
					ev.tool_name = fn_name;
					st.cb(ev);
				} else if (!fn_args.empty() && st.in_tool) {
					st.tool_args += fn_args;
					StreamEvent ev;
					ev.type      = StreamEvent::TOOL_USE_INPUT_DELTA;
					ev.tool_id   = st.tool_id;
					ev.tool_input = fn_args;
					st.cb(ev);
				}
			}
		}
	}

	// finish_reason — record it but don't emit MESSAGE_END yet.
	// Usage data may arrive in a later chunk (Ollama sends a trailing
	// usage-only chunk with empty choices[] after finish_reason).
	// The actual MESSAGE_END is emitted on [DONE] where tokens are final.
	if (!finish_reason.empty() && finish_reason != "null") {
		end_tool_call(st);
		st.stop_reason = (finish_reason == "tool_calls")
			? "tool_use" : "end_turn";
	}
}

// ----------------------------------------------------------------
// curl write callback (identical SSE line protocol)
// ----------------------------------------------------------------

int oai_xferinfo(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
	auto* flag = static_cast<std::atomic<bool>*>(userdata);
	if (flag && flag->load(std::memory_order_relaxed)) return 1;
	return IsCancelRequested() ? 1 : 0;
}

size_t oai_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* st = static_cast<OaiState*>(userdata);
	if (st->cb_error) return 0;  // already failed — abort curl
	try {
		// Only accumulate raw_body for error reporting (non-SSE responses).
		// Cap at 64 KB to avoid doubling memory on large successful streams.
		if (st->raw_body.size() < 65536)
			st->raw_body.append(ptr, size * nmemb);
		st->buffer.append(ptr, size * nmemb);
		for (;;) {
			size_t nl = st->buffer.find('\n');
			if (nl == std::string::npos) break;
			std::string line = st->buffer.substr(0, nl);
			st->buffer.erase(0, nl + 1);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty()) {
				process_oai_event(*st);
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
		return 0;  // trigger CURLE_WRITE_ERROR
	}
	return size * nmemb;
}

}  // namespace

// ----------------------------------------------------------------
// OpenAICompatProvider
// ----------------------------------------------------------------

OpenAICompatProvider::OpenAICompatProvider(std::string api_key,
                                           std::string base_url)
	: fApiKey(std::move(api_key)),
	  fBaseUrl(std::move(base_url)) {
	// Strip trailing slash.
	while (!fBaseUrl.empty() && fBaseUrl.back() == '/')
		fBaseUrl.pop_back();
}

namespace {

bool is_image_model(const std::string& model) {
	// Models that use /v1/images/generations instead of /chat/completions.
	return model.find("dall-e") != std::string::npos
	    || model.find("gpt-image") != std::string::npos
	    || model.find("imagen") != std::string::npos;
}

// Extract the last user message text as the image prompt.
std::string extract_prompt(const CompletionRequest& req) {
	for (auto it = req.messages.rbegin(); it != req.messages.rend(); ++it) {
		if (it->role == "user") {
			for (const auto& b : it->content) {
				if (b.type == ContentBlock::TEXT && !b.text.empty())
					return b.text;
			}
		}
	}
	return "image";
}

size_t img_collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	static_cast<std::string*>(ud)->append(ptr, size * nmemb);
	return size * nmemb;
}

}  // namespace

void OpenAICompatProvider::_StreamImageGen(const CompletionRequest& req,
                                            StreamCallback cb) {
	std::string prompt = extract_prompt(req);

	json::Value body;
	body.set("model", json::Value(req.model));
	body.set("prompt", json::Value(prompt));
	body.set("n", json::Value(1));
	// gpt-image-1 uses "output_format", dall-e uses "response_format".
	if (req.model.find("gpt-image") != std::string::npos) {
		body.set("output_format", json::Value(std::string("png")));
	} else {
		body.set("response_format", json::Value(std::string("b64_json")));
	}

	std::string payload = body.serialize();
	std::string url = fBaseUrl + "/v1/images/generations";

	CURL* curl = curl_easy_init();
	if (!curl) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "curl_easy_init failed";
		cb(err);
		return;
	}
	CurlGuard curl_guard(curl);

	std::string response;
	HeadersGuard hg;
	struct curl_slist*& headers = hg.headers;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!fApiKey.empty()) {
		std::string auth = "Authorization: Bearer " + fApiKey;
		headers = curl_slist_append(headers, auth.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, img_collect_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	if (url.rfind("https://", 0) == 0) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);
		curl_easy_setopt(curl, CURLOPT_CAINFO,
			"/system/data/ssl/CARootCertificates.pem");
		curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, (curl_off_t)50 * 1024 * 1024);
		// Certificate pinning for known providers
		size_t proto = url.find("://");
		if (proto != std::string::npos) {
			size_t start = proto + 3;
			size_t end = url.find_first_of("/?#", start);
			std::string host = url.substr(start, end != std::string::npos ? end - start : std::string::npos);
			if (host == "api.openai.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.openrouter.com" || host == "openrouter.ai") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "generativelanguage.googleapis.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.anthropic.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.x.ai") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.brave.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.tavily.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			}
		}
	}

	// Notify user that generation is in progress.
	{
		StreamEvent ev;
		ev.type = StreamEvent::TEXT_DELTA;
		ev.text = "Generating image…\n";
		cb(ev);
	}

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (rc != CURLE_OK) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = std::string("Image generation failed: ")
			+ curl_easy_strerror(rc);
		cb(err);
		return;
	}

	if (http_code != 200) {
		// Try to extract error message.
		std::string detail = response;
		try {
			json::Value errBody = json::Value::parse(response);
			if (errBody.is_arr() && !errBody.as_arr().empty())
				errBody = errBody.as_arr()[0];
			const json::Value* ej = errBody.find("error");
			if (ej && ej->is_obj()) {
				std::string msg = ej->str("message");
				if (!msg.empty()) detail = msg;
			}
		} catch (const std::exception&) {}
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "HTTP " + std::to_string(http_code) + ": " + detail;
		cb(err);
		return;
	}

	// Parse response: {"data": [{"b64_json": "...", "revised_prompt": "..."}]}
	try {
		json::Value root = json::Value::parse(response);
		const json::Value* data = root.find("data");
		if (data && data->is_arr()) {
			for (const auto& img : data->as_arr()) {
				std::string b64 = img.str("b64_json");
				std::string revised = img.str("revised_prompt");

				if (!revised.empty()) {
					StreamEvent ev;
					ev.type = StreamEvent::TEXT_DELTA;
					ev.text = revised + "\n";
					cb(ev);
				}

				if (!b64.empty()) {
					StreamEvent ev;
					ev.type = StreamEvent::IMAGE_BLOCK;
					ev.image_media_type = "image/png";
					ev.image_data = std::move(b64);
					cb(ev);
				}
			}
		}
	} catch (const std::exception&) {
		StreamEvent err;
		err.type = StreamEvent::ERROR;
		err.error = "Failed to parse image generation response.";
		cb(err);
		return;
	}

	// Emit end.
	StreamEvent end;
	end.type = StreamEvent::MESSAGE_END;
	end.stop_reason = "end_turn";
	cb(end);
}

void OpenAICompatProvider::stream(const CompletionRequest& req,
                                   StreamCallback cb) {
	// Route image generation models to the dedicated endpoint.
	if (is_image_model(req.model)) {
		_StreamImageGen(req, cb);
		return;
	}

	std::string body;
	bool tools_stripped = false;
	try {
		body = build_oai_body(req);
	} catch (const std::exception& e) {
		StreamEvent err;
		err.type  = StreamEvent::ERROR;
		err.error = std::string("build_oai_body: ") + e.what();
		cb(err);
		return;
	}

	const std::string url = fBaseUrl + "/v1/chat/completions";
	// OpenAI-compat: 3 retries with short backoff. Local servers (Ollama,
	// LM Studio) rarely 429; connection errors resolve quickly or not at all.
	const int kMaxAttempts = 3;

	for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
		CURL* curl = curl_easy_init();
		if (!curl) {
			StreamEvent err;
			err.type  = StreamEvent::ERROR;
			err.error = "curl_easy_init failed";
			cb(err);
			return;
		}
		CurlGuard curl_guard(curl);

		OaiState st;
		st.cb = cb;

		HeadersGuard hg2;
		struct curl_slist*& headers = hg2.headers;
		if (!fApiKey.empty()) {
			std::string auth = "Authorization: Bearer " + fApiKey;
			headers = curl_slist_append(headers, auth.c_str());
		}
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "Accept: text/event-stream");
		// OpenRouter requires HTTP-Referer and X-Title for billing/ranking.
		// These are harmless on other providers (ignored if not recognized).
		headers = curl_slist_append(headers, "HTTP-Referer: https://codeberg.org/atomozero/Pluto");
		headers = curl_slist_append(headers, "X-Title: Pluto");

curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, oai_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, oai_xferinfo);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, req.cancel);
	// Enable SSL verification for HTTPS URLs
	if (url.rfind("https://", 0) == 0) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);
		curl_easy_setopt(curl, CURLOPT_CAINFO,
			"/system/data/ssl/CARootCertificates.pem");
		curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, (curl_off_t)50 * 1024 * 1024);
		// Certificate pinning for known providers
		size_t proto = url.find("://");
		if (proto != std::string::npos) {
			size_t start = proto + 3;
			size_t end = url.find_first_of("/?#", start);
			std::string host = url.substr(start, end != std::string::npos ? end - start : std::string::npos);
			if (host == "api.openai.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.openrouter.com" || host == "openrouter.ai") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "generativelanguage.googleapis.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.anthropic.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.x.ai") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.brave.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			} else if (host == "api.tavily.com") {
				curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, "sha256/REPLACE_WITH_ACTUAL_PIN");
			}
		}
	}

		CURLcode rc = curl_easy_perform(curl);
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (rc != CURLE_OK) {
			if (rc == CURLE_ABORTED_BY_CALLBACK) {
				StreamEvent err;
				err.type  = StreamEvent::ERROR;
				err.error = "cancelled";
				cb(err);
				return;
			}
			bool pre = (http_code == 0);
			bool conn = rc == CURLE_COULDNT_CONNECT
			         || rc == CURLE_COULDNT_RESOLVE_HOST
			         || rc == CURLE_OPERATION_TIMEDOUT;
			if (pre && conn && attempt < kMaxAttempts) {
				int64_t d = 1000000LL * (1LL << (attempt - 1));
				StreamEvent info;
				info.type = StreamEvent::RETRY_INFO;
				info.text = std::string(curl_easy_strerror(rc))
					+ ", retrying in " + std::to_string(d / 1000000) + "s…";
				cb(info);
#ifdef __HAIKU__
				snooze(d);
#else
				usleep(static_cast<useconds_t>(d));
#endif
				continue;
			}
			StreamEvent err;
			err.type  = StreamEvent::ERROR;
			err.error = std::string("curl: ") + curl_easy_strerror(rc);
			cb(err);
			return;
		}

		// For non-200 responses the server may return a plain JSON
		// error body (not SSE).  Capture whatever is left in the
		// buffer so the error message is meaningful.
		std::string residual = st.buffer;
		if (!st.event_data.empty()) {
			if (!residual.empty()) residual += '\n';
			residual += st.event_data;
		}

		if (http_code == 200) {
			if (!st.done) {
				end_tool_call(st);
				StreamEvent ev;
				ev.type = StreamEvent::MESSAGE_END;
				ev.stop_reason  = "end_turn";
				ev.input_tokens  = st.prompt_tokens;
				ev.output_tokens = st.completion_tokens;
				cb(ev);
			}
			return;
		}

		bool retryable = (http_code == 429 || http_code >= 500);
		if (retryable && attempt < kMaxAttempts) {
			int64_t d = 1000000LL * (1LL << (attempt - 1));
			StreamEvent info;
			info.type = StreamEvent::RETRY_INFO;
			info.text = "HTTP " + std::to_string(http_code)
				+ ", retrying in " + std::to_string(d / 1000000) + "s…";
			cb(info);
#ifdef __HAIKU__
			snooze(d);
#else
			usleep(static_cast<useconds_t>(d));
#endif
			continue;
		}

		// Try to extract a human-readable message from a JSON error body.
		// Prefer raw_body (the complete HTTP response) over residual
		// (leftover SSE buffer), which may contain only fragments.
		std::string detail = st.raw_body.empty() ? residual : st.raw_body;
		try {
			json::Value errBody = json::Value::parse(detail);
			// Gemini wraps errors in an array: [{...}]
			if (errBody.is_arr() && !errBody.as_arr().empty())
				errBody = errBody.as_arr()[0];
			const json::Value* ej = errBody.find("error");
			if (ej && ej->is_obj()) {
				std::string msg = ej->str("message");
				if (!msg.empty()) detail = msg;
			}
		} catch (const std::exception&) {}

		// If we get HTTP 400 and the request had tools, retry without
		// them — the model likely doesn't support tool use. This is
		// safe because 400 means the request was malformed, and
		// stripping tools is the most likely fix.
		if (http_code == 400 && !tools_stripped
		    && !req.tools.empty()) {
			CompletionRequest stripped = req;
			stripped.tools.clear();
			try {
				body = build_oai_body(stripped);
			} catch (const std::exception& e) {
				StreamEvent err;
				err.type = StreamEvent::ERROR;
				err.error = std::string("rebuild request failed: ") + e.what();
				cb(err);
				return;
			}
			tools_stripped = true;
			StreamEvent info;
			info.type = StreamEvent::RETRY_INFO;
			info.text = "model does not support tools — retrying without";
			cb(info);
			continue;
		}

		StreamEvent err;
		err.type  = StreamEvent::ERROR;
		err.error = "HTTP " + std::to_string(http_code) + ": " + detail;
		cb(err);
		return;
	}
}

}  // namespace pluto
