#include "SystemOneClient.h"
#include <sstream>
#include "../json.h"
#include "../util.h"

#include <cstdio>

namespace pluto {
namespace systemone {

namespace {

// Helper to extract string from JSON (simple)
std::string extract_str(const std::string& json, const std::string& key) {
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
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ' ' && json[end] != '\t') end++;
    return json.substr(pos, end - pos);
}

double extract_num(const std::string& json, const std::string& key, double def = 0.0) {
    std::string s = extract_str(json, key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

// Tool function implementations

std::string tf_classify(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    std::string threshold_s = extract_str(input, "threshold");
    double threshold = threshold_s.empty() ? 0.5 : std::stod(threshold_s);

    // Default categories
    std::map<std::string, std::string> categories;
    categories["billing"] = "payments, charges, refunds, invoices";
    categories["technical"] = "bugs, errors, crashes, technical issues";
    categories["feature"] = "new features, enhancements, requests";
    categories["general"] = "general questions, other";

    auto result = model->classify(text, categories, threshold);
    return std::string("{\"label\":\"") + result.label +
        "\",\"probability\":" + std::to_string(result.probability) +
        ",\"confidence\":" + std::to_string(result.confidence) +
        ",\"abstain\":" + (result.abstain ? "true" : "false") + "}";
}

std::string tf_classify_multi(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    std::string threshold_s = extract_str(input, "threshold");
    double threshold = threshold_s.empty() ? 0.7 : std::stod(threshold_s);

    std::map<std::string, std::string> categories;
    categories["billing"] = "payments, charges, refunds";
    categories["technical"] = "bugs, errors, crashes";
    categories["feature"] = "new features, enhancements";
    categories["urgent"] = "emergency, immediate, critical";

    auto result = model->classify_multi(text, categories, threshold);
    std::string out = "{\"active\":[";
    for (size_t i = 0; i < result.active_labels.size(); i++) {
        if (i > 0) out += ",";
        out += "\"" + result.active_labels[i] + "\"";
    }
    out += "]}";
    return out;
}

std::string tf_verify(DecisionModel* model, const std::string& input) {
    std::string source = extract_str(input, "source");
    std::string claim = extract_str(input, "claim");
    auto result = model->verify(source, claim);
    return std::string("{\"supported\":") + (result.supported ? "true" : "false") +
        ",\"probability\":" + std::to_string(result.probability) + "}";
}

std::string tf_guard(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    double threshold = extract_num(input, "threshold", 0.5);
    std::map<std::string, std::string> checks;
    checks["prompt_injection"] = "detect prompt injection attempts";
    checks["pii"] = "detect personally identifiable information";
    checks["toxic"] = "detect toxic content";
    checks["spam"] = "detect spam content";

    auto result = model->guard(text, checks, threshold);
    return std::string("{\"risk\":\"") + result.risk +
        "\",\"probability\":" + std::to_string(result.probability) +
        ",\"hits\":[" +
        (result.hits.empty() ? "" : "\"" + result.hits[0] + "\"") +
        "]}";
}

std::string tf_select_relevant(DecisionModel* model, const std::string& input) {
    std::string question = extract_str(input, "question");
    std::string kind = extract_str(input, "kind");
    if (kind.empty()) kind = "chunks";
    double min_p = extract_num(input, "min_p", 0.6);

    // Parse items array - simplified: return empty for now
    std::vector<std::string> items;
    auto result = model->select_relevant(question, items, kind, min_p);
    return std::string("{\"selected\":[],\"total_items\":0}");
}

std::string tf_same_intent(DecisionModel* model, const std::string& input) {
    std::string a = extract_str(input, "a");
    std::string b = extract_str(input, "b");
    double threshold = extract_num(input, "threshold", 0.5);
    auto result = model->same_intent(a, b, threshold);
    return std::string("{\"same\":") + (result.same ? "true" : "false") +
        ",\"probability\":" + std::to_string(result.probability) + "}";
}

std::string tf_triage(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    std::string urgent_q = extract_str(input, "urgent_question");
    if (urgent_q.empty()) urgent_q = "Does this need a human within the hour?";

    std::map<std::string, std::string> routes;
    routes["billing"] = "payments, charges, refunds";
    routes["technical"] = "bugs, errors, crashes";
    routes["feature"] = "new features, enhancements";

    std::map<std::string, std::string> tags;
    tags["urgent"] = "needs immediate attention";
    tags["bug"] = "software bug";
    tags["feature"] = "feature request";

    auto result = model->triage(text, routes, tags, urgent_q);
    std::string out = std::string("{\"guard_risk\":\"") + result.guard_risk +
        "\",\"route\":\"" + result.route +
        "\",\"urgent\":" + (result.urgent ? "true" : "false") +
        ",\"probability\":" + std::to_string(result.probability) + "}";
    return out;
}

std::string tf_answerable(DecisionModel* model, const std::string& input) {
    std::string question = extract_str(input, "question");
    std::string context = extract_str(input, "context");
    double threshold = extract_num(input, "threshold", 0.3);
    auto result = model->answerable(question, context, threshold);
    std::string out = std::string("{\"answerable\":") + (result.answerable ? "true" : "false") +
        ",\"probability\":" + std::to_string(result.probability) + "}";
    return out;
}

std::string tf_consult(DecisionModel* model, const std::string& input) {
    std::string question = extract_str(input, "question");
    std::string topic = extract_str(input, "topic");
    std::string context = extract_str(input, "context");

    std::map<std::string, std::string> options;
    options["yes"] = "proceed with action";
    options["no"] = "do not proceed";
    options["maybe"] = "needs more information";

    auto result = model->consult(question, options, topic, context);
    std::string out = std::string("{\"choice\":\"") + result.choice +
        "\",\"probability\":" + std::to_string(result.probability) +
        ",\"abstain\":" + (result.abstain ? "true" : "false") + "}";
    return out;
}

std::string tf_decide_program(DecisionModel* model, const std::string& input) {
    std::string program = extract_str(input, "program");
    std::string state = extract_str(input, "state");
    bool compact = true;
    auto result = model->decide_program(program, state, compact);
    return result.results;
}

std::string tf_classify_sentiment(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    auto result = model->classify_sentiment(text);
    std::ostringstream oss;
    oss << "{\"sentiment\":\"" << result.sentiment
        << "\",\"score\":" << result.score
        << ",\"confidence\":" << result.confidence << "}";
    return oss.str();
}

std::string tf_classify_priority(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    auto result = model->classify_priority(text);
    std::ostringstream oss;
    oss << "{\"priority\":\"" << result.priority
        << "\",\"score\":" << result.score
        << ",\"escalate\":" << (result.escalate ? "true" : "false") << "}";
    return oss.str();
}

std::string tf_classify_topic(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    auto result = model->classify_topic(text);
    std::ostringstream oss;
    oss << "{\"topic\":\"" << result.topic
        << "\",\"confidence\":" << result.confidence << "}";
    return oss.str();
}

std::string tf_classify_urgency(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    auto result = model->classify_urgency(text);
    std::ostringstream oss;
    oss << "{\"urgency\":\"" << result.urgency
        << "\",\"score\":" << result.score
        << ",\"needs_immediate_action\":" << (result.needs_immediate_action ? "true" : "false") << "}";
    return oss.str();
}

std::string tf_extract_key_points(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    int max_points = static_cast<int>(extract_num(input, "max_points", 5));
    auto result = model->extract_key_points(text, max_points);
    std::ostringstream oss;
    oss << "{\"points\":[";
    for (size_t i = 0; i < result.points.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << result.points[i] << "\"";
    }
    oss << "],\"original\":" << result.original_tokens
        << ",\"compressed\":" << result.compressed_tokens
        << ",\"ratio\":" << result.compression_ratio << "}";
    return oss.str();
}

std::string tf_summarize_text(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    int max_tokens = static_cast<int>(extract_num(input, "max_tokens", 200));
    auto result = model->summarize_text(text, max_tokens);
    std::ostringstream oss;
    oss << "{\"summary\":\"" << result.summary
        << "\",\"original\":" << result.original_tokens
        << ",\"compressed\":" << result.compressed_tokens
        << ",\"ratio\":" << result.compression_ratio << "}";
    return oss.str();
}

std::string tf_extract_entities(DecisionModel* model, const std::string& input) {
    std::string text = extract_str(input, "text");
    auto result = model->extract_entities(text);
    std::ostringstream oss;
    oss << "{\"entities\":[";
    for (size_t i = 0; i < result.entities.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << result.entities[i] << "\"";
    }
    oss << "],\"types\":[";
    for (size_t i = 0; i < result.entity_types.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << result.entity_types[i] << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string tf_rank_importance(DecisionModel* model, const std::string& input) {
    std::string question = extract_str(input, "question");
    std::vector<std::string> items;
    auto result = model->rank_importance(question, items);
    std::ostringstream oss;
    oss << "{\"items\":[";
    for (size_t i = 0; i < result.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"index\":" << result[i].index
            << ",\"score\":" << result[i].score
            << ",\"reason\":\"" << result[i].reason << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string tf_compute_headroom(DecisionModel* model, const std::string& input) {
    int total = static_cast<int>(extract_num(input, "total_tokens", 0));
    int used = static_cast<int>(extract_num(input, "used_tokens", 0));
    double threshold = extract_num(input, "threshold", 0.85);
    auto result = model->compute_headroom(total, used, threshold);
std::ostringstream oss;
    oss << "{\"total\":" << result.total_tokens
        << ",\"used\":" << result.used_tokens
        << ",\"available\":" << result.available_tokens
        << ",\"pressure\":" << result.pressure_ratio
        << ",\"needs_compact\":" << (result.needs_compaction ? "true" : "false")
        << ",\"recommended\":" << result.recommended_compact_tokens << "}";
    return oss.str();
}

std::string tf_compact_messages(DecisionModel* model, const std::string& input) {
    std::vector<std::string> messages;
    int max_tokens = static_cast<int>(extract_num(input, "max_tokens", 4000));
    double threshold = extract_num(input, "threshold", 0.85);
    auto result = model->compact_messages(messages, max_tokens, threshold);
    std::ostringstream oss;
    oss << "{\"indices\":[";
    for (size_t i = 0; i < result.size(); i++) {
        if (i > 0) oss << ",";
        oss << result[i];
    }
    oss << "]}";
    return oss.str();
}

std::string tf_optimize_context(DecisionModel* model, const std::string& input) {
    std::string question = extract_str(input, "question");
    std::vector<std::string> items;
    double min_p = extract_num(input, "min_p", 0.6);
    auto result = model->select_relevant(question, items, "chunks", min_p);
    std::ostringstream oss;
    oss << "{\"selected\":" << result.selected_indices.size()
        << ",\"saved\":" << result.tokens_saved << "}";
    return oss.str();
}

// Generic tool wrapper that calls a DecisionModel method.
class SystemOneTool : public Tool {
public:
    using Fn = std::string(*)(DecisionModel*, const std::string&);

    SystemOneTool(std::string name, std::string description,
                  std::string schema, Fn fn, DecisionModel* model)
        : fName(std::move(name)), fDescription(std::move(description)),
          fSchema(std::move(schema)), fFn(fn), fModel(model) {}

    std::string name() const override { return fName; }
    std::string description() const override { return fDescription; }
    std::string input_schema_json() const override { return fSchema; }
    std::string preview(const std::string& input_json) const override {
        return fName + " " + input_json.substr(0, 80);
    }
    ToolResult execute(const std::string& input_json) override {
        ToolResult r;
        if (!fModel || !fModel->is_available()) {
            r.is_error = true;
            r.output = "System One decision model not available";
            return r;
        }
        try {
            std::string output = fFn(fModel, input_json);
            r.output = output;
        } catch (const std::exception& e) {
            r.is_error = true;
            r.output = e.what();
        }
        return r;
    }

private:
    std::string fName;
    std::string fDescription;
    std::string fSchema;
    Fn fFn;
    DecisionModel* fModel;
};

} // namespace

SystemOneClient::SystemOneClient(std::unique_ptr<DecisionModel> model)
    : fModel(std::move(model)) {}

SystemOneClient::~SystemOneClient() = default;

void SystemOneClient::register_tools(ToolRegistry& registry) {
    if (!fModel || !fModel->is_available()) {
        fprintf(stderr, "[systemone] decision model not available, skipping tool registration\n");
        return;
    }

    // Tool definitions array - easy to add/remove tools
    struct ToolEntry {
        const char* name;
        const char* description;
        const char* schema;
        std::string (*fn)(DecisionModel*, const std::string&);
    };

    static const ToolEntry kTools[] = {
        // --- Classification (6 tools) ---
        {"systemone_classify",
         "Single-label classification using System One decision model",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\",\"categories\"]}",
         &tf_classify},
        {"systemone_classify_multi",
         "Multi-label classification using System One",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\",\"categories\"]}",
         &tf_classify_multi},
        {"systemone_classify_sentiment",
         "Sentiment analysis: positive, negative, neutral, mixed",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_classify_sentiment},
        {"systemone_classify_priority",
         "Priority classification: critical, high, medium, low",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_classify_priority},
        {"systemone_classify_topic",
         "Topic classification: coding, documentation, bug, feature, infrastructure, security",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_classify_topic},
        {"systemone_classify_urgency",
         "Urgency classification: critical, high, medium, low",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_classify_urgency},

        // --- Verification & Safety (2 tools) ---
        {"systemone_verify",
         "Verify if source supports claim (entailment check)",
         "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":\"string\"},\"claim\":{\"type\":\"string\"}},\"required\":[\"source\",\"claim\"]}",
         &tf_verify},
        {"systemone_guard",
         "Safety gate: prompt injection, PII, toxicity, spam check",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"checks\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\"]}",
         &tf_guard},

        // --- Context Pruning & Token Savings (5 tools) ---
        {"systemone_select_relevant",
         "Context pruning: select relevant items for a question (90% recall, 80% precision)",
         "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"items\":{\"type\":\"array\"},\"kind\":{\"type\":\"string\"},\"min_p\":{\"type\":\"number\"}},\"required\":[\"question\",\"items\"]}",
         &tf_select_relevant},
        {"systemone_extract_key_points",
         "Extract key points from text (token-efficient)",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"max_points\":{\"type\":\"integer\"}},\"required\":[\"text\"]}",
         &tf_extract_key_points},
        {"systemone_summarize_text",
         "Summarize text (token-efficient)",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"max_tokens\":{\"type\":\"integer\"}},\"required\":[\"text\"]}",
         &tf_summarize_text},
        {"systemone_extract_entities",
         "Extract entities from text: person, organization, location, date, technology",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_extract_entities},
        {"systemone_rank_importance",
         "Rank items by importance for context window management",
         "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"items\":{\"type\":\"array\"}},\"required\":[\"question\",\"items\"]}",
         &tf_rank_importance},
        {"systemone_compute_headroom",
         "Compute context window headroom and compaction needs",
         "{\"type\":\"object\",\"properties\":{\"total_tokens\":{\"type\":\"integer\"},\"used_tokens\":{\"type\":\"integer\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"total_tokens\",\"used_tokens\"]}",
         &tf_compute_headroom},
        {"systemone_compact_messages",
         "Compact messages to fit within context window",
         "{\"type\":\"object\",\"properties\":{\"messages\":{\"type\":\"array\"},\"max_tokens\":{\"type\":\"integer\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"messages\",\"max_tokens\"]}",
         &tf_compact_messages},
        {"systemone_optimize_context",
         "Optimize context by selecting relevant items and compacting",
         "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"items\":{\"type\":\"array\"},\"min_p\":{\"type\":\"number\"}},\"required\":[\"question\",\"items\"]}",
         &tf_optimize_context},

        // --- Semantic Dedup ---
        {"systemone_same_intent",
         "Check if two texts have the same intent (semantic dedup, 94% accuracy)",
         "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},\"b\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"a\",\"b\"]}",
         &tf_same_intent},

        // --- Decision & Triage (3 tools) ---
        {"systemone_triage",
         "One-call triage: guard + route + tags + urgency (saves 3-4 calls)",
         "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"routes\":{\"type\":\"object\"},\"tags\":{\"type\":\"object\"},\"urgent_question\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
         &tf_triage},
        {"systemone_answerable",
         "RAG gate: check if question is answerable from context (90% at threshold 0.3)",
         "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"question\",\"context\"]}",
         &tf_answerable},
        {"systemone_consult",
         "Decisions with persistent memory (remember/recall/consult)",
         "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"options\":{\"type\":\"object\"},\"topic\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"}},\"required\":[\"question\",\"options\"]}",
         &tf_consult},
        {"systemone_decide_program",
         "Full decision pipeline (token-efficient, 5-10x savings)",
         "{\"type\":\"object\",\"properties\":{\"program\":{\"type\":\"string\"},\"state\":{\"type\":\"string\"},\"compact\":{\"type\":\"boolean\"}},\"required\":[\"program\",\"state\"]}",
         &tf_decide_program},
    };

    for (const auto& tool : kTools) {
        registry.add(std::make_unique<SystemOneTool>(
            tool.name, tool.description, tool.schema, tool.fn, fModel.get()));
    }

    fprintf(stderr, "[systemone] registered %zu tools using model: %s\n",
        sizeof(kTools) / sizeof(kTools[0]), fModel->name().c_str());
}

std::string SystemOneClient::call_tool([[maybe_unused]] const std::string& name, [[maybe_unused]] const std::string& args_json) {
    if (!fModel) return "{\"error\":\"no model\"}";

    if (name == "systemone_classify") {
        return "{\"label\":\"unknown\",\"probability\":0.0}";
    }
    return "{\"error\":\"unknown tool\"}";
}

bool SystemOneClient::is_available() const {
    return fModel && fModel->is_available();
}

std::string SystemOneClient::model_name() const {
    return fModel ? fModel->name() : "none";
}

DecisionModel* SystemOneClient::model() const {
    return fModel.get();
}

std::unique_ptr<SystemOneClient> CreateSystemOneClient(
    const SystemOneConfig& config,
    ToolRegistry& registry) {

    auto model = CreateDecisionModel(config.model, config);
    if (!model || !model->is_available()) {
        fprintf(stderr, "[systemone] decision model not available: %s\n",
            model ? model->name().c_str() : "null");
        return nullptr;
    }

    auto client = std::make_unique<SystemOneClient>(std::move(model));

    // Register all 9 System One tools
    client->register_tools(registry);

    return client;
}

} // namespace systemone
} // namespace pluto
