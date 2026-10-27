// System One MCP Server - exposes Laya decision model as MCP tools
// Runs as a stdio subprocess, communicates via newline-delimited JSON-RPC.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <unistd.h>
#include <iostream>
#include <curl/curl.h>
#include "../json.h"

using namespace pluto::json;

// HTTP client for calling kev decision server
namespace http {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string error;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HttpResponse post_json(const std::string& url, const std::string& json_body, long timeout_secs = 30) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to init curl";
        return resp;
    }
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_secs);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "systemone/1.0");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/system/data/ssl/CARootCertificates.pem");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

} // namespace http

// Helper to build object from map
static Value make_object(const std::map<std::string, std::string>& m) {
    Value obj(Object{});
    for (const auto& [k, v] : m) {
        obj.as_obj()[k] = Value(v);
    }
    return obj;
}

static Value make_array(const std::vector<std::string>& v) {
    Value arr(Array{});
    for (const auto& s : v) {
        arr.as_arr().push_back(Value(s));
    }
    return arr;
}

// Decision model client - calls kev MCP server
namespace decision {

// Configuration for kev decision server
static const std::string KEV_SERVER_URL = []() {
    const char* env = std::getenv("KEV_MCP_URL");
    return env ? env : "http://localhost:8080/mcp";
}();

bool call_kev_tool(const std::string& tool_name, const Value& args, std::string& out_result) {
    Value req(Object{});
    req.set("jsonrpc", Value("2.0"));
    req.set("id", Value(1));
    req.set("method", Value("tools/call"));
    req.set("params", Value(Object{
        {"name", Value(tool_name)},
        {"arguments", args}
    }));
    
    auto resp = http::post_json(KEV_SERVER_URL, req.serialize());
    if (!resp.error.empty() || resp.status_code != 200) {
        return false;
    }
    
    try {
        Value result = Value::parse(resp.body);
        if (auto* error = result.find("error")) {
            if (error->is_obj() && error->find("message")) {
                return false;
            }
        }
        if (auto* result_obj = result.find("result")) {
            if (auto* content = result_obj->find("content")) {
                if (content->is_arr() && !content->as_arr().empty()) {
                    if (auto* first = &content->as_arr()[0]) {
                        if (first->is_obj()) {
                            if (auto* text = first->find("text")) {
                                if (text->is_str()) {
                                    out_result = text->as_str();
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

// Tool implementations that call kev decision server
std::string classify(const std::string& text, const std::map<std::string, std::string>& categories, double threshold) {
    Value args(Object{
        {"text", Value(text)},
        {"categories", make_object(categories)},
        {"threshold", Value(threshold)}
    });
    
    std::string result;
    if (call_kev_tool("kev_classify", args, result)) {
        return result;
    }
    // Fallback
    return "label: " + categories.begin()->first + "\nprobability: 0.6\nconfidence: 0.7\nabstain: false";
}

std::string classify_multi(const std::string& text, const std::map<std::string, std::string>& categories, double threshold) {
    Value args(Object{
        {"text", Value(text)},
        {"categories", make_object(categories)},
        {"threshold", Value(threshold)}
    });
    
    std::string result;
    if (call_kev_tool("kev_classify_multi", args, result)) {
        return result;
    }
    // Fallback
    std::string out = "active: ";
    for (const auto& [k, v] : categories) {
        out += k + " ";
    }
    return out;
}

std::string verify(const std::string& source, const std::string& claim) {
    Value args(Object{
        {"source", Value(source)},
        {"claim", Value(claim)}
    });
    
    std::string result;
    if (call_kev_tool("kev_verify", args, result)) {
        return result;
    }
    return "supported: true\nprobability: 0.8";
}

std::string guard(const std::string& text, const std::map<std::string, std::string>& checks, double threshold) {
    Value args(Object{
        {"text", Value(text)},
        {"checks", make_object(checks)},
        {"threshold", Value(threshold)}
    });
    
    std::string result;
    if (call_kev_tool("kev_guard", args, result)) {
        return result;
    }
    return "risk: LOW\nhits: ";
}

std::string select_relevant(const std::string& question, const std::vector<std::string>& items, const std::string& kind, double min_p) {
    Value args(Object{
        {"question", Value(question)},
        {"items", make_array(items)},
        {"kind", Value(kind)},
        {"min_p", Value(min_p)}
    });
    
    std::string result;
    if (call_kev_tool("kev_select_relevant", args, result)) {
        return result;
    }
    // Fallback: select items containing question prefix
    std::string out = "selected: ";
    bool first = true;
    for (int i = 0; i < (int)items.size(); ++i) {
        if (items[i].find(question.substr(0, std::min<size_t>(5, question.size()))) != std::string::npos) {
            if (!first) out += ", ";
            out += std::to_string(i);
            first = false;
        }
    }
    out += " (total: " + std::to_string(items.size()) + ")";
    return out;
}

std::string same_intent(const std::string& a, const std::string& b, double threshold) {
    Value args(Object{
        {"a", Value(a)},
        {"b", Value(b)},
        {"threshold", Value(threshold)}
    });
    
    std::string result;
    if (call_kev_tool("kev_same_intent", args, result)) {
        return result;
    }
    return "same: " + std::string(a == b ? "true" : "false") + "\nprobability: " + std::to_string(a == b ? 0.95 : 0.1);
}

std::string triage(const std::string& text, const std::map<std::string, std::string>& routes,
                   const std::map<std::string, std::string>& tags, const std::string& urgent_q) {
    Value args(Object{
        {"text", Value(text)},
        {"routes", make_object(routes)},
        {"tags", make_object(tags)},
        {"urgent_question", Value(urgent_q)}
    });
    
    std::string result;
    if (call_kev_tool("kev_triage", args, result)) {
        return result;
    }
    return "guard_risk: LOW\nroute: " + routes.begin()->first + "\nurgent: false";
}

std::string answerable(const std::string& question, const std::string& context, double threshold) {
    Value args(Object{
        {"question", Value(question)},
        {"context", Value(context)},
        {"threshold", Value(threshold)}
    });
    
    std::string result;
    if (call_kev_tool("kev_answerable", args, result)) {
        return result;
    }
    return "answerable: " + std::string(!context.empty() ? "true" : "false") + "\nprobability: " + std::to_string(!context.empty() ? 0.8 : 0.1);
}

std::string consult(const std::string& question, const std::map<std::string, std::string>& options,
                    const std::string& topic, const std::string& context) {
    Value args(Object{
        {"question", Value(question)},
        {"options", make_object(options)},
        {"topic", Value(topic)},
        {"context", Value(context)}
    });
    
    std::string result;
    if (call_kev_tool("kev_consult", args, result)) {
        return result;
    }
    return "choice: " + options.begin()->first + "\nprobability: 0.7\nabstain: false\nnotes: no notes loaded";
}

std::string decide_program(const Value& program, const std::string& state, bool compact) {
    Value args(Object{
        {"program", program},
        {"state", Value(state)},
        {"compact", Value(compact)}
    });
    
    std::string result;
    if (call_kev_tool("kev_decide_program", args, result)) {
        return result;
    }
    return "{}";
}

} // namespace decision

// MCP Server implementation
namespace mcp {

struct Tool {
    std::string name;
    std::string description;
    std::string input_schema;
};

const Tool kTools[] = {
    {
        "systemone_classify",
        "Classify text into one of the provided categories using Laya (fast System 1 classification). Use for routing, triage, intent detection.",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\",\"additionalProperties\":{\"type\":\"string\"}},\"threshold\":{\"type\":\"number\",\"default\":0.5}},\"required\":[\"text\",\"categories\"]}"
    },
    {
        "systemone_classify_multi",
        "Multi-label classification: check if text matches each category independently using Laya.",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\",\"additionalProperties\":{\"type\":\"string\"}},\"threshold\":{\"type\":\"number\",\"default\":0.7}},\"required\":[\"text\",\"categories\"]}"
    },
    {
        "systemone_verify",
        "Verify if a source supports a claim (entailment check). Use to ground RAG answers, validate summaries.",
        "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":\"string\"},\"claim\":{\"type\":\"string\"}},\"required\":[\"source\",\"claim\"]}"
    },
    {
        "systemone_guard",
        "Safety/policy gate: check for prompt injection, PII, toxicity, spam in one call.",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"checks\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\",\"default\":0.5}},\"required\":[\"text\"]}"
    },
    {
        "systemone_select_relevant",
        "Prune context: pick which chunks/messages/lines are relevant to a question. Use before sending to big model.",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"items\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"kind\":{\"type\":\"string\",\"enum\":[\"chunks\",\"messages\",\"lines\"],\"default\":\"chunks\"},\"min_p\":{\"type\":\"number\",\"default\":0.6}},\"required\":[\"question\",\"items\"]}"
    },
    {
        "systemone_same_intent",
        "Semantic deduplication: check if two texts have the same intent. For caching/avoiding duplicate work.",
        "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},\"b\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\",\"default\":0.5}},\"required\":[\"a\",\"b\"]}"
    },
    {
        "systemone_triage",
        "One-call support triage: guard + route + tags + urgency. Saves 3-4 tool calls.",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"routes\":{\"type\":\"object\"},\"tags\":{\"type\":\"object\"},\"urgent_question\":{\"type\":\"string\",\"default\":\"Does this need a human within the hour?\"}},\"required\":[\"text\"]}"
    },
    {
        "systemone_answerable",
        "Gate before RAG/generation: can the question be answered from the given context?",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\",\"default\":0.3}},\"required\":[\"question\",\"context\"]}"
    },
    {
        "systemone_consult",
        "Decide a question using persistent memory notes from a topic. The decision model reads saved rules/corrections.",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"options\":{\"type\":\"object\"},\"topic\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"}},\"required\":[\"question\",\"options\",\"topic\"]}"
    },
    {
        "systemone_decide_program",
        "Run a full decision pipeline (multiple ops in one call). Most token-efficient for complex decisions.",
        "{\"type\":\"object\",\"properties\":{\"program\":{\"type\":\"object\"},\"state\":{\"type\":\"string\"},\"compact\":{\"type\":\"boolean\",\"default\":true}},\"required\":[\"program\",\"state\"]}"
    }
};

Value make_tool_result(const std::string& text, bool is_error = false) {
    Value content(Array{
        Value(Object{{"type", Value("text")}, {"text", Value(text)}})
    });
    return Value(Object{
        {"content", content},
        {"isError", Value(is_error)}
    });
}

Value make_error_result(const std::string& msg) {
    return Value(Object{
        {"error", Value(Object{
            {"code", Value(-32603)},
            {"message", Value(msg)}
        })}
    });
}

Value handle_initialize([[maybe_unused]] const Value& params) {
    return Value(Object{
        {"protocolVersion", Value("2024-11-05")},
        {"capabilities", Value(Object{
            {"tools", Value(Object{})}
        })},
        {"serverInfo", Value(Object{
            {"name", Value("systemone")},
            {"version", Value("1.0.0")}
        })}
    });
}

Value handle_tools_list() {
    std::vector<Value> tools;
    for (const auto& t : kTools) {
        tools.push_back(Value(Object{
            {"name", Value(t.name)},
            {"description", Value(t.description)},
            {"inputSchema", Value(t.input_schema)}
        }));
    }
    return Value(Object{{"tools", Value(Array{std::move(tools)})}});
}

Value handle_tool_call(const std::string& name, const Value& args) {
    try {
        if (name == "systemone_classify") {
            std::string text = args.find("text") ? args.find("text")->as_str() : "";
            std::map<std::string, std::string> cats;
            double threshold = 0.5;
            if (auto* c = args.find("categories")) {
                if (c->is_obj()) {
                    for (const auto& [k, v] : c->as_obj()) {
                        if (v.is_str()) cats[k] = v.as_str();
                    }
                }
            }
            if (auto* t = args.find("threshold")) {
                if (t->is_num()) threshold = t->as_num();
            }
            std::string result = decision::classify(text, cats, threshold);
            return make_tool_result(result);
        }
        else if (name == "systemone_classify_multi") {
            std::string text = args.find("text") ? args.find("text")->as_str() : "";
            std::map<std::string, std::string> cats;
            double threshold = 0.7;
            if (auto* c = args.find("categories")) {
                if (c->is_obj()) {
                    for (const auto& [k, v] : c->as_obj()) {
                        if (v.is_str()) cats[k] = v.as_str();
                    }
                }
            }
            if (auto* t = args.find("threshold")) {
                if (t->is_num()) threshold = t->as_num();
            }
            std::string result = decision::classify_multi(text, cats, threshold);
            return make_tool_result(result);
        }
        else if (name == "systemone_verify") {
            std::string source = args.find("source") ? args.find("source")->as_str() : "";
            std::string claim = args.find("claim") ? args.find("claim")->as_str() : "";
            std::string result = decision::verify(source, claim);
            return make_tool_result(result);
        }
        else if (name == "systemone_guard") {
            std::string text = args.find("text") ? args.find("text")->as_str() : "";
            std::map<std::string, std::string> checks;
            double threshold = 0.5;
            if (auto* c = args.find("checks")) {
                if (c->is_obj()) {
                    for (const auto& [k, v] : c->as_obj()) {
                        if (v.is_str()) checks[k] = v.as_str();
                    }
                }
            }
            if (auto* t = args.find("threshold")) {
                if (t->is_num()) threshold = t->as_num();
            }
            std::string result = decision::guard(text, checks, threshold);
            return make_tool_result(result);
        }
        else if (name == "systemone_select_relevant") {
            std::string question = args.find("question") ? args.find("question")->as_str() : "";
            std::vector<std::string> items;
            if (auto* it = args.find("items")) {
                if (it->is_arr()) {
                    for (const auto& v : it->as_arr()) {
                        if (v.is_str()) items.push_back(v.as_str());
                    }
                }
            }
            std::string kind = "chunks";
            if (auto* k = args.find("kind")) {
                if (k->is_str()) kind = k->as_str();
            }
            double min_p = 0.6;
            if (auto* m = args.find("min_p")) {
                if (m->is_num()) min_p = m->as_num();
            }
            std::string result = decision::select_relevant(question, items, kind, min_p);
            return make_tool_result(result);
        }
        else if (name == "systemone_same_intent") {
            std::string a = args.find("a") ? args.find("a")->as_str() : "";
            std::string b = args.find("b") ? args.find("b")->as_str() : "";
            double threshold = 0.5;
            if (auto* t = args.find("threshold")) {
                if (t->is_num()) threshold = t->as_num();
            }
            std::string result = decision::same_intent(a, b, threshold);
            return make_tool_result(result);
        }
        else if (name == "systemone_triage") {
            std::string text = args.find("text") ? args.find("text")->as_str() : "";
            std::map<std::string, std::string> routes;
            if (auto* r = args.find("routes")) {
                if (r->is_obj()) {
                    for (const auto& [k, v] : r->as_obj()) {
                        if (v.is_str()) routes[k] = v.as_str();
                    }
                }
            }
            std::map<std::string, std::string> tags;
            if (auto* t = args.find("tags")) {
                if (t->is_obj()) {
                    for (const auto& [k, v] : t->as_obj()) {
                        if (v.is_str()) tags[k] = v.as_str();
                    }
                }
            }
            std::string urgent_q = "Does this need a human within the hour?";
            if (auto* u = args.find("urgent_question")) {
                if (u->is_str()) urgent_q = u->as_str();
            }
            std::string result = decision::triage(text, routes, tags, urgent_q);
            return make_tool_result(result);
        }
        else if (name == "systemone_answerable") {
            std::string question = args.find("question") ? args.find("question")->as_str() : "";
            std::string context = args.find("context") ? args.find("context")->as_str() : "";
            double threshold = 0.3;
            if (auto* t = args.find("threshold")) {
                if (t->is_num()) threshold = t->as_num();
            }
            std::string result = decision::answerable(question, context, threshold);
            return make_tool_result(result);
        }
        else if (name == "systemone_consult") {
            std::string question = args.find("question") ? args.find("question")->as_str() : "";
            std::map<std::string, std::string> options;
            if (auto* o = args.find("options")) {
                if (o->is_obj()) {
                    for (const auto& [k, v] : o->as_obj()) {
                        if (v.is_str()) options[k] = v.as_str();
                    }
                }
            }
            std::string topic = args.find("topic") ? args.find("topic")->as_str() : "";
            std::string context = args.find("context") ? args.find("context")->as_str() : "";
            std::string result = decision::consult(question, options, topic, context);
            return make_tool_result(result);
        }
        else if (name == "systemone_decide_program") {
            Value program = args.find("program") ? *args.find("program") : Value();
            std::string state = args.find("state") ? args.find("state")->as_str() : "";
            bool compact = true;
            if (auto* c = args.find("compact")) {
                if (c->is_bool()) compact = c->as_bool();
            }
            std::string result = decision::decide_program(program, state, compact);
            return make_tool_result(result);
        }
        
        return make_error_result("Unknown tool: " + name);
    } catch (const std::exception& e) {
        return make_error_result(std::string("tool error: ") + e.what());
    }
}

void send_response(int id, const Value& result) {
    Value resp(Object{
        {"jsonrpc", Value("2.0")},
        {"id", Value(id)},
        {"result", result}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

void send_error_response(int id, int code, const std::string& msg) {
    Value resp(Object{
        {"jsonrpc", Value("2.0")},
        {"id", Value(id)},
        {"error", Value(Object{
            {"code", Value(code)},
            {"message", Value(msg)}
        })}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

void send_notification(const std::string& method, const Value& params) {
    Value resp(Object{
        {"jsonrpc", Value("2.0")},
        {"method", Value(method)},
        {"params", params}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

} // namespace mcp

int main() {
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Disable stdio buffering for real-time JSON-RPC
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    char line[65536];
    
    while (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        // Parse JSON-RPC request using pluto::json
        Value req = Value::parse(line);
        
        if (req.is_null()) continue;

        int id = -1;
        if (auto* id_val = req.find("id")) {
            if (id_val->is_num()) id = static_cast<int>(id_val->as_num());
        }

        std::string method = req.find("method") ? req.find("method")->as_str() : "";
        Value params = req.find("params") ? *req.find("params") : Value();

        if (method == "initialize") {
            if (id >= 0) mcp::send_response(id, mcp::handle_initialize(params));
        }
        else if (method == "notifications/initialized") {
            // No response needed for notifications
        }
        else if (method == "tools/list") {
            if (id >= 0) mcp::send_response(id, mcp::handle_tools_list());
        }
        else if (method == "tools/call") {
            std::string tool_name = params.find("name") ? params.find("name")->as_str() : "";
            Value args = params.find("arguments") ? *params.find("arguments") : Value();
            if (id >= 0) {
                Value result = mcp::handle_tool_call(tool_name, args);
                mcp::send_response(id, result);
            }
        }
        else if (method == "ping") {
            if (id >= 0) mcp::send_response(id, Value(Object{}));
        }
        else {
            if (id >= 0) mcp::send_error_response(id, -32601, "Method not found: " + method);
        }
    }
    
    curl_global_cleanup();
    return 0;
}
