#include "Anthropic.h"
#include "../version.h"
#include "curl_security.h"

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
#include <cstdlib>
#include <ctime>

#include "../cancel.h"
#include "../json.h"
#include "../log.h"
#include "CurlUtil.h"

namespace pluto {

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
    int tool_index = -1;  // sequential index for robust matching

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
                    + " (" + std::to_string(kb) + " KB approx)]");
            }
            if (bv.is_obj()) blocks.push_back(std::move(bv));
        }
        mv.set("content", Value(std::move(blocks)));
        msgs.push_back(std::move(mv));
    }
    root.set("messages", Value(std::move(msgs)));

    return root.serialize();
}

static void emit_text_delta(StreamState& st, const std::string& text) {
    if (!text.empty()) {
        StreamEvent ev;
        ev.type = StreamEvent::TEXT_DELTA;
        ev.text = text;
        st.cb(ev);
    }
}

static void emit_tool_start(StreamState& st, const std::string& id,
                            const std::string& name, int index) {
    StreamEvent ev;
    ev.type = StreamEvent::TOOL_USE_START;
    ev.tool_id = id;
    ev.tool_name = name;
    ev.tool_index = index;
    st.cb(ev);
}

static void emit_tool_input_delta(StreamState& st, const std::string& delta, int index) {
    if (!delta.empty()) {
        StreamEvent ev;
        ev.type = StreamEvent::TOOL_USE_INPUT_DELTA;
        ev.tool_input = delta;
        ev.tool_index = index;
        st.cb(ev);
    }
}

static void emit_tool_end(StreamState& st, const std::string& full_json, int index) {
    StreamEvent ev;
    ev.type = StreamEvent::TOOL_USE_END;
    ev.tool_input = full_json;
    ev.tool_index = index;
    st.cb(ev);
}

static void emit_message_end(StreamState& st, const std::string& stop_reason) {
    StreamEvent ev;
    ev.type = StreamEvent::MESSAGE_END;
    ev.stop_reason = stop_reason;
    ev.input_tokens = st.input_tokens;
    ev.output_tokens = st.output_tokens;
    ev.cache_read_tokens = st.cache_read;
    ev.cache_creation_tokens = st.cache_creation;
    st.cb(ev);
}

static void process_sse_line(const std::string& line, StreamState& st) {
    if (line.empty() || line[0] != 'd' || line.substr(0, 5) != "data:") return;
    
    std::string data = line.substr(5);
    if (data == "[DONE]") {
        st.done = true;
        return;
    }
    
    try {
        json::Value v = json::Value::parse(data);
        const json::Value* type_val = v.find("type");
        std::string type = type_val ? type_val->as_str() : "";
        
        if (type == "message_start") {
            const json::Value* msg = v.find("message");
            if (msg && msg->find("usage")) {
                const json::Value* u = msg->find("usage");
                if (u) {
                    const json::Value* input_tokens = u->find("input_tokens");
                    const json::Value* output_tokens = u->find("output_tokens");
                    if (input_tokens) st.input_tokens = static_cast<int>(input_tokens->as_num());
                    if (output_tokens) st.output_tokens = static_cast<int>(output_tokens->as_num());
                    const json::Value* cache_read = u->find("cache_read_input_tokens");
                    if (cache_read) st.cache_read = static_cast<int>(cache_read->as_num());
                    const json::Value* cache_creation = u->find("cache_creation_input_tokens");
                    if (cache_creation) st.cache_creation = static_cast<int>(cache_creation->as_num());
                }
            }
        }
        else if (type == "content_block_start") {
            const json::Value* index_val = v.find("index");
            int index = index_val ? static_cast<int>(index_val->as_num()) : 0;
            const json::Value* block = v.find("content_block");
            std::string block_type = block && block->find("type") ? block->find("type")->as_str() : "";
            
            if (block_type == "text") {
                st.block_type = StreamState::TEXT_BLOCK;
                st.tool_index = -1;
            }
            else if (block_type == "tool_use") {
                st.block_type = StreamState::TOOL_USE_BLOCK;
                st.tool_id = block && block->find("id") ? block->find("id")->as_str() : "";
                st.tool_name = block && block->find("name") ? block->find("name")->as_str() : "";
                st.tool_input.clear();
                st.tool_index = index;
                emit_tool_start(st, st.tool_id, st.tool_name, index);
            }
        }
        else if (type == "content_block_delta") {
            const json::Value* index_val = v.find("index");
            int index = index_val ? static_cast<int>(index_val->as_num()) : 0;
            const json::Value* delta = v.find("delta");
            std::string delta_type = delta && delta->find("type") ? delta->find("type")->as_str() : "";
            
            if (delta_type == "text_delta" && st.block_type == StreamState::TEXT_BLOCK) {
                const json::Value* text_val = delta ? delta->find("text") : nullptr;
                emit_text_delta(st, text_val ? text_val->as_str() : "");
            }
            else if (delta_type == "input_json_delta" && st.block_type == StreamState::TOOL_USE_BLOCK) {
                const json::Value* partial_json = delta ? delta->find("partial_json") : nullptr;
                st.tool_input += partial_json ? partial_json->as_str() : "";
                emit_tool_input_delta(st, partial_json ? partial_json->as_str() : "", index);
            }
        }
        else if (type == "content_block_stop") {
            if (st.block_type == StreamState::TOOL_USE_BLOCK) {
                emit_tool_end(st, st.tool_input, st.tool_index);
            }
            st.block_type = StreamState::NONE;
            st.tool_id.clear();
            st.tool_name.clear();
            st.tool_input.clear();
            st.tool_index = -1;
        }
        else if (type == "message_delta") {
            const json::Value* delta = v.find("delta");
            if (delta && delta->find("stop_reason")) {
                const json::Value* sr_val = delta->find("stop_reason");
                std::string sr = sr_val ? sr_val->as_str() : "";
                if (!sr.empty()) {
                    // store for message_end
                }
            }
            if (delta && delta->find("usage")) {
                const json::Value* u = delta->find("usage");
                if (u) {
                    const json::Value* output_tokens = u->find("output_tokens");
                    if (output_tokens) st.output_tokens = static_cast<int>(output_tokens->as_num());
                }
            }
        }
        else if (type == "message_stop") {
            // Final usage may be here
        }
    } catch (...) {
        // Ignore parse errors in streaming
    }
}

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    StreamState* st = static_cast<StreamState*>(userdata);
    if (!st || st->done) return 0;
    
    size_t total = size * nmemb;
    st->buffer.append(ptr, total);
    
    size_t pos = 0;
    while ((pos = st->buffer.find('\n')) != std::string::npos) {
        std::string line = st->buffer.substr(0, pos);
        st->buffer.erase(0, pos + 1);
        
        if (!line.empty() && line.back() == '\r') line.pop_back();
        process_sse_line(line, *st);
        if (st->cb_error) return 0;
    }
    return total;
}

void AnthropicProvider::stream(const CompletionRequest& req, StreamCallback cb) {
    std::string body;
    try {
        body = build_request_body(req);
    } catch (const std::exception& e) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = std::string("build request failed: ") + e.what();
        cb(err);
        return;
    }

    const char* api_key = fToken.empty() ? getenv("ANTHROPIC_API_KEY") : fToken.c_str();
    if (!api_key || !*api_key) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "ANTHROPIC_API_KEY not set";
        cb(err);
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "curl_easy_init failed";
        cb(err);
        return;
    }
    CurlGuard guard(curl);

    HeadersGuard headers;
    headers.append("Content-Type: application/json");
    
    if (fAuthType == AuthType::OAUTH) {
        headers.append(std::string("Authorization: Bearer ") + api_key);
    } else {
        headers.append(std::string("x-api-key: ") + api_key);
    }
    headers.append("anthropic-version: 2023-06-01");
    headers.append("anthropic-beta: prompt-caching-2024-07-31");
    headers.append("Accept: text/event-stream");
    headers.append(std::string("User-Agent: Pluto/") + PLUTO_VERSION);

    std::string url = fBaseUrl;
    if (url.empty()) url = "https://api.anthropic.com";
    if (url.back() == '/') url.pop_back();
    url += "/v1/messages";

    StreamState st;
    st.cb = std::move(cb);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);

    // Apply security with pinning (uses correct pins from curl_security.h)
    // Apply security with pinning (uses correct pins from curl_security.h)
    ApplyCurlSecurityWithPinning(curl, url);

    // Override timeouts for long streaming responses
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    if (req.cancel) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            [](void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                auto cancel = static_cast<std::atomic<bool>*>(clientp);
                return cancel && cancel->load() ? 1 : 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(req.cancel));
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    for (int attempt = 1; attempt <= 3; ++attempt) {
        st.buffer.clear();
        st.done = false;
        st.cb_error = false;

        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = "Request cancelled";
            cb(err);
            return;
        }

        if (res != CURLE_OK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = std::string("curl: ") + curl_easy_strerror(res);
            if (attempt < 3) {
                err.type = StreamEvent::RETRY_INFO;
                err.text = "Network error: " + std::string(curl_easy_strerror(res)) + ", retrying...";
                cb(err);
                sleep_ms(calc_backoff_ms(attempt), req.cancel);
                continue;
            }
            cb(err);
            return;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            if (!st.done) emit_message_end(st, "end_turn");
            return;
        }

        if (http_code == 429 || (http_code >= 500 && http_code < 600)) {
            std::string retry_after_hdr;
            curl_easy_getinfo(curl, CURLINFO_RETRY_AFTER, &retry_after_hdr);
            
            long retry_secs = -1;
            if (!retry_after_hdr.empty()) {
                retry_secs = parse_retry_after(retry_after_hdr);
            }
            if (retry_secs < 0) {
                retry_secs = calc_backoff_ms(attempt) / 1000;
                if (retry_secs < 1) retry_secs = 1;
            }
            if (retry_secs > 60) retry_secs = 60;

            log_warn("anthropic", "HTTP %ld (retry-after=%ld), body: %.512s",
                http_code, retry_secs, st.buffer.c_str());
            StreamEvent info;
            info.type = StreamEvent::RETRY_INFO;
            info.text = "HTTP " + std::to_string(http_code)
                + ", retrying in " + std::to_string(retry_secs) + "s...";
            cb(info);
            sleep_ms(retry_secs * 1000, req.cancel);
            continue;
        }

        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "HTTP " + std::to_string(http_code) + ": " + st.buffer;
        cb(err);
        return;
    }
}

AnthropicProvider::AnthropicProvider(std::string token, AuthType auth_type, std::string base_url)
    : fToken(std::move(token)), fAuthType(auth_type), fBaseUrl(std::move(base_url)) {}

bool AnthropicProvider::validate_config(std::string& error_out) const {
    if (fToken.empty() && getenv("ANTHROPIC_API_KEY") == nullptr) {
        error_out = "ANTHROPIC_API_KEY not set";
        return false;
    }
    return true;
}

ProviderCapabilities AnthropicProvider::capabilities() const {
    ProviderCapabilities caps;
    caps.streaming = true;
    caps.tools = true;
    caps.vision = true;
    caps.reasoning = false;
    caps.system_prompt = true;
    caps.cache_control = true;
    caps.max_context = 200000;
    return caps;
}

std::vector<ModelInfo> AnthropicProvider::list_models() const {
    return {
        {"claude-3-5-sonnet-20241022", "Claude 3.5 Sonnet", "Anthropic", 8192, 200000, true, true, true, false, 3.0, 15.0},
        {"claude-3-5-haiku-20241022", "Claude 3.5 Haiku", "Anthropic", 8192, 200000, true, true, true, false, 0.8, 4.0},
        {"claude-3-opus-20240229", "Claude 3 Opus", "Anthropic", 4096, 200000, true, true, true, false, 15.0, 75.0},
    };
}

bool AnthropicProvider::supports_model(const std::string& model) const {
    static const std::vector<std::string> known = {
        "claude-3-5-sonnet", "claude-3-5-haiku", "claude-3-opus",
        "claude-3-sonnet", "claude-3-haiku"
    };
    for (const auto& m : known) {
        if (model.find(m) != std::string::npos) return true;
    }
    return false;
}

double AnthropicProvider::estimate_cost(const std::string& model, int input_tokens, int output_tokens) const {
    double in_cost = 0.0, out_cost = 0.0;
    if (model.find("opus") != std::string::npos) {
        in_cost = 15.0; out_cost = 75.0;
    } else if (model.find("sonnet") != std::string::npos) {
        in_cost = 3.0; out_cost = 15.0;
    } else if (model.find("haiku") != std::string::npos) {
        in_cost = 0.8; out_cost = 4.0;
    }
    return (input_tokens / 1000000.0) * in_cost + (output_tokens / 1000000.0) * out_cost;
}

}  // namespace pluto
