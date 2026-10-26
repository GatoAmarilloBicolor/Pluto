#include "RemoteDecisionModel.h"
#include "../json.h"
#include "../util.h"
#include "TokenSaver.h"

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

std::string json_str_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return {};
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return {};
        return json.substr(pos, end - pos);
    }
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
    std::string inner = json_arr.substr(1, json_arr.size() - 2);
    size_t pos = 0;
    while (pos < inner.size()) {
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
    if (!fConfig.base_url.empty()) return fConfig.base_url;
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
        size_t res_pos = response.data.find("\"result\"");
        if (res_pos == std::string::npos) return {};

        size_t colon = response.data.find(':', res_pos);
        if (colon == std::string::npos) return {};

        size_t start = colon + 1;
        while (start < response.data.size() && (response.data[start] == ' ' || response.data[start] == '\t')) start++;

        if (start >= response.data.size()) return {};

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
            size_t pos = start + 1;
            size_t end = response.data.find('"', pos);
            if (end == std::string::npos) return {};
            return response.data.substr(pos, end - pos);
        } else {
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

// ---------- Classification ----------

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

SentimentResult RemoteDecisionModel::classify_sentiment(
    const std::string& text) {

    SentimentResult result;
    if (!is_available()) {
        result.sentiment = "neutral";
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["positive"] = "positive sentiment, happy, good, great";
    categories["negative"] = "negative sentiment, sad, bad, terrible";
    categories["neutral"] = "neutral sentiment, neither positive nor negative";
    categories["mixed"] = "mixed sentiment, both positive and negative";

    auto r = classify(text, categories, 0.3);
    result.sentiment = r.label;
    result.score = r.probability;
    result.confidence = r.confidence;
    return result;
}

PriorityResult RemoteDecisionModel::classify_priority(
    const std::string& text) {

    PriorityResult result;
    if (!is_available()) {
        result.priority = "low";
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down, security breach";
    categories["high"] = "high priority, urgent, important, asap";
    categories["medium"] = "medium priority, normal, standard";
    categories["low"] = "low priority, minor, trivial, cosmetic";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.escalate = (r.label == "critical" || r.label == "high");
    return result;
}

TopicResult RemoteDecisionModel::classify_topic(
    const std::string& text) {

    TopicResult result;
    if (!is_available()) return result;

    std::map<std::string, std::string> categories;
    categories["coding"] = "programming, code, software development, debugging";
    categories["documentation"] = "documentation, README, guides, tutorials";
    categories["bug"] = "bug report, error, issue, crash";
    categories["feature"] = "feature request, enhancement, new functionality";
    categories["infrastructure"] = "infrastructure, deployment, devops, ci/cd";
    categories["security"] = "security, vulnerability, patch, exploit";

    auto r = classify(text, categories, 0.3);
    result.topic = r.label;
    result.confidence = r.confidence;
    return result;
}

UrgencyResult RemoteDecisionModel::classify_urgency(
    const std::string& text) {

    UrgencyResult result;
    if (!is_available()) {
        result.urgency = "low";
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, immediate action required";
    categories["high"] = "high urgency, urgent, important, time-sensitive";
    categories["medium"] = "medium urgency, normal priority";
    categories["low"] = "low urgency, can wait, minor";

    auto r = classify(text, categories, 0.3);
    result.urgency = r.label;
    result.score = r.probability;
    result.needs_immediate_action = (r.label == "critical");
    return result;
}

// ---------- Verification & Safety ----------

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
    result.explanation = json_str_value(resp, "explanation");
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

// ---------- Context Pruning & Token Savings ----------

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

KeyPointsResult RemoteDecisionModel::extract_key_points(
    const std::string& text,
    int max_points) {

    KeyPointsResult result;
    result.original_tokens = estimate_tokens(text);

    if (!is_available()) {
        // Fallback: simple sentence extraction
        result.points = {extract_key_sentences(text, max_points)};
        result.compressed_tokens = estimate_tokens(result.points[0]);
        result.compression_ratio = compute_compression_ratio(text, result.points[0]);
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["key_point"] = "important fact, insight, or conclusion";

    auto r = classify_multi(text, categories, 0.3);
    result.points = r.active_labels;

    if (result.points.empty()) {
        result.points = {extract_key_sentences(text, max_points)};
    }

    std::string joined;
    for (const auto& p : result.points) {
        if (!joined.empty()) joined += " ";
        joined += p;
    }
    result.compressed_tokens = estimate_tokens(joined);
    result.compression_ratio = compute_compression_ratio(text, joined);
    return result;
}

SummarizeResult RemoteDecisionModel::summarize_text(
    const std::string& text,
    int max_tokens) {

    SummarizeResult result;
    result.original_tokens = estimate_tokens(text);

    if (!is_available()) {
        result.summary = compress_message(text, max_tokens);
        result.compressed_tokens = estimate_tokens(result.summary);
        result.compression_ratio = compute_compression_ratio(text, result.summary);
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["summary"] = "concise summary of the main points";

    auto r = classify_multi(text, categories, 0.3);
    result.summary.clear();
    for (const auto& p : r.active_labels) {
        if (!result.summary.empty()) result.summary += " ";
        result.summary += p;
    }

    if (result.summary.empty()) {
        result.summary = compress_message(text, max_tokens);
    }

    result.compressed_tokens = estimate_tokens(result.summary);
    result.compression_ratio = compute_compression_ratio(text, result.summary);
    return result;
}

ExtractEntitiesResult RemoteDecisionModel::extract_entities(
    const std::string& text) {

    ExtractEntitiesResult result;
    if (!is_available()) return result;

    std::map<std::string, std::string> categories;
    categories["person"] = "person name, human, individual";
    categories["organization"] = "company, organization, institution";
    categories["location"] = "place, city, country, location";
    categories["date"] = "date, time, when";
    categories["technology"] = "technology, software, hardware, tool";

    auto r = classify_multi(text, categories, 0.3);
    result.entities = r.active_labels;

    // Simple entity type assignment based on category match
    for (const auto& e : result.entities) {
        result.entity_types.push_back(e);  // Simplified: use entity as type
        result.confidence_scores[e] = 0.8;
    }

    return result;
}

std::vector<RankItem> RemoteDecisionModel::rank_importance(
    const std::string& question,
    const std::vector<std::string>& items) {

    std::vector<RankItem> result;
    if (!is_available()) {
        // Fallback: rank by keyword overlap
        for (size_t i = 0; i < items.size(); i++) {
            RankItem ri;
            ri.index = static_cast<int>(i);
            ri.score = compute_compression_ratio(
                question, extract_key_sentences(items[i], 1));
            ri.reason = "keyword overlap";
            result.push_back(ri);
        }
        std::sort(result.begin(), result.end(),
            [](const RankItem& a, const RankItem& b) {
                return a.score > b.score;
            });
        return result;
    }

    // Use select_relevant to get indices, then assign scores
    auto sel = select_relevant(question, items, "chunks", 0.3);
    for (size_t i = 0; i < items.size(); i++) {
        RankItem ri;
        ri.index = static_cast<int>(i);
        ri.score = 0.0;
        ri.reason = "not selected";
        for (int sel_idx : sel.selected_indices) {
            if (sel_idx == static_cast<int>(i)) {
                ri.score = 1.0;
                ri.reason = "selected as relevant";
                break;
            }
        }
        result.push_back(ri);
    }

    std::sort(result.begin(), result.end(),
        [](const RankItem& a, const RankItem& b) {
            return a.score > b.score;
        });

    return result;
}

ContextHeadroomResult RemoteDecisionModel::compute_headroom(
    int total_tokens,
    int used_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::compute_context_headroom(
        total_tokens, used_tokens, compaction_threshold);
}

std::vector<int> RemoteDecisionModel::compact_messages(
    const std::vector<std::string>& messages,
    int max_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::select_messages_to_compact(
        messages, "", max_tokens, compaction_threshold);
}

// ---------- Semantic Dedup ----------

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

// ---------- Decision & Triage ----------

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

FullTriageResult RemoteDecisionModel::triage_full(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    FullTriageResult result;
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

    std::string actions_str = json_array_value(resp, "recommended_actions");
    result.recommended_actions = parse_string_array(actions_str);

    std::string tokens_saved_str = json_str_value(resp, "tokens_saved");
    if (!tokens_saved_str.empty()) {
        try { result.estimated_tokens_saved = std::stoi(tokens_saved_str); } catch (...) {}
    }

    result.reasoning = json_str_value(resp, "reasoning");
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
    result.reasoning = json_str_value(resp, "reasoning");
    return result;
}

RouteDecisionResult RemoteDecisionModel::decide_route(
    const std::string& text,
    const std::map<std::string, std::string>& routes) {

    RouteDecisionResult result;
    if (!is_available()) {
        result.confidence = 0.0;
        return result;
    }

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"routes\":" + map_to_json(routes) + "}";

    std::string resp = post_jsonrpc("systemone_decide_route", params);
    if (resp.empty()) return result;

    result.route = json_str_value(resp, "route");
    result.confidence = json_num_value(resp, "confidence", 0.0);
    result.reasoning = json_str_value(resp, "reasoning");

    std::string alts_str = json_array_value(resp, "alternatives");
    result.alternatives = parse_string_array(alts_str);
    return result;
}

PriorityDecisionResult RemoteDecisionModel::decide_priority(
    const std::string& text) {

    PriorityDecisionResult result;
    if (!is_available()) {
        result.priority = "low";
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down";
    categories["high"] = "high priority, urgent, important";
    categories["medium"] = "medium priority, normal";
    categories["low"] = "low priority, minor";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.should_escalate = (r.label == "critical" || r.label == "high");
    result.reasoning = "Classified as " + r.label + " with confidence " +
        std::to_string(r.confidence);
    return result;
}

ActionDecisionResult RemoteDecisionModel::decide_action(
    const std::string& text,
    const std::vector<std::string>& available_actions) {

    ActionDecisionResult result;
    if (!is_available()) {
        result.confidence = 0.0;
        return result;
    }

    std::map<std::string, std::string> categories;
    for (const auto& action : available_actions) {
        categories[action] = "action: " + action;
    }

    auto r = classify(text, categories, 0.3);
    result.action = r.label;
    result.confidence = r.confidence;
    result.reasoning = "Selected action: " + r.label;

    // Estimate tokens for action execution
    result.estimated_tokens = estimate_tokens(text) + estimate_tokens(result.action);
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

    // Extract token savings if present
    std::string ts_str = json_str_value(resp, "tokens_saved");
    if (!ts_str.empty()) {
        try { result.tokens_saved = std::stoi(ts_str); } catch (...) {}
    }

    std::string steps_str = json_str_value(resp, "steps_executed");
    if (!steps_str.empty()) {
        try { result.steps_executed = std::stoi(steps_str); } catch (...) {}
    }

    return result;
}

SentimentResult RemoteDecisionModel::classify_sentiment(
    const std::string& text) {

    SentimentResult result;
    if (!is_available()) {
        result.sentiment = "neutral";
        result.score = 0.0;
        result.confidence = 0.0;
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["positive"] = "positive sentiment, happy, good, great, excellent";
    categories["negative"] = "negative sentiment, sad, bad, terrible, awful";
    categories["neutral"] = "neutral sentiment, neither positive nor negative";
    categories["mixed"] = "mixed sentiment, both positive and negative";

    auto r = classify(text, categories, 0.3);
    result.sentiment = r.label;
    result.score = r.probability;
    result.confidence = r.confidence;
    return result;
}

PriorityResult RemoteDecisionModel::classify_priority(
    const std::string& text) {

    PriorityResult result;
    if (!is_available()) {
        result.priority = "low";
        result.score = 0.0;
        result.should_escalate = false;
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down, security breach";
    categories["high"] = "high priority, urgent, important, asap";
    categories["medium"] = "medium priority, normal, standard";
    categories["low"] = "low priority, minor, trivial, cosmetic";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.should_escalate = (r.label == "critical" || r.label == "high");
    return result;
}

TopicResult RemoteDecisionModel::classify_topic(
    const std::string& text) {

    TopicResult result;
    if (!is_available()) return result;

    std::map<std::string, std::string> categories;
    categories["coding"] = "programming, code, software development, debugging";
    categories["documentation"] = "documentation, README, guides, tutorials";
    categories["bug"] = "bug report, error, issue, crash";
    categories["feature"] = "feature request, enhancement, new functionality";
    categories["infrastructure"] = "infrastructure, deployment, devops, ci/cd";
    categories["security"] = "security, vulnerability, patch, exploit";

    auto r = classify(text, categories, 0.3);
    result.topic = r.label;
    result.confidence = r.confidence;
    return result;
}

UrgencyResult RemoteDecisionModel::classify_urgency(
    const std::string& text) {

    UrgencyResult result;
    if (!is_available()) {
        result.urgency = "low";
        result.score = 0.0;
        result.needs_immediate_action = false;
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, immediate action required";
    categories["high"] = "high urgency, urgent, important, time-sensitive";
    categories["medium"] = "medium urgency, normal priority";
    categories["low"] = "low urgency, can wait, minor";

    auto r = classify(text, categories, 0.3);
    result.urgency = r.label;
    result.score = r.probability;
    result.needs_immediate_action = (r.label == "critical");
    return result;
}

KeyPointsResult RemoteDecisionModel::extract_key_points(
    const std::string& text,
    int max_points) {

    KeyPointsResult result;
    result.original_tokens = estimate_tokens(text);

    if (!is_available()) {
        result.points = {extract_key_sentences(text, max_points)};
        result.compressed_tokens = estimate_tokens(result.points[0]);
        result.compression_ratio = compute_compression_ratio(text, result.points[0]);
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["key_point"] = "important fact, insight, or conclusion";

    auto r = classify_multi(text, categories, 0.3);
    result.points = r.active_labels;

    if (result.points.empty()) {
        result.points = {extract_key_sentences(text, max_points)};
    }

    std::string joined;
    for (const auto& p : result.points) {
        if (!joined.empty()) joined += " ";
        joined += p;
    }
    result.compressed_tokens = estimate_tokens(joined);
    result.compression_ratio = compute_compression_ratio(text, joined);
    return result;
}

SummarizeResult RemoteDecisionModel::summarize_text(
    const std::string& text,
    int max_tokens) {

    SummarizeResult result;
    result.original_tokens = estimate_tokens(text);

    if (!is_available()) {
        result.summary = compress_message(text, max_tokens);
        result.compressed_tokens = estimate_tokens(result.summary);
        result.compression_ratio = compute_compression_ratio(text, result.summary);
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["summary"] = "concise summary of the main points";

    auto r = classify_multi(text, categories, 0.3);
    result.summary.clear();
    for (const auto& p : r.active_labels) {
        if (!result.summary.empty()) result.summary += " ";
        result.summary += p;
    }

    if (result.summary.empty()) {
        result.summary = compress_message(text, max_tokens);
    }

    result.compressed_tokens = estimate_tokens(result.summary);
    result.compression_ratio = compute_compression_ratio(text, result.summary);
    return result;
}

ExtractEntitiesResult RemoteDecisionModel::extract_entities(
    const std::string& text) {

    ExtractEntitiesResult result;
    if (!is_available()) return result;

    std::map<std::string, std::string> categories;
    categories["person"] = "person name, human, individual";
    categories["organization"] = "company, organization, institution";
    categories["location"] = "place, city, country, location";
    categories["date"] = "date, time, when";
    categories["technology"] = "technology, software, hardware, tool";

    auto r = classify_multi(text, categories, 0.3);
    result.entities = r.active_labels;

    for (const auto& e : result.entities) {
        result.entity_types.push_back(e);
        result.confidence_scores[e] = 0.8;
    }

    return result;
}

std::vector<RankItem> RemoteDecisionModel::rank_importance(
    const std::string& question,
    const std::vector<std::string>& items) {

    std::vector<RankItem> result;
    if (!is_available()) {
        for (size_t i = 0; i < items.size(); i++) {
            RankItem ri;
            ri.index = static_cast<int>(i);
            ri.score = compute_compression_ratio(
                question, extract_key_sentences(items[i], 1));
            ri.reason = "keyword overlap";
            result.push_back(ri);
        }
        std::sort(result.begin(), result.end(),
            [](const RankItem& a, const RankItem& b) {
                return a.score > b.score;
            });
        return result;
    }

    auto sel = select_relevant(question, items, "chunks", 0.3);
    for (size_t i = 0; i < items.size(); i++) {
        RankItem ri;
        ri.index = static_cast<int>(i);
        ri.score = 0.0;
        ri.reason = "not selected";
        for (int sel_idx : sel.selected_indices) {
            if (sel_idx == static_cast<int>(i)) {
                ri.score = 1.0;
                ri.reason = "selected as relevant";
                break;
            }
        }
        result.push_back(ri);
    }

    std::sort(result.begin(), result.end(),
        [](const RankItem& a, const RankItem& b) {
            return a.score > b.score;
        });

    return result;
}

ContextHeadroomResult RemoteDecisionModel::compute_headroom(
    int total_tokens,
    int used_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::compute_context_headroom(
        total_tokens, used_tokens, compaction_threshold);
}

std::vector<int> RemoteDecisionModel::compact_messages(
    const std::vector<std::string>& messages,
    int max_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::select_messages_to_compact(
        messages, "", max_tokens, compaction_threshold);
}

FullTriageResult RemoteDecisionModel::triage_full(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    FullTriageResult result;
    result.guard_risk = "LOW";

    std::string best_route;
    double best_score = 0.0;
    for (const auto& [route, desc] : routes) {
        double score = keyword_overlap(text, route) * 0.6 +
                       keyword_overlap(text, desc) * 0.4;
        if (score > best_score) {
            best_score = score;
            best_route = route;
        }
    }
    result.route = best_route;

    bool toxic = contains_toxic_keywords(text);
    bool pii = contains_pii_keywords(text);
    if (toxic || pii) {
        result.guard_risk = "HIGH";
    } else if (contains_urgent_keywords(text)) {
        result.guard_risk = "MEDIUM";
    }

    result.urgent = contains_urgent_keywords(text);
    result.probability = result.urgent ? 0.8 : 0.3;

    for (const auto& [tag, desc] : tags) {
        if (keyword_overlap(text, tag) > 0.2 ||
            keyword_overlap(text, desc) > 0.2) {
            result.tags.push_back(tag);
        }
    }

    if (result.urgent) {
        result.recommended_actions.push_back("escalate_to_human");
    }
    if (result.guard_risk == "HIGH") {
        result.recommended_actions.push_back("security_review");
    }

    result.estimated_tokens_saved = estimate_tokens(text) / 4;
    result.reasoning = "Triage completed: route=" + result.route +
        ", risk=" + result.guard_risk +
        ", urgent=" + (result.urgent ? "true" : "false");

    return result;
}

RouteDecisionResult RemoteDecisionModel::decide_route(
    const std::string& text,
    const std::map<std::string, std::string>& routes) {

    RouteDecisionResult result;
    result.confidence = 0.0;

    if (!is_available()) return result;

    std::string params = "{\"text\":\"" + json_escape(text) +
        "\",\"routes\":" + map_to_json(routes) + "}";

    std::string resp = post_jsonrpc("systemone_decide_route", params);
    if (resp.empty()) return result;

    result.route = json_str_value(resp, "route");
    result.confidence = json_num_value(resp, "confidence", 0.0);
    result.reasoning = json_str_value(resp, "reasoning");

    std::string alts_str = json_array_value(resp, "alternatives");
    result.alternatives = parse_string_array(alts_str);
    return result;
}

PriorityDecisionResult RemoteDecisionModel::decide_priority(
    const std::string& text) {

    PriorityDecisionResult result;
    if (!is_available()) {
        result.priority = "low";
        return result;
    }

    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down";
    categories["high"] = "high priority, urgent, important";
    categories["medium"] = "medium priority, normal";
    categories["low"] = "low priority, minor";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.should_escalate = (r.label == "critical" || r.label == "high");
    result.reasoning = "Classified as " + r.label + " with confidence " +
        std::to_string(r.confidence);
    return result;
}

ActionDecisionResult RemoteDecisionModel::decide_action(
    const std::string& text,
    const std::vector<std::string>& available_actions) {

    ActionDecisionResult result;
    if (!is_available()) {
        result.confidence = 0.0;
        return result;
    }

    std::map<std::string, std::string> categories;
    for (const auto& action : available_actions) {
        categories[action] = "action: " + action;
    }

    auto r = classify(text, categories, 0.3);
    result.action = r.label;
    result.confidence = r.confidence;
    result.reasoning = "Selected action: " + r.label;

    result.estimated_tokens = estimate_tokens(text) + estimate_tokens(result.action);
    return result;
}

} // namespace systemone
} // namespace pluto
