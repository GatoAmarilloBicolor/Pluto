#include "RemoteDecisionModel.h"
#include "../json.h"
#include "../util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace pluto {
namespace systemone {

namespace {

struct CurlString {
    std::string data;
    static size_t write_cb(char* ptr, size_t size, size_t nm, void* ud) {
        auto* s = static_cast<CurlString*>(ud);
        s->data.append(ptr, size * nm);
        return size * nm;
    }
};

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
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
    return out;
}

std::string map_to_json(const std::map<std::string, std::string>& m) {
    std::string out = "{";
    bool first = true;
    for (const auto& [k, v] : m) {
        if (!first) out += ",";
        out += "\"" + json_escape(k) + "\":\"" + json_escape(v) + "\"";
        first = false;
    }
    out += "}";
    return out;
}

std::string array_to_json(const std::vector<std::string>& v) {
    std::string out = "[";
    bool first = true;
    for (const auto& s : v) {
        if (!first) out += ",";
        out += "\"" + json_escape(s) + "\"";
        first = false;
    }
    out += "]";
    return out;
}

std::string vector_int_to_json(const std::vector<int>& v) {
    std::string out = "[";
    bool first = true;
    for (int i : v) {
        if (!first) out += ",";
        out += std::to_string(i);
        first = false;
    }
    out += "]";
    return out;
}

// Extract a string value from simple JSON (no full parser needed for our use).
std::string json_str_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    pos++; // skip ':'
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return {};
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return {};
        return json.substr(pos, end - pos);
    }
    // Number or boolean
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']' && json[end] != ' ' && json[end] != '\t') end++;
    return json.substr(pos, end - pos);
}

double json_num_value(const std::string& json, const std::string& key, double def = 0.0) {
    std::string s = json_str_value(json, key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

bool json_bool_value(const std::string& json, const std::string& key, bool def = false) {
    std::string s = json_str_value(json, key);
    if (s == "true") return true;
    if (s == "false") return false;
    return def;
}

std::string json_array_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find('[', pos);
    if (pos == std::string::npos) return {};
    size_t end = json.find(']', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos + 1);
}

std::vector<std::string> parse_string_array(const std::string& json_arr) {
    std::vector<std::string> out;
    if (json_arr.empty() || json_arr == "[]") return out;
    // Remove brackets
    std::string inner = json_arr.substr(1, json_arr.size() - 2);
    size_t pos = 0;
    while (pos < inner.size()) {
        // Skip whitespace and commas
        while (pos < inner.size() && (inner[pos] == ' ' || inner[pos] == '\t' || inner[pos] == ',')) pos++;
        if (pos >= inner.size()) break;
        if (inner[pos] == '"') {
            pos++;
            size_t end = inner.find('"', pos);
            if (end == std::string::npos) break;
            out.push_back(inner.substr(pos, end - pos));
            pos = end + 1;
        }
    }
    return out;
}

} // namespace

RemoteDecisionModel::RemoteDecisionModel(DecisionModelType type, const SystemOneConfig& config)
    : fType(type), fConfig(config) {
    fServerUrl = resolve_server_url();
    fApiKey = ResolveSystemOneApiKey();
    fAvailable = init_curl();
}

RemoteDecisionModel::~RemoteDecisionModel() {
    if (fCurl) curl_easy_cleanup(fCurl);
}

std::string RemoteDecisionModel::name() const {
    switch (fType) {
        case DecisionModelType::REMOTE_KEV: return "kev";
        case DecisionModelType::REMOTE_LAYA: return "laya";
        case DecisionModelType::REMOTE_JEV: return "jev";
        default: return "remote";
    }
}

std::string RemoteDecisionModel::resolve_server_url() {
    // 1. Check base_url override
    if (!fConfig.base_url.empty()) return fConfig.base_url;

    // 2. Use model-specific URL
    switch (fType) {
        case DecisionModelType::REMOTE_KEV: return fConfig.kev_url;
        case DecisionModelType::REMOTE_LAYA: return fConfig.laya_url;
        case DecisionModelType::REMOTE_JEV: return fConfig.jev_url;
        default: return fConfig.kev_url;
    }
}

bool RemoteDecisionModel::init_curl() {
    fCurl = curl_easy_init();
    if (!fCurl) return false;

    curl_easy_setopt(fCurl, CURLOPT_TIMEOUT, fConfig.timeout_secs);
    curl_easy_setopt(fCurl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_easy_setopt(fCurl, CURLOPT_USERAGENT, "pluto-systemone/1.0");
    curl_easy_setopt(fCurl, CURLOPT_CAINFO, "/system/data/ssl/CARootCertificates.pem");
    curl_easy_setopt(fCurl, CURLOPT_FOLLOWLOCATION, 1L);
    return true;
}

std::string RemoteDecisionModel::post_jsonrpc(const std::string& method, const std::string& params_json) {
    if (!fCurl || fServerUrl.empty()) return {};

    // Build JSON-RPC request
    std::string req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"" + method + "\",\"params\":" + params_json + "}";

    CurlString response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!fApiKey.empty()) {
        std::string auth = "Authorization: Bearer " + fApiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(fCurl, CURLOPT_URL, fServerUrl.c_str());
    curl_easy_setopt(fCurl, CURLOPT_POSTFIELDS, req.c_str());
    curl_easy_setopt(fCurl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(fCurl, CURLOPT_WRITEFUNCTION, CurlString::write_cb);
    curl_easy_setopt(fCurl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(fCurl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) return {};

    // Parse JSON-RPC response to extract result
    try {
        // Find "result" field
        size_t res_pos = response.data.find("\"result\"");
        if (res_pos == std::string::npos) return {};

        size_t colon = response.data.find(':', res_pos);
        if (colon == std::string::npos) return {};

        // The result value could be an object, string, etc.
        // Return everything from colon+1 to end (roughly)
        size_t start = colon + 1;
        while (start < response.data.size() && (response.data[start] == ' ' || response.data[start] == '\t')) start++;

        if (start >= response.data.size()) return {};

        // Find matching closing brace/bracket
        if (response.data[start] == '{') {
            int depth = 0;
            size_t i = start;
            while (i < response.data.size()) {
                if (response.data[i] == '{') depth++;
                else if (response.data[i] == '}') {
                    depth--;
                    if (depth == 0) { i++; break; }
                }
                i++;
            }
            return response.data.substr(start, i - start);
        } else if (response.data[start] == '"') {
            pos: // string
            pos++;
            size_t end = response.data.find('"', pos);
            if (end == std::string::npos) return {};
            return response.data.substr(pos, end - pos);
        } else {
            // Number, boolean, null
            size_t end = start;
            while (end < response.data.size() && response.data[end] != ',' && response.data[end] != '}' && response.data[end] != ']') end++;
            return response.data.substr(start, end - start);
        }
    } catch (...) {
        return {};
    }
}

bool RemoteDecisionModel::is_available() {
    return fAvailable && !fServerUrl.empty();
}

// ---------- Tool implementations ----------

ClassifyResult RemoteDecisionModel::classify(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    ClassifyResult result;
    if (!is_available()) {
        result.abstain = true;
        return result;
    }

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"categories\":" + map_to_json(categories) +
        ",\"threshold\":" + std::to_string(threshold) + "}";

    std::string resp = post_jsonrpc("systemone_classify", params);
    if (resp.empty()) {
        result.abstain = true;
        return result;
    }

    result.label = json_str_value(resp, "label");
    result.probability = json_num_value(resp, "probability", 0.0);
    result.confidence = json_num_value(resp, "confidence", 0.0);
    result.abstain = json_bool_value(resp, "abstain", true);
    return result;
}

ClassifyMultiResult RemoteDecisionModel::classify_multi(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    ClassifyMultiResult result;
    if (!is_available()) return result;

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"categories\":" + map_to_json(categories) +
        ",\"threshold\":" + std::to_string(threshold) + "}";

    std::string resp = post_jsonrpc("systemone_classify_multi", params);
    if (resp.empty()) return result;

    std::string active_str = json_array_value(resp, "active");
    result.active_labels = parse_string_array(active_str);
    return result;
}

VerifyResult RemoteDecisionModel::verify(
    const std::string& source,
    const std::string& claim) {

    VerifyResult result;
    if (!is_available()) return result;

    std::string params = "{\"source\":\"" + json_escape(source) +
        "\",\"claim\":\"" + json_escape(claim) + "\"}";

    std::string resp = post_jsonrpc("systemone_verify", params);
    if (resp.empty()) return result;

    result.supported = json_bool_value(resp, "supported", false);
    result.probability = json_num_value(resp, "probability", 0.0);
    return result;
}

GuardResult RemoteDecisionModel::guard(
    const std::string& text,
    const std::map<std::string, std::string>& checks,
    double threshold) {

    GuardResult result;
    if (!is_available()) {
        result.risk = "UNKNOWN";
        return result;
    }

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"checks\":" + map_to_json(checks) +
        ",\"threshold\":" + std::to_string(threshold) + "}";

    std::string resp = post_jsonrpc("systemone_guard", params);
    if (resp.empty()) return result;

    result.risk = json_str_value(resp, "risk");
    if (result.risk.empty()) result.risk = "LOW";
    result.probability = json_num_value(resp, "probability", 0.0);

    std::string hits_str = json_array_value(resp, "hits");
    result.hits = parse_string_array(hits_str);
    return result;
}

SelectRelevantResult RemoteDecisionModel::select_relevant(
    const std::string& question,
    const std::vector<std::string>& items,
    const std::string& kind,
    double min_p) {

    SelectRelevantResult result;
    result.total_items = static_cast<int>(items.size());
    if (!is_available()) return result;

    std::string params = "{\"question\":\"" + json_escape(question) +
        "\",\"items\":" + array_to_json(items) +
        ",\"kind\":\"" + json_escape(kind) + "\"" +
        ",\"min_p\":" + std::to_string(min_p) + "}";

    std::string resp = post_jsonrpc("systemone_select_relevant", params);
    if (resp.empty()) return result;

    std::string sel_str = json_array_value(resp, "selected") + json_array_value(resp, "sel");
    if (!sel_str.empty()) {
        result.selected_indices = {};
        // Parse comma-separated indices from array like [0,2,5]
        std::string inner = sel_str;
        if (inner.size() >= 2) inner = inner.substr(1, inner.size() - 2);
        std::stringstream ss(inner);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try { result.selected_indices.push_back(std::stoi(token)); } catch (...) {}
        }
    }
    return result;
}

SameIntentResult RemoteDecisionModel::same_intent(
    const std::string& a,
    const std::string& b,
    double threshold) {

    SameIntentResult result;
    if (!is_available()) return result;

    std::string params = "{\"a\":\"" + json_escape(a) +
        "\",\"b\":\"" + json_escape(b) +
        "\",\"threshold\":" + std::to_string(threshold) + "}";

    std::string resp = post_jsonrpc("systemone_same_intent", params);
    if (resp.empty()) return result;

    result.same = json_bool_value(resp, "same", false);
    result.probability = json_num_value(resp, "probability", 0.0);
    return result;
}

TriageResult RemoteDecisionModel::triage(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    TriageResult result;
    if (!is_available()) {
        result.guard_risk = "UNKNOWN";
        return result;
    }

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"routes\":" + map_to_json(routes) +
        ",\"tags\":" + map_to_json(tags) +
        ",\"urgent_question\":\"" + json_escape(urgent_question) + "\"}";

    std::string resp = post_jsonrpc("systemone_triage", params);
    if (resp.empty()) return result;

    result.guard_risk = json_str_value(resp, "guard_risk");
    if (result.guard_risk.empty()) result.guard_risk = "LOW";
    result.route = json_str_value(resp, "route");
    result.urgent = json_bool_value(resp, "urgent", false);
    result.probability = json_num_value(resp, "probability", 0.0);

    std::string tags_str = json_array_value(resp, "tags");
    result.tags = parse_string_array(tags_str);
    return result;
}

AnswerableResult RemoteDecisionModel::answerable(
    const std::string& question,
    const std::string& context,
    double threshold) {

    AnswerableResult result;
    if (!is_available()) return result;

    std::string params = "{\"question\":\"" + json_escape(question) +
        "\",\"context\":\"" + json_escape(context) +
        "\",\"threshold\":" + std::to_string(threshold) + "}";

    std::string resp = post_jsonrpc("systemone_answerable", params);
    if (resp.empty()) return result;

    result.answerable = json_bool_value(resp, "answerable", false);
    result.probability = json_num_value(resp, "probability", 0.0);
    return result;
}

ConsultResult RemoteDecisionModel::consult(
    const std::string& question,
    const std::map<std::string, std::string>& options,
    const std::string& topic,
    const std::string& context) {

    ConsultResult result;
    if (!is_available()) {
        result.abstain = true;
        return result;
    }

    std::string params = "{\"question\":\"" + json_escape(question) +
        "\",\"options\":" + map_to_json(options) +
        ",\"topic\":\"" + json_escape(topic) +
        "\",\"context\":\"" + json_escape(context) + "\"}";

    std::string resp = post_jsonrpc("systemone_consult", params);
    if (resp.empty()) return result;

    result.choice = json_str_value(resp, "choice");
    result.probability = json_num_value(resp, "probability", 0.0);
    result.abstain = json_bool_value(resp, "abstain", true);
    result.notes_used = json_str_value(resp, "notes_used");
    return result;
}

DecideProgramResult RemoteDecisionModel::decide_program(
    const std::string& program_json,
    const std::string& state,
    bool compact) {

    DecideProgramResult result;
    if (!is_available()) return result;

    std::string params = "{\"program\":" + program_json +
        ",\"state\":\"" + json_escape(state) + "\"" +
        ",\"compact\":" + (compact ? "true" : "false") + "}";

    std::string resp = post_jsonrpc("systemone_decide_program", params);
    if (resp.empty()) return result;

    result.results = resp;
    return result;
}

} // namespace systemone
} // namespace pluto
