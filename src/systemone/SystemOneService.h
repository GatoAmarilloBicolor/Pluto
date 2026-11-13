#pragma once

#include "dsh/Context.h"
#include "tools/Tool.h"
#include "storage/SessionStore.h"
#include "json.h"
#include "config.h"
#include "curl_security.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>

namespace pluto::systemone {

struct ClassifyResult {
    std::string label;
    double probability = 0.0;
    double confidence = 0.0;
    bool abstain = true;
};

struct ClassifyMultiResult {
    std::vector<std::string> active_labels;
    std::map<std::string, double> scores;
    bool abstain = true;
};

struct VerifyResult {
    bool entails = false;
    double confidence = 0.0;
};

struct GuardResult {
    bool safe = true;
    std::string reason;
    std::vector<std::string> triggered_categories;
};

struct SelectRelevantResult {
    std::vector<std::string> relevant;
    double recall = 0.0;
    double precision = 0.0;
};

struct SameIntentResult {
    bool same = false;
    double confidence = 0.0;
};

struct TriageResult {
    std::string urgency;
    std::string complexity;
    std::vector<std::string> tags;
    std::string route;
    std::string model_suggestion;
};

struct AnswerableResult {
    bool answerable = false;
    double confidence = 0.0;
    std::string reason;
};

struct DecideProgramResult {
    json::Value plan;
    std::vector<std::string> steps;
    bool feasible = true;
};

class SystemOneService {
public:
    explicit SystemOneService(pluto::dsh::Context& ctx,
                               std::shared_ptr<ToolRegistry> tools,
                               std::shared_ptr<SessionStore> store,
                               const SystemOneConfig& cfg);
    
    void register_tools(ToolRegistry& registry);
    
    // Memoria persistente
    void remember(const std::string& topic, const std::string& fact);
    std::optional<std::string> recall(const std::string& topic);
    std::string consult(const std::string& topic, const std::string& question);
    
    // Núcleo de decisión - 9 herramientas reales
    ClassifyResult classify(const std::string& text, 
                           const std::map<std::string, std::string>& categories,
                           double threshold = 0.5);
    ClassifyMultiResult classify_multi(const std::string& text,
                                      const std::map<std::string, std::string>& categories,
                                      double threshold = 0.5);
    VerifyResult verify(const std::string& premise, const std::string& hypothesis);
    GuardResult guard(const std::string& input, 
                     const std::vector<std::string>& categories = {"prompt_injection", "pii", "toxic", "spam"});
    SelectRelevantResult select_relevant(const std::string& query,
                                        const std::vector<std::string>& candidates,
                                        int max_results = 10);
    SameIntentResult same_intent(const std::string& a, const std::string& b);
    TriageResult triage(const std::string& input);
    AnswerableResult answerable(const std::string& query, 
                               const std::vector<std::string>& context);
    DecideProgramResult decide_program(const std::string& goal, 
                                      const json::Value& context);
    
    bool is_available() const { return cfg_.enabled; }
    const SystemOneConfig& config() const { return cfg_; }

private:
    pluto::dsh::Context& ctx_;
    std::shared_ptr<ToolRegistry> tools_;
    std::shared_ptr<SessionStore> store_;
    SystemOneConfig cfg_;
    mutable std::mutex curl_mutex_;
    
    // HTTP helpers
    std::string post_jsonrpc(const std::string& method, const json::Value& params);
    std::optional<json::Value> call_model(const std::string& prompt, 
                                         const std::map<std::string, std::string>& categories,
                                         double threshold);
    
    // Fallback local heuristics
    ClassifyResult local_classify(const std::string& text,
                                 const std::map<std::string, std::string>& categories,
                                 double threshold);
    GuardResult local_guard(const std::string& input,
                           const std::vector<std::string>& categories);
    TriageResult local_triage(const std::string& input);
};

} // namespace pluto::systemone