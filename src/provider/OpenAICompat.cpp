// OpenAI-compatible streaming provider.
// Maps between pluto's Anthropic-shaped ContentBlock model and the
// OpenAI /v1/chat/completions SSE format.

#include "OpenAICompat.h"
#include "../version.h"
#include "curl_security.h"

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
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>

#include "../cancel.h"
#include "../json.h"
#include "../log.h"
#include "CurlUtil.h"

namespace pluto {

namespace {

// OpenAI reasoning models (o1, o3, o4 families) require
// max_completion_tokens instead of max_tokens and reject the latter
// with HTTP 400. They also don't support stream_options.
bool is_reasoning_model(const std::string& model) {
    if (model.size() >= 2 && model[0] == 'o' && model[1] >= '0' && model[1] <= '9')
        return true;
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

    if (!reasoning) {
        Value so;
        so.set("include_usage", Value(true));
        root.set("stream_options", so);
    }

    Array msgs;

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
                    tcs.push_back(tc);
                }
                mv.set("tool_calls", tcs);
            }
            msgs.push_back(mv);
            continue;
        }

        if (m.role == "user") {
            if (has_tool_result) {
                for (const auto& b : m.content) {
                    if (b.type != ContentBlock::TOOL_RESULT) continue;
                    Value tr;
                    tr.set("role", std::string("tool"));
                    tr.set("tool_call_id", b.tool_id);
                    tr.set("content", b.text);
                    msgs.push_back(tr);
                }
                continue;
            }

            std::string text;
            for (const auto& b : m.content)
                if (b.type == ContentBlock::TEXT) text += b.text;
            if (!text.empty()) {
                Value mv;
                mv.set("role", std::string("user"));
                mv.set("content", text);
                msgs.push_back(mv);
            }
            continue;
        }
    }

    root.set("messages", Value(std::move(msgs)));

    if (!req.tools.empty()) {
        Array tools;
        for (const auto& t : req.tools) {
            Value tv;
            tv.set("type", std::string("function"));
            Value fn;
            fn.set("name", t.name);
            fn.set("description", t.description);
            Value schema(Object{});
            if (!t.input_schema_json.empty()) {
                try { schema = Value::parse(t.input_schema_json); }
                catch (const std::exception&) { schema = Value(Object{}); }
            }
            fn.set("parameters", schema);
            tv.set("function", fn);
            tools.push_back(tv);
        }
        root.set("tools", Value(std::move(tools)));
        root.set("tool_choice", std::string("auto"));
    }

    return root.serialize();
}

struct OaiStreamState {
    std::string buffer;
    StreamCallback cb;
    bool done = false;
    bool cb_error = false;
    
    std::string current_tool_id;
    std::string current_tool_name;
    std::string current_tool_args;
    int current_tool_index = -1;
    
    int input_tokens = 0;
    int output_tokens = 0;
    
    enum { IDLE, IN_TEXT, IN_TOOL } state = IDLE;
};

static void oai_emit_text(OaiStreamState& st, const std::string& text) {
    if (!text.empty()) {
        StreamEvent ev;
        ev.type = StreamEvent::TEXT_DELTA;
        ev.text = text;
        try { st.cb(ev); } catch (...) { st.cb_error = true; }
    }
}

static void oai_emit_tool_start(OaiStreamState& st,
                                const std::string& id, const std::string& name, int index) {
    StreamEvent ev;
    ev.type = StreamEvent::TOOL_USE_START;
    ev.tool_id = id;
    ev.tool_name = name;
    ev.tool_index = index;
    try { st.cb(ev); } catch (...) { st.cb_error = true; }
}

static void oai_emit_tool_delta(OaiStreamState& st, const std::string& delta, int index) {
    if (!delta.empty()) {
        StreamEvent ev;
        ev.type = StreamEvent::TOOL_USE_INPUT_DELTA;
        ev.tool_input = delta;
        ev.tool_index = index;
        try { st.cb(ev); } catch (...) { st.cb_error = true; }
    }
}

static void oai_emit_tool_end(OaiStreamState& st, const std::string& full_json, int index) {
    StreamEvent ev;
    ev.type = StreamEvent::TOOL_USE_END;
    ev.tool_input = full_json;
    ev.tool_index = index;
    try { st.cb(ev); } catch (...) { st.cb_error = true; }
}

static void oai_emit_message_end(OaiStreamState& st, const std::string& stop_reason) {
    StreamEvent ev;
    ev.type = StreamEvent::MESSAGE_END;
    ev.stop_reason = stop_reason;
    ev.input_tokens = st.input_tokens;
    ev.output_tokens = st.output_tokens;
    try { st.cb(ev); } catch (...) { st.cb_error = true; }
}

static void oai_process_line(const std::string& line, OaiStreamState& st) {
    if (line.empty() || line[0] != 'd' || line.substr(0, 5) != "data:") return;
    
    std::string data = line.substr(5);
    if (data == "[DONE]") {
        st.done = true;
        return;
    }
    
    try {
        json::Value v = json::Value::parse(data);
        const json::Value* choices = v.find("choices");
        if (!choices || !choices->is_arr() || choices->as_arr().empty()) return;
        
        const json::Value& choice = choices->as_arr()[0];
        
        const json::Value* u = v.find("usage");
        if (u) {
            const json::Value* prompt_tokens = u->find("prompt_tokens");
            const json::Value* completion_tokens = u->find("completion_tokens");
            if (prompt_tokens) st.input_tokens = static_cast<int>(prompt_tokens->as_num());
            if (completion_tokens) st.output_tokens = static_cast<int>(completion_tokens->as_num());
        }
        
        const json::Value* delta = choice.find("delta");
        if (delta) {
            const json::Value* content = delta->find("content");
            if (content && !content->is_null()) {
                std::string text = content->as_str();
                if (!text.empty()) {
                    if (st.state != OaiStreamState::IN_TEXT) {
                        if (st.state == OaiStreamState::IN_TOOL) {
                            oai_emit_tool_end(st, st.current_tool_args, st.current_tool_index);
                            st.current_tool_args.clear();
                        }
                        st.state = OaiStreamState::IN_TEXT;
                    }
                    oai_emit_text(st, text);
                }
            }
            
            const json::Value* tool_calls = delta->find("tool_calls");
            if (tool_calls && tool_calls->is_arr()) {
                const json::Array& tcalls = tool_calls->as_arr();
                for (const auto& tc : tcalls) {
                    const json::Value* index_val = tc.find("index");
                    int index = index_val ? static_cast<int>(index_val->as_num()) : 0;
                    
                    const json::Value* id_val = tc.find("id");
                    if (id_val && !id_val->is_null()) {
                        if (st.state == OaiStreamState::IN_TOOL) {
                            oai_emit_tool_end(st, st.current_tool_args, st.current_tool_index);
                        }
                        st.current_tool_id = id_val->as_str();
                        st.current_tool_args.clear();
                        st.current_tool_index = index;
                        st.state = OaiStreamState::IN_TOOL;
                    }
                    
                    const json::Value* function = tc.find("function");
                    if (function) {
                        const json::Value* name_val = function->find("name");
                        if (name_val && !name_val->is_null()) {
                            st.current_tool_name = name_val->as_str();
                            oai_emit_tool_start(st, st.current_tool_id, st.current_tool_name, index);
                        }
                        
                        const json::Value* arguments = function->find("arguments");
                        if (arguments && !arguments->is_null()) {
                            std::string args_delta = arguments->as_str();
                            st.current_tool_args += args_delta;
                            oai_emit_tool_delta(st, args_delta, index);
                        }
                    }
                }
            }
        }
        
        const json::Value* fr_val = choice.find("finish_reason");
        if (fr_val && !fr_val->is_null()) {
            std::string fr = fr_val->as_str();
            if (fr != "null") {
                if (st.state == OaiStreamState::IN_TOOL) {
                    oai_emit_tool_end(st, st.current_tool_args, st.current_tool_index);
                }
                oai_emit_message_end(st, fr);
            }
        }
    } catch (...) {
        // Ignore parse errors in streaming
    }
}

static size_t oai_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    OaiStreamState* st = static_cast<OaiStreamState*>(userdata);
    if (!st || st->done) return 0;
    
    size_t total = size * nmemb;
    try {
        st->buffer.append(ptr, total);
    } catch (...) {
        return 0;
    }
    
    size_t pos = 0;
    while ((pos = st->buffer.find('\n')) != std::string::npos) {
        std::string line = st->buffer.substr(0, pos);
        st->buffer.erase(0, pos + 1);
        
        if (!line.empty() && line.back() == '\r') line.pop_back();
        try {
            oai_process_line(line, *st);
        } catch (...) {
            st->cb_error = true;
        }
        if (st->cb_error) return 0;
    }
    return total;
}

}  // namespace

void OpenAICompatProvider::stream(const CompletionRequest& req, StreamCallback cb) {
    std::string base_url = fBaseUrl;
    if (base_url.empty()) base_url = "https://api.openai.com/v1/chat/completions";
    if (base_url.find("/chat/completions") == std::string::npos) {
        if (base_url.back() == '/') base_url.pop_back();
        base_url += "/chat/completions";
    }
    
    std::string body;
    try {
        body = build_oai_body(req);
    } catch (const std::exception& e) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = std::string("build request failed: ") + e.what();
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
    headers.append("Accept: text/event-stream");
    headers.append(std::string("User-Agent: Pluto/") + PLUTO_VERSION);

    if (!fApiKey.empty()) {
        headers.append(std::string("Authorization: Bearer ") + fApiKey);
    }

    OaiStreamState st;
    st.cb = std::move(cb);

    curl_easy_setopt(curl, CURLOPT_URL, base_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, oai_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);

    ApplyCurlSecurityWithPinning(curl, base_url);

    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
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

    bool tools_stripped = false;
    CompletionRequest current_req = req;
    std::string current_body = body;

    for (int attempt = 1; attempt <= 4; ++attempt) {
        st.buffer.clear();
        st.done = false;
        st.cb_error = false;
        st.state = OaiStreamState::IDLE;
        st.current_tool_args.clear();
        st.current_tool_id.clear();
        st.current_tool_name.clear();
        st.current_tool_index = -1;

        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = "Request cancelled";
            try { cb(err); } catch (...) {}
            return;
        }

        if (res != CURLE_OK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = std::string("curl: ") + curl_easy_strerror(res);
            if (attempt < 4) {
                err.type = StreamEvent::RETRY_INFO;
                err.text = "Network error: " + std::string(curl_easy_strerror(res)) + ", retrying...";
                try { cb(err); } catch (...) {}
                sleep_ms(calc_backoff_ms(attempt), req.cancel);
                continue;
            }
            try { cb(err); } catch (...) {}
            return;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            if (!st.done) {
                try { oai_emit_message_end(st, "stop"); } catch (...) {}
            }
            return;
        }

        if (http_code == 400 && !tools_stripped && !current_req.tools.empty()) {
            tools_stripped = true;
            current_req.tools.clear();
            try {
                current_body = build_oai_body(current_req);
            } catch (const std::exception& e) {
                StreamEvent err;
                err.type = StreamEvent::ERROR;
                err.error = std::string("rebuild request failed: ") + e.what();
                try { cb(err); } catch (...) {}
                return;
            }
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, current_body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(current_body.size()));
            
            StreamEvent info;
            info.type = StreamEvent::RETRY_INFO;
            info.text = "model does not support tools — retrying without";
            try { cb(info); } catch (...) {}
            continue;
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

            log_warn("openai-compat", "HTTP %ld (retry-after=%ld), body: %.512s",
                http_code, retry_secs, st.buffer.c_str());
            StreamEvent info;
            info.type = StreamEvent::RETRY_INFO;
            info.text = "HTTP " + std::to_string(http_code)
                + ", retrying in " + std::to_string(retry_secs) + "s...";
            try { cb(info); } catch (...) {}
            sleep_ms(retry_secs * 1000, req.cancel);
            continue;
        }

        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "HTTP " + std::to_string(http_code) + ": " + st.buffer;
        try { cb(err); } catch (...) {}
        return;
    }
}

OpenAICompatProvider::OpenAICompatProvider(std::string api_key, std::string base_url)
    : fApiKey(std::move(api_key)), fBaseUrl(std::move(base_url)) {}

bool OpenAICompatProvider::validate_config(std::string& error_out) const {
    if (fApiKey.empty()) {
        error_out = "API key not set";
        return false;
    }
    return true;
}

ProviderCapabilities OpenAICompatProvider::capabilities() const {
    ProviderCapabilities caps;
    caps.streaming = true;
    caps.tools = true;
    caps.vision = true;
    caps.reasoning = false;
    caps.system_prompt = true;
    caps.cache_control = false;
    caps.max_context = 128000;
    return caps;
}

std::vector<ModelInfo> OpenAICompatProvider::list_models() const {
    return {
        {"gpt-4o", "GPT-4o", "OpenAI", 4096, 128000, true, true, true, false, 5.0, 15.0},
        {"gpt-4o-mini", "GPT-4o mini", "OpenAI", 4096, 128000, true, true, true, false, 0.15, 0.6},
        {"gpt-4-turbo", "GPT-4 Turbo", "OpenAI", 4096, 128000, true, true, true, false, 10.0, 30.0},
    };
}

bool OpenAICompatProvider::supports_model(const std::string& model) const {
    static const std::vector<std::string> known = {
        "gpt-4", "gpt-3.5", "o1", "o3", "o4"
    };
    for (const auto& m : known) {
        if (model.find(m) != std::string::npos) return true;
    }
    return true;
}

double OpenAICompatProvider::estimate_cost(const std::string& model, int input_tokens, int output_tokens) const {
    double in_cost = 0.0, out_cost = 0.0;
    if (model.find("gpt-4o-mini") != std::string::npos) {
        in_cost = 0.15; out_cost = 0.6;
    } else if (model.find("gpt-4o") != std::string::npos) {
        in_cost = 5.0; out_cost = 15.0;
    } else if (model.find("gpt-4") != std::string::npos) {
        in_cost = 10.0; out_cost = 30.0;
    } else {
        in_cost = 0.0; out_cost = 0.0;
    }
    return (input_tokens / 1000000.0) * in_cost + (output_tokens / 1000000.0) * out_cost;
}

}  // namespace pluto
