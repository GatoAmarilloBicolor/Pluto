#include "SystemOneClient.h"
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

    // Register all 9 System One tools
    registry.add(std::make_unique<SystemOneTool>(
        "systemone_classify",
        "Single-label classification using System One decision model",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\",\"categories\"]}",
        tf_classify,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_classify_multi",
        "Multi-label classification using System One",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"categories\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\",\"categories\"]}",
        tf_classify_multi,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_verify",
        "Verify if source supports claim (entailment check)",
        "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":\"string\"},\"claim\":{\"type\":\"string\"}},\"required\":[\"source\",\"claim\"]}",
        tf_verify,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_guard",
        "Safety gate: prompt injection, PII, toxicity, spam check",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"checks\":{\"type\":\"object\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"text\"]}",
        tf_guard,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_select_relevant",
        "Context pruning: select relevant items for a question",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"items\":{\"type\":\"array\"},\"kind\":{\"type\":\"string\"},\"min_p\":{\"type\":\"number\"}},\"required\":[\"question\",\"items\"]}",
        tf_select_relevant,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_same_intent",
        "Check if two texts have the same intent (semantic dedup)",
        "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},\"b\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"a\",\"b\"]}",
        tf_same_intent,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_triage",
        "One-call triage: guard + route + tags + urgency",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"routes\":{\"type\":\"object\"},\"tags\":{\"type\":\"object\"},\"urgent_question\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
        tf_triage,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_answerable",
        "RAG gate: check if question is answerable from context",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"},\"threshold\":{\"type\":\"number\"}},\"required\":[\"question\",\"context\"]}",
        tf_answerable,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_consult",
        "Decision with persistent memory (consult)",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\"},\"options\":{\"type\":\"object\"},\"topic\":{\"type\":\"string\"},\"context\":{\"type\":\"string\"}},\"required\":[\"question\",\"options\"]}",
        tf_consult,
        fModel.get()));

    registry.add(std::make_unique<SystemOneTool>(
        "systemone_decide_program",
        "Full decision pipeline (token-efficient)",
        "{\"type\":\"object\",\"properties\":{\"program\":{\"type\":\"string\"},\"state\":{\"type\":\"string\"},\"compact\":{\"type\":\"boolean\"}},\"required\":[\"program\",\"state\"]}",
        tf_decide_program,
        fModel.get()));

    fprintf(stderr, "[systemone] registered 9 tools using model: %s\n",
        fModel->name().c_str());
}

std::string SystemOneClient::call_tool(const std::string& name, const std::string& args_json) {
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
