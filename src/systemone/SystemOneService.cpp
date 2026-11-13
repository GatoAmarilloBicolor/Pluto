#include "SystemOneService.h"
#include "SystemOneCategories.h"
#include "dsh/Context.h"
#include "tools/Tool.h"
#include "storage/SessionStore.h"
#include "json.h"
#include "config.h"
#include "curl_security.h"
#include "log.h"
#include <curl/curl.h>
#include <mutex>
#include <sstream>

namespace pluto::systemone {

static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* buffer = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    buffer->append(static_cast<const char*>(ptr), total);
    return total;
}

SystemOneService::SystemOneService(pluto::dsh::Context& ctx,
                                    std::shared_ptr<ToolRegistry> tools,
                                    std::shared_ptr<SessionStore> store,
                                    const SystemOneConfig& cfg)
    : ctx_(ctx), tools_(tools), store_(store), cfg_(cfg) {}

void SystemOneService::register_tools(ToolRegistry& registry) {
    if (!cfg_.enabled) return;

    // systemone_classify
    registry.add(std::make_unique<Tool>("systemone_classify",
        "Single-label classification using System One decision model",
        R"({"type":"object","properties":{"text":{"type":"string"},"categories":{"type":"object"},"threshold":{"type":"number"}},"required":["text","categories"]})",
        [this](const json::Value& input) -> json::Value {
            std::string text = input.find("text") ? input.find("text")->as_str() : "";
            double threshold = input.find("threshold") ? input.find("threshold")->as_double() : 0.5;
            std::map<std::string, std::string> categories;
            if (auto* cat = input.find("categories"); cat && cat->is_obj()) {
                for (const auto& [k, v] : cat->as_obj()) {
                    categories[k] = v.as_str();
                }
            }
            auto result = classify(text, categories, threshold);
            json::Value out;
            out.set("label", result.label);
            out.set("probability", result.probability);
            out.set("confidence", result.confidence);
            out.set("abstain", result.abstain);
            return out;
        }));

    // systemone_classify_multi
    registry.add(std::make_unique<Tool>("systemone_classify_multi",
        "Multi-label classification using System One decision model",
        R"({"type":"object","properties":{"text":{"type":"string"},"categories":{"type":"object"},"threshold":{"type":"number"}},"required":["text","categories"]})",
        [this](const json::Value& input) -> json::Value {
            std::string text = input.find("text") ? input.find("text")->as_str() : "";
            double threshold = input.find("threshold") ? input.find("threshold")->as_double() : 0.5;
            std::map<std::string, std::string> categories;
            if (auto* cat = input.find("categories"); cat && cat->is_obj()) {
                for (const auto& [k, v] : cat->as_obj()) {
                    categories[k] = v.as_str();
                }
            }
            auto result = classify_multi(text, categories, threshold);
            json::Value out;
            json::Value labels_arr;
            for (const auto& l : result.active_labels) labels_arr.push(l);
            out.set("active_labels", std::move(labels_arr));
            json::Value scores_obj;
            for (const auto& [k, v] : result.scores) scores_obj.set(k, v);
            out.set("scores", std::move(scores_obj));
            out.set("abstain", result.abstain);
            return out;
        }));

    // systemone_verify
    registry.add(std::make_unique<Tool>("systemone_verify",
        "Entailment verification (premise entails hypothesis?)",
        R"({"type":"object","properties":{"premise":{"type":"string"},"hypothesis":{"type":"string"}},"required":["premise","hypothesis"]})",
        [this](const json::Value& input) -> json::Value {
            std::string premise = input.find("premise") ? input.find("premise")->as_str() : "";
            std::string hypothesis = input.find("hypothesis") ? input.find("hypothesis")->as_str() : "";
            auto result = verify(premise, hypothesis);
            json::Value out;
            out.set("entails", result.entails);
            out.set("confidence", result.confidence);
            return out;
        }));

    // systemone_guard
    registry.add(std::make_unique<Tool>("systemone_guard",
        "Safety gate: detect prompt injection, PII, toxic content, spam",
        R"({"type":"object","properties":{"input":{"type":"string"},"categories":{"type":"array","items":{"type":"string"}}},"required":["input"]})",
        [this](const json::Value& input) -> json::Value {
            std::string text = input.find("input") ? input.find("input")->as_str() : "";
            std::vector<std::string> cats = {"prompt_injection", "pii", "toxic", "spam"};
            if (auto* c = input.find("categories"); c && c->is_arr()) {
                cats.clear();
                for (const auto& item : c->as_arr()) cats.push_back(item.as_str());
            }
            auto result = guard(text, cats);
            json::Value out;
            out.set("safe", result.safe);
            out.set("reason", result.reason);
            json::Value triggered_arr;
            for (const auto& t : result.triggered_categories) triggered_arr.push(t);
            out.set("triggered_categories", std::move(triggered_arr));
            return out;
        }));

    // systemone_select_relevant
    registry.add(std::make_unique<Tool>("systemone_select_relevant",
        "Context pruning: select relevant items for token budget",
        R"({"type":"object","properties":{"query":{"type":"string"},"candidates":{"type":"array","items":{"type":"string"}},"max_results":{"type":"integer"}},"required":["query","candidates"]})",
        [this](const json::Value& input) -> json::Value {
            std::string query = input.find("query") ? input.find("query")->as_str() : "";
            std::vector<std::string> candidates;
            if (auto* c = input.find("candidates"); c && c->is_arr()) {
                for (const auto& item : c->as_arr()) candidates.push_back(item.as_str());
            }
            int max_results = input.find("max_results") ? input.find("max_results")->as_int() : 10;
            auto result = select_relevant(query, candidates, max_results);
            json::Value out;
            json::Value rel_arr;
            for (const auto& r : result.relevant) rel_arr.push(r);
            out.set("relevant", std::move(rel_arr));
            out.set("recall", result.recall);
            out.set("precision", result.precision);
            return out;
        }));

    // systemone_same_intent
    registry.add(std::make_unique<Tool>("systemone_same_intent",
        "Semantic deduplication: check if two texts have same intent",
        R"({"type":"object","properties":{"a":{"type":"string"},"b":{"type":"string"}},"required":["a","b"]})",
        [this](const json::Value& input) -> json::Value {
            std::string a = input.find("a") ? input.find("a")->as_str() : "";
            std::string b = input.find("b") ? input.find("b")->as_str() : "";
            auto result = same_intent(a, b);
            json::Value out;
            out.set("same", result.same);
            out.set("confidence", result.confidence);
            return out;
        }));

    // systemone_triage
    registry.add(std::make_unique<Tool>("systemone_triage",
        "Full triage: urgency + complexity + tags + route + model suggestion",
        R"({"type":"object","properties":{"input":{"type":"string"}},"required":["input"]})",
        [this](const json::Value& input) -> json::Value {
            std::string text = input.find("input") ? input.find("input")->as_str() : "";
            auto result = triage(text);
            json::Value out;
            out.set("urgency", result.urgency);
            out.set("complexity", result.complexity);
            json::Value tags_arr;
            for (const auto& t : result.tags) tags_arr.push(t);
            out.set("tags", std::move(tags_arr));
            out.set("route", result.route);
            out.set("model_suggestion", result.model_suggestion);
            return out;
        }));

    // systemone_answerable
    registry.add(std::make_unique<Tool>("systemone_answerable",
        "RAG gate: check if query is answerable from context",
        R"({"type":"object","properties":{"query":{"type":"string"},"context":{"type":"array","items":{"type":"string"}}},"required":["query","context"]})",
        [this](const json::Value& input) -> json::Value {
            std::string query = input.find("query") ? input.find("query")->as_str() : "";
            std::vector<std::string> context;
            if (auto* c = input.find("context"); c && c->is_arr()) {
                for (const auto& item : c->as_arr()) context.push_back(item.as_str());
            }
            auto result = answerable(query, context);
            json::Value out;
            out.set("answerable", result.answerable);
            out.set("confidence", result.confidence);
            out.set("reason", result.reason);
            return out;
        }));

    // systemone_decide_program
    registry.add(std::make_unique<Tool>("systemone_decide_program",
        "Full decision pipeline: plan with persistent memory",
        R"({"type":"object","properties":{"goal":{"type":"string"},"context":{"type":"object"}},"required":["goal"]})",
        [this](const json::Value& input) -> json::Value {
            std::string goal = input.find("goal") ? input.find("goal")->as_str() : "";
            json::Value context = input.find("context") ? *input.find("context") : json::Value();
            auto result = decide_program(goal, context);
            json::Value out;
            out.set("plan", result.plan);
            json::Value steps_arr;
            for (const auto& s : result.steps) steps_arr.push(s);
            out.set("steps", std::move(steps_arr));
            out.set("feasible", result.feasible);
            return out;
        }));

    // systemone_consult - persistent memory
    registry.add(std::make_unique<Tool>("systemone_consult",
        "Consult persistent topic memory (remember/recall/consult)",
        R"({"type":"object","properties":{"topic":{"type":"string"},"question":{"type":"string"}},"required":["topic"]})",
        [this](const json::Value& input) -> json::Value {
            std::string topic = input.find("topic") ? input.find("topic")->as_str() : "";
            std::string question = input.find("question") ? input.find("question")->as_str() : "";
            std::string result = consult(topic, question);
            json::Value out;
            out.set("result", result);
            return out;
        }));
}

void SystemOneService::remember(const std::string& topic, const std::string& fact) {
    if (!store_) return;
    std::string key = "systemone:memory:" + topic;
    std::string existing;
    if (auto val = store_->get(key)) existing = *val;
    if (!existing.empty()) existing += "\n";
    existing += fact;
    store_->set(key, existing);
}

std::optional<std::string> SystemOneService::recall(const std::string& topic) {
    if (!store_) return std::nullopt;
    std::string key = "systemone:memory:" + topic;
    return store_->get(key);
}

std::string SystemOneService::consult(const std::string& topic, const std::string& question) {
    auto memory = recall(topic);
    if (!memory) return "[no memory for topic: " + topic + "]";
    
    // Use local classification to answer based on memory
    std::map<std::string, std::string> cats = {
        {"answer", "direct answer from memory"},
        {"unknown", "not in memory"}
    };
    auto result = local_classify(question, cats, 0.5);
    if (result.label == "answer") return *memory;
    return "[memory exists but question not directly answered: " + *memory + "]";
}

SystemOneService::ClassifyResult SystemOneService::classify(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    if (cfg_.prefer_local || !is_available()) {
        return local_classify(text, categories, threshold);
    }

    auto response = call_model(text, categories, threshold);
    if (!response) {
        return local_classify(text, categories, threshold);
    }

    ClassifyResult result;
    result.label = response->find("label") ? response->find("label")->as_str() : "unknown";
    result.probability = response->find("probability") ? response->find("probability")->as_double() : 0.0;
    result.confidence = response->find("confidence") ? response->find("confidence")->as_double() : 0.0;
    result.abstain = response->find("abstain") ? response->find("abstain")->as_bool() : true;
    return result;
}

SystemOneService::ClassifyMultiResult SystemOneService::classify_multi(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    if (cfg_.prefer_local || !is_available()) {
        // Local fallback for multi-label
        ClassifyMultiResult result;
        result.abstain = true;
        return result;
    }

    auto response = call_model(text, categories, threshold);
    if (!response) {
        return ClassifyMultiResult{};
    }

    ClassifyMultiResult result;
    result.abstain = response->find("abstain") ? response->find("abstain")->as_bool() : true;
    if (auto* labels = response->find("active_labels"); labels && labels->is_arr()) {
        for (const auto& item : labels->as_arr()) {
            result.active_labels.push_back(item.as_str());
        }
    }
    if (auto* scores = response->find("scores"); scores && scores->is_obj()) {
        for (const auto& [k, v] : scores->as_obj()) {
            result.scores[k] = v.as_double();
        }
    }
    return result;
}

SystemOneService::VerifyResult SystemOneService::verify(
    const std::string& premise, const std::string& hypothesis) {

    if (cfg_.prefer_local || !is_available()) {
        return VerifyResult{false, 0.0};
    }

    json::Value params;
    params.set("premise", premise);
    params.set("hypothesis", hypothesis);
    auto response = post_jsonrpc("verify", params);
    if (response.empty()) return VerifyResult{false, 0.0};

    VerifyResult result;
    // Parse response
    return result;
}

SystemOneService::GuardResult SystemOneService::guard(
    const std::string& input,
    const std::vector<std::string>& categories) {

    if (cfg_.prefer_local || !is_available()) {
        return local_guard(input, categories);
    }

    json::Value params;
    params.set("input", input);
    json::Value cats_arr;
    for (const auto& c : categories) cats_arr.push(c);
    params.set("categories", std::move(cats_arr));

    auto response = post_jsonrpc("guard", params);
    if (response.empty()) return local_guard(input, categories);

    GuardResult result;
    result.safe = true;
    return result;
}

SystemOneService::SelectRelevantResult SystemOneService::select_relevant(
    const std::string& query,
    const std::vector<std::string>& candidates,
    int max_results) {

    if (cfg_.prefer_local || !is_available()) {
        // Simple local: return first max_results
        SelectRelevantResult result;
        for (int i = 0; i < max_results && i < (int)candidates.size(); ++i) {
            result.relevant.push_back(candidates[i]);
        }
        result.recall = result.relevant.empty() ? 0.0 : 1.0;
        result.precision = 1.0;
        return result;
    }

    json::Value params;
    params.set("query", query);
    json::Value cand_arr;
    for (const auto& c : candidates) cand_arr.push(c);
    params.set("candidates", std::move(cand_arr));
    params.set("max_results", max_results);

    auto response = post_jsonrpc("select_relevant", params);
    SelectRelevantResult result;
    return result;
}

SystemOneService::SameIntentResult SystemOneService::same_intent(
    const std::string& a, const std::string& b) {

    if (cfg_.prefer_local || !is_available()) {
        return SameIntentResult{a == b, a == b ? 1.0 : 0.0};
    }

    json::Value params;
    params.set("a", a);
    params.set("b", b);
    auto response = post_jsonrpc("same_intent", params);
    SameIntentResult result;
    return result;
}

SystemOneService::TriageResult SystemOneService::triage(const std::string& input) {
    if (cfg_.prefer_local || !is_available()) {
        return local_triage(input);
    }

    json::Value params;
    params.set("input", input);
    auto response = post_jsonrpc("triage", params);
    TriageResult result;
    return result;
}

SystemOneService::AnswerableResult SystemOneService::answerable(
    const std::string& query, const std::vector<std::string>& context) {

    if (cfg_.prefer_local || !is_available()) {
        return AnswerableResult{!context.empty(), context.empty() ? 0.0 : 0.5, "local fallback"};
    }

    json::Value params;
    params.set("query", query);
    json::Value ctx_arr;
    for (const auto& c : context) ctx_arr.push(c);
    params.set("context", std::move(ctx_arr));

    auto response = post_jsonrpc("answerable", params);
    AnswerableResult result;
    return result;
}

SystemOneService::DecideProgramResult SystemOneService::decide_program(
    const std::string& goal, const json::Value& context) {

    if (cfg_.prefer_local || !is_available()) {
        return DecideProgramResult{};
    }

    json::Value params;
    params.set("goal", goal);
    params.set("context", context);
    auto response = post_jsonrpc("decide_program", params);
    DecideProgramResult result;
    return result;
}

SystemOneService::ClassifyResult SystemOneService::local_classify(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    // Simple keyword-based classification using shared categories
    ClassifyResult result;
    double best_score = 0.0;
    std::string best_label = "unknown";

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& [label, desc] : categories) {
        double score = 0.0;
        std::string lower_desc = desc;
        std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);
        
        // Simple keyword overlap
        std::istringstream iss(lower_desc);
        std::string word;
        while (iss >> word) {
            if (word.size() > 3 && lower.find(word) != std::string::npos) {
                score += 1.0;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_label = label;
        }
    }

    if (best_score >= threshold * 10.0) {  // heuristic scaling
        result.label = best_label;
        result.probability = std::min(1.0, best_score / 10.0);
        result.confidence = std::min(1.0, best_score / 10.0);
        result.abstain = false;
    } else {
        result.abstain = true;
    }
    return result;
}

SystemOneService::GuardResult SystemOneService::local_guard(
    const std::string& input,
    const std::vector<std::string>& categories) {

    GuardResult result;
    result.safe = true;

    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& cat : categories) {
        const auto& keywords = SystemOneCategories::guard_keywords(cat);
        for (const auto& kw : keywords) {
            if (lower.find(kw) != std::string::npos) {
                result.safe = false;
                result.reason = "Detected " + cat + ": " + kw;
                result.triggered_categories.push_back(cat);
                break;
            }
        }
    }
    return result;
}

SystemOneService::TriageResult SystemOneService::local_triage(const std::string& input) {
    TriageResult result;
    
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Urgency
    if (lower.find("urgent") != std::string::npos || 
        lower.find("emergency") != std::string::npos ||
        lower.find("critical") != std::string::npos ||
        lower.find("asap") != std::string::npos) {
        result.urgency = "high";
    } else if (lower.find("soon") != std::string::npos ||
               lower.find("priority") != std::string::npos) {
        result.urgency = "medium";
    } else {
        result.urgency = "low";
    }

    // Complexity
    if (lower.find("simple") != std::string::npos ||
        lower.find("quick") != std::string::npos ||
        lower.find("basic") != std::string::npos) {
        result.complexity = "simple";
    } else if (lower.find("complex") != std::string::npos ||
               lower.find("detailed") != std::string::npos ||
               lower.find("comprehensive") != std::string::npos) {
        result.complexity = "complex";
    } else {
        result.complexity = "medium";
    }

    // Tags from keywords
    for (const auto& [tag, kw] : SystemOneCategories::triage_tags()) {
        if (lower.find(kw) != std::string::npos) {
            result.tags.push_back(tag);
        }
    }

    // Route suggestion
    if (result.urgency == "high" || result.complexity == "complex") {
        result.route = "detailed_analysis";
        result.model_suggestion = "opus-4";
    } else {
        result.route = "quick_response";
        result.model_suggestion = "haiku-3";
    }

    return result;
}

std::optional<json::Value> SystemOneService::call_model(
    const std::string& prompt,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    if (cfg_.base_url.empty()) return std::nullopt;

    json::Value params;
    params.set("text", prompt);
    json::Value cats_obj;
    for (const auto& [k, v] : categories) cats_obj.set(k, v);
    params.set("categories", std::move(cats_obj));
    params.set("threshold", threshold);

    std::string response = post_jsonrpc("classify", params);
    if (response.empty()) return std::nullopt;

    try {
        return json::Value::parse(response);
    } catch (...) {
        return std::nullopt;
    }
}

std::string SystemOneService::post_jsonrpc(const std::string& method, const json::Value& params) {
    std::lock_guard<std::mutex> lock(curl_mutex_);

    CurlHandle curl;
    if (!curl.init(cfg_.base_url)) return "";

    json::Value req;
    req.set("jsonrpc", "2.0");
    req.set("id", 1);
    req.set("method", method);
    req.set("params", params);

    std::string body = req.serialize();
    std::string response;

    SlistPtr headers;
    headers = AppendSlist(headers, "Content-Type: application/json");
    if (!cfg_.api_key.empty()) {
        headers = AppendSlist(headers, "Authorization: Bearer " + cfg_.api_key);
    }

    curl.setopt(CURLOPT_POST, 1L);
    curl.setopt(CURLOPT_POSTFIELDS, body.c_str());
    curl.setopt(CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl.setopt(CURLOPT_HTTPHEADER, headers.get());
    curl.setopt(CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl.setopt(CURLOPT_WRITEDATA, &response);
    curl.setopt(CURLOPT_TIMEOUT, cfg_.timeout_secs);
    curl.setopt(CURLOPT_CONNECTTIMEOUT, 10L);

    // Apply centralized TLS security
    pluto::ApplyCurlSecurity(curl.get(), cfg_.base_url);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        log_warn("systemone", "curl error: %s", curl_easy_strerror(res));
        return "";
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        log_warn("systemone", "HTTP %ld", http_code);
        return "";
    }

    // Parse JSON-RPC response
    try {
        auto parsed = json::Value::parse(response);
        if (auto* result = parsed.find("result")) {
            return result->serialize();
        }
    } catch (...) {
        log_warn("systemone", "JSON parse failed");
    }
    return "";
}

} // namespace pluto::systemone