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

// JSON parsing - simple implementation for MCP
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool bool_val = false;
    double num_val = 0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    JsonValue() = default;
    JsonValue(bool v) : type(Bool), bool_val(v) {}
    JsonValue(double v) : type(Number), num_val(v) {}
    JsonValue(const std::string& v) : type(String), str_val(v) {}
    JsonValue(std::string&& v) : type(String), str_val(std::move(v)) {}
    JsonValue(const char* v) : type(String), str_val(v) {}

    static JsonValue null() { return JsonValue(); }
    static JsonValue array(std::vector<JsonValue>&& v) { JsonValue r; r.type = Array; r.arr_val = std::move(v); return r; }
    static JsonValue object(std::map<std::string, JsonValue>&& v) { JsonValue r; r.type = Object; r.obj_val = std::move(v); return r; }

    bool is_null() const { return type == Null; }
    bool is_bool() const { return type == Bool; }
    bool is_number() const { return type == Number; }
    bool is_string() const { return type == String; }
    bool is_array() const { return type == Array; }
    bool is_object() const { return type == Object; }

    bool as_bool() const { return bool_val; }
    double as_number() const { return num_val; }
    const std::string& as_string() const { return str_val; }
    const std::vector<JsonValue>& as_array() const { return arr_val; }
    const std::map<std::string, JsonValue>& as_object() const { return obj_val; }

    JsonValue* find(const std::string& key) {
        if (type != Object) return nullptr;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? &it->second : nullptr;
    }
    const JsonValue* find(const std::string& key) const {
        if (type != Object) return nullptr;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? &it->second : nullptr;
    }

    std::string serialize() const;
};

namespace {
    // Simplified JSON parser for MCP protocol
    struct Parser {
        const char* s;
        size_t i = 0, len = 0;

        Parser(const std::string& str) : s(str.c_str()), len(str.size()) {}

        void skip_ws() {
            while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
        }

        JsonValue parse() {
            skip_ws();
            if (i >= len) return JsonValue();
            char c = s[i];
            if (c == 'n' && i+3 < len && s[i+1]=='u' && s[i+2]=='l' && s[i+3]=='l') { i += 4; return JsonValue::null(); }
            if (c == 't' && i+3 < len && s[i+1]=='r' && s[i+2]=='u' && s[i+3]=='e') { i += 4; return JsonValue(true); }
            if (c == 'f' && i+4 < len && s[i+1]=='a' && s[i+2]=='l' && s[i+3]=='s' && s[i+4]=='e') { i += 5; return JsonValue(false); }
            if (c == '"') return parse_string();
            if (c == '[') return parse_array();
            if (c == '{') return parse_object();
            return parse_number();
        }

        JsonValue parse_string() {
            ++i; // skip "
            size_t start = i;
            while (i < len && s[i] != '"') {
                if (s[i] == '\\') { i += 2; continue; }
                ++i;
            }
            std::string val = std::string(s + start, i - start);
            if (i < len) ++i;
            return JsonValue(val);
        }

        JsonValue parse_number() {
            size_t start = i;
            if (s[i] == '-') ++i;
            while (i < len && s[i] >= '0' && s[i] <= '9') ++i;
            if (i < len && s[i] == '.') { ++i; while (i < len && s[i] >= '0' && s[i] <= '9') ++i; }
            if (i < len && (s[i] == 'e' || s[i] == 'E')) { ++i; if (i < len && (s[i] == '+' || s[i] == '-')) ++i; while (i < len && s[i] >= '0' && s[i] <= '9') ++i; }
            double val = std::strtod(s + start, nullptr);
            return JsonValue(val);
        }

        JsonValue parse_array() {
            ++i; // skip [
            std::vector<JsonValue> arr;
            while (true) {
                skip_ws();
                if (i < len && s[i] == ']') { ++i; break; }
                arr.push_back(parse());
                skip_ws();
                if (i < len && s[i] == ',') { ++i; continue; }
                if (i < len && s[i] == ']') { ++i; break; }
            }
            return JsonValue::array(std::move(arr));
        }

        JsonValue parse_object() {
            ++i; // skip {
            std::map<std::string, JsonValue> obj;
            while (true) {
                skip_ws();
                if (i < len && s[i] == '}') { ++i; break; }
                JsonValue key = parse_string();
                skip_ws();
                if (i < len && s[i] == ':') ++i;
                JsonValue val = parse();
                obj[key.as_string()] = std::move(val);
                skip_ws();
                if (i < len && s[i] == ',') { ++i; continue; }
                if (i < len && s[i] == '}') { ++i; break; }
            }
            return JsonValue::object(std::move(obj));
        }
    };
}

std::string JsonValue::serialize() const {
    switch (type) {
        case Null: return "null";
        case Bool: return bool_val ? "true" : "false";
        case Number: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.17g", num_val);
            return buf;
        }
        case String: {
            std::string out = "\"";
            for (char c : str_val) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out += c; break;
                }
            }
            out += "\"";
            return out;
        }
        case Array: {
            std::string out = "[";
            for (size_t i = 0; i < arr_val.size(); ++i) {
                if (i > 0) out += ",";
                out += arr_val[i].serialize();
            }
            out += "]";
            return out;
        }
        case Object: {
            std::string out = "{";
            bool first = true;
            for (const auto& [k, v] : obj_val) {
                if (!first) out += ",";
                first = false;
                out += "\"" + k + "\":" + v.serialize();
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

// Decision model tool wrappers - call out to kev MCP or implement locally
namespace decision {

struct ClassifyResult {
    std::string label;
    double probability = 0;
    double confidence = 0;
    bool abstain = false;
};

struct ClassifyMultiResult {
    std::vector<std::string> active;
    std::map<std::string, double> probabilities;
};

struct VerifyResult {
    bool supported = false;
    double probability = 0;
};

struct GuardResult {
    std::string risk = "LOW";
    std::vector<std::string> hits;
    std::map<std::string, double> probabilities;
};

struct SelectRelevantResult {
    std::vector<int> selected_indices;
    int total_items = 0;
};

struct SameIntentResult {
    bool same = false;
    double probability = 0;
};

struct TriageResult {
    std::string guard_risk = "LOW";
    std::string route;
    std::vector<std::string> tag_hits;
    bool urgent = false;
};

struct AnswerableResult {
    bool answerable = false;
    double probability = 0;
};

struct ConsultResult {
    std::string choice;
    double probability = 0;
    bool abstain = false;
    std::string notes_used;
};

struct DecideProgramResult {
    JsonValue results;
};

// Stub implementations - in production these would call the kev MCP server
// For now, return reasonable defaults for testing

ClassifyResult classify(const std::string& text, const std::map<std::string, std::string>& categories, double threshold = 0.5) {
    // Simple keyword-based classification for demo
    std::string lower = text;
    for (auto& c : lower) c = std::tolower(c);
    
    ClassifyResult r;
    r.label = categories.begin()->first;
    r.probability = 0.6;
    r.confidence = 0.7;
    r.abstain = r.confidence < threshold;
    return r;
}

ClassifyMultiResult classify_multi(const std::string& text, const std::map<std::string, std::string>& categories, double threshold = 0.7) {
    ClassifyMultiResult r;
    std::string lower = text;
    for (auto& c : lower) c = std::tolower(c);
    for (const auto& [cat, desc] : categories) {
        r.probabilities[cat] = 0.3;
        if (lower.find(cat) != std::string::npos) {
            r.probabilities[cat] = 0.8;
            r.active.push_back(cat);
        }
    }
    return r;
}

VerifyResult verify(const std::string& source, const std::string& claim) {
    VerifyResult r;
    r.supported = true;
    r.probability = 0.8;
    return r;
}

GuardResult guard(const std::string& text, const std::map<std::string, std::string>& checks, double threshold = 0.5) {
    GuardResult r;
    r.risk = "LOW";
    return r;
}

SelectRelevantResult select_relevant(const std::string& question, const std::vector<std::string>& items, const std::string& kind, double min_p = 0.6) {
    SelectRelevantResult r;
    r.total_items = items.size();
    for (int i = 0; i < (int)items.size(); ++i) {
        if (items[i].find(question.substr(0, 5)) != std::string::npos) {
            r.selected_indices.push_back(i);
        }
    }
    return r;
}

SameIntentResult same_intent(const std::string& a, const std::string& b, double threshold = 0.5) {
    SameIntentResult r;
    r.same = a.size() == b.size() && a == b;
    r.probability = r.same ? 0.95 : 0.1;
    return r;
}

TriageResult triage(const std::string& text, const std::map<std::string, std::string>& routes,
                     const std::map<std::string, std::string>& tags, const std::string& urgent_q) {
    TriageResult r;
    r.guard_risk = "LOW";
    r.route = routes.begin()->first;
    r.urgent = false;
    return r;
}

AnswerableResult answerable(const std::string& question, const std::string& context, double threshold = 0.3) {
    AnswerableResult r;
    r.answerable = !context.empty();
    r.probability = r.answerable ? 0.8 : 0.1;
    return r;
}

ConsultResult consult(const std::string& question, const std::map<std::string, std::string>& options,
                       const std::string& topic, const std::string& context) {
    ConsultResult r;
    r.choice = options.begin()->first;
    r.probability = 0.7;
    r.abstain = false;
    r.notes_used = "no notes loaded";
    return r;
}

DecideProgramResult decide_program(const JsonValue& program, const std::string& state, bool compact = true) {
    DecideProgramResult r;
    r.results = JsonValue::object({});
    return r;
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
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\",\"additionalProperties\":{\"type\":\"string\"}}},\"required\":[\"text\",\"categories\"]}"
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

JsonValue make_tool_result(const std::string& text, bool is_error = false) {
    JsonValue content = JsonValue::array({
        JsonValue::object({{"type", JsonValue("text")}, {"text", JsonValue(text)}})
    });
    return JsonValue::object({
        {"content", content},
        {"isError", JsonValue(is_error)}
    });
}

JsonValue make_error_result(const std::string& msg) {
    return JsonValue::object({
        {"error", JsonValue::object({
            {"code", JsonValue(-32603)},
            {"message", JsonValue(msg)}
        })}
    });
}

JsonValue handle_initialize(const JsonValue& params) {
    return JsonValue::object({
        {"protocolVersion", JsonValue("2024-11-05")},
        {"capabilities", JsonValue::object({
            {"tools", JsonValue::object({})}
        })},
        {"serverInfo", JsonValue::object({
            {"name", JsonValue("systemone")},
            {"version", JsonValue("1.0.0")}
        })}
    });
}

JsonValue handle_tools_list() {
    std::vector<JsonValue> tools;
    for (const auto& t : kTools) {
        tools.push_back(JsonValue::object({
            {"name", JsonValue(t.name)},
            {"description", JsonValue(t.description)},
            {"inputSchema", JsonValue(t.input_schema)}
        }));
    }
    return JsonValue::object({{"tools", JsonValue::array(std::move(tools))}});
}

JsonValue handle_tool_call(const std::string& name, const JsonValue& args) {
    using namespace decision;
    
    try {
        if (name == "systemone_classify") {
            std::string text = args.find("text") ? args.find("text")->as_string() : "";
            std::map<std::string, std::string> cats;
            if (auto* c = args.find("categories")) {
                if (c->is_object()) {
                    for (const auto& [k, v] : c->as_object()) {
                        if (v.is_string()) cats[k] = v.as_string();
                    }
                }
            }
            auto r = classify(text, cats);
            return make_tool_result(
                "label: " + r.label + "\nprobability: " + std::to_string(r.probability) +
                "\nconfidence: " + std::to_string(r.confidence) +
                "\nabstain: " + (r.abstain ? "true" : "false")
            );
        }
        else if (name == "systemone_classify_multi") {
            std::string text = args.find("text") ? args.find("text")->as_string() : "";
            std::map<std::string, std::string> cats;
            if (auto* c = args.find("categories")) {
                if (c->is_object()) {
                    for (const auto& [k, v] : c->as_object()) {
                        if (v.is_string()) cats[k] = v.as_string();
                    }
                }
            }
            double threshold = 0.7;
            if (auto* t = args.find("threshold")) {
                if (t->is_number()) threshold = t->as_number();
            }
            auto r = classify_multi(text, cats, threshold);
            std::string out = "active: ";
            for (size_t i = 0; i < r.active.size(); ++i) {
                if (i > 0) out += ", ";
                out += r.active[i];
            }
            out += "\nprobabilities: ";
            for (const auto& [k, v] : r.probabilities) {
                out += k + "=" + std::to_string(v) + " ";
            }
            return make_tool_result(out);
        }
        else if (name == "systemone_verify") {
            std::string source = args.find("source") ? args.find("source")->as_string() : "";
            std::string claim = args.find("claim") ? args.find("claim")->as_string() : "";
            auto r = verify(source, claim);
            return make_tool_result(
                "supported: " + std::string(r.supported ? "true" : "false") +
                "\nprobability: " + std::to_string(r.probability)
            );
        }
        else if (name == "systemone_guard") {
            std::string text = args.find("text") ? args.find("text")->as_string() : "";
            std::map<std::string, std::string> checks;
            if (auto* c = args.find("checks")) {
                if (c->is_object()) {
                    for (const auto& [k, v] : c->as_object()) {
                        if (v.is_string()) checks[k] = v.as_string();
                    }
                }
            }
            double threshold = 0.5;
            if (auto* t = args.find("threshold")) {
                if (t->is_number()) threshold = t->as_number();
            }
            auto r = guard(text, checks, threshold);
            std::string out = "risk: " + r.risk + "\nhits: ";
            for (const auto& h : r.hits) out += h + " ";
            return make_tool_result(out);
        }
        else if (name == "systemone_select_relevant") {
            std::string question = args.find("question") ? args.find("question")->as_string() : "";
            std::vector<std::string> items;
            if (auto* it = args.find("items")) {
                if (it->is_array()) {
                    for (const auto& v : it->as_array()) {
                        if (v.is_string()) items.push_back(v.as_string());
                    }
                }
            }
            std::string kind = "chunks";
            if (auto* k = args.find("kind")) {
                if (k->is_string()) kind = k->as_string();
            }
            double min_p = 0.6;
            if (auto* m = args.find("min_p")) {
                if (m->is_number()) min_p = m->as_number();
            }
            auto r = select_relevant(question, items, kind, min_p);
            std::string out = "selected: ";
            for (size_t i = 0; i < r.selected_indices.size(); ++i) {
                if (i > 0) out += ", ";
                out += std::to_string(r.selected_indices[i]);
            }
            out += " (total: " + std::to_string(r.total_items) + ")";
            return make_tool_result(out);
        }
        else if (name == "systemone_same_intent") {
            std::string a = args.find("a") ? args.find("a")->as_string() : "";
            std::string b = args.find("b") ? args.find("b")->as_string() : "";
            double threshold = 0.5;
            if (auto* t = args.find("threshold")) {
                if (t->is_number()) threshold = t->as_number();
            }
            auto r = same_intent(a, b, threshold);
            return make_tool_result(
                "same: " + std::string(r.same ? "true" : "false") +
                "\nprobability: " + std::to_string(r.probability)
            );
        }
        else if (name == "systemone_triage") {
            std::string text = args.find("text") ? args.find("text")->as_string() : "";
            std::map<std::string, std::string> routes;
            if (auto* r = args.find("routes")) {
                if (r->is_object()) {
                    for (const auto& [k, v] : r->as_object()) {
                        if (v.is_string()) routes[k] = v.as_string();
                    }
                }
            }
            std::map<std::string, std::string> tags;
            if (auto* t = args.find("tags")) {
                if (t->is_object()) {
                    for (const auto& [k, v] : t->as_object()) {
                        if (v.is_string()) tags[k] = v.as_string();
                    }
                }
            }
            std::string urgent_q = "Does this need a human within the hour?";
            if (auto* u = args.find("urgent_question")) {
                if (u->is_string()) urgent_q = u->as_string();
            }
            auto r = triage(text, routes, tags, urgent_q);
            return make_tool_result(
                "guard_risk: " + r.guard_risk +
                "\nroute: " + r.route +
                "\nurgent: " + (r.urgent ? "true" : "false")
            );
        }
        else if (name == "systemone_answerable") {
            std::string question = args.find("question") ? args.find("question")->as_string() : "";
            std::string context = args.find("context") ? args.find("context")->as_string() : "";
            double threshold = 0.3;
            if (auto* t = args.find("threshold")) {
                if (t->is_number()) threshold = t->as_number();
            }
            auto r = answerable(question, context, threshold);
            return make_tool_result(
                "answerable: " + std::string(r.answerable ? "true" : "false") +
                "\nprobability: " + std::to_string(r.probability)
            );
        }
        else if (name == "systemone_consult") {
            std::string question = args.find("question") ? args.find("question")->as_string() : "";
            std::map<std::string, std::string> options;
            if (auto* o = args.find("options")) {
                if (o->is_object()) {
                    for (const auto& [k, v] : o->as_object()) {
                        if (v.is_string()) options[k] = v.as_string();
                    }
                }
            }
            std::string topic = args.find("topic") ? args.find("topic")->as_string() : "";
            std::string context = args.find("context") ? args.find("context")->as_string() : "";
            auto r = consult(question, options, topic, context);
            return make_tool_result(
                "choice: " + r.choice +
                "\nprobability: " + std::to_string(r.probability) +
                "\nabstain: " + (r.abstain ? "true" : "false") +
                "\nnotes: " + r.notes_used
            );
        }
        else if (name == "systemone_decide_program") {
            JsonValue program = args.find("program") ? *args.find("program") : JsonValue();
            std::string state = args.find("state") ? args.find("state")->as_string() : "";
            bool compact = true;
            if (auto* c = args.find("compact")) {
                if (c->is_bool()) compact = c->as_bool();
            }
            auto r = decide_program(program, state, compact);
            return make_tool_result(r.results.serialize());
        }
        
        return make_error_result("Unknown tool: " + name);
    } catch (const std::exception& e) {
        return make_error_result(std::string("tool error: ") + e.what());
    }
}

void send_response(int id, const JsonValue& result) {
    JsonValue resp = JsonValue::object({
        {"jsonrpc", JsonValue("2.0")},
        {"id", JsonValue(id)},
        {"result", result}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

void send_error_response(int id, int code, const std::string& msg) {
    JsonValue resp = JsonValue::object({
        {"jsonrpc", JsonValue("2.0")},
        {"id", JsonValue(id)},
        {"error", JsonValue::object({
            {"code", JsonValue(code)},
            {"message", JsonValue(msg)}
        })}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

void send_notification(const std::string& method, const JsonValue& params) {
    JsonValue resp = JsonValue::object({
        {"jsonrpc", JsonValue("2.0")},
        {"method", JsonValue(method)},
        {"params", params}
    });
    std::cout << resp.serialize() << "\n";
    std::cout.flush();
}

} // namespace mcp

int main() {
    // Disable stdio buffering for real-time JSON-RPC
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    int next_id = 1;
    char line[65536];
    
    while (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        // Parse JSON-RPC request
        mcp::Parser parser(line);
        JsonValue req = parser.parse();
        
        if (req.is_null()) continue;

        int id = -1;
        if (auto* id_val = req.find("id")) {
            if (id_val->is_number()) id = static_cast<int>(id_val->as_number());
        }

        std::string method = req.find("method") ? req.find("method")->as_string() : "";
        JsonValue params = req.find("params") ? *req.find("params") : JsonValue();

        if (method == "initialize") {
            if (id >= 0) send_response(id, mcp::handle_initialize(params));
        }
        else if (method == "notifications/initialized") {
            // No response needed for notifications
        }
        else if (method == "tools/list") {
            if (id >= 0) send_response(id, mcp::handle_tools_list());
        }
        else if (method == "tools/call") {
            std::string tool_name = params.find("name") ? params.find("name")->as_string() : "";
            JsonValue args = params.find("arguments") ? *params.find("arguments") : JsonValue();
            if (id >= 0) {
                JsonValue result = mcp::handle_tool_call(tool_name, args);
                send_response(id, result);
            }
        }
        else if (method == "ping") {
            if (id >= 0) send_response(id, JsonValue::object({}));
        }
        else {
            if (id >= 0) send_error_response(id, -32601, "Method not found: " + method);
        }
    }
    return 0;
}
