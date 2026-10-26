#pragma once

#include "SystemOneConfig.h"

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace pluto {
namespace systemone {

// Result of a single-label classification.
struct ClassifyResult {
    std::string label;
    double probability = 0.0;
    double confidence = 0.0;
    bool abstain = false;
};

// Result of multi-label classification.
struct ClassifyMultiResult {
    std::vector<std::string> active_labels;
    std::map<std::string, double> probabilities;
};

// Result of verification (entailment check).
struct VerifyResult {
    bool supported = false;
    double probability = 0.0;
};

// Result of safety guard check.
struct GuardResult {
    std::string risk = "LOW";  // LOW, MEDIUM, HIGH, CRITICAL
    std::vector<std::string> hits;
    double probability = 0.0;
};

// Result of context pruning.
struct SelectRelevantResult {
    std::vector<int> selected_indices;
    int total_items = 0;
};

// Result of semantic dedup (same intent check).
struct SameIntentResult {
    bool same = false;
    double probability = 0.0;
};

// Result of triage (guard + route + tags + urgency).
struct TriageResult {
    std::string guard_risk = "LOW";
    std::string route;
    std::vector<std::string> tags;
    bool urgent = false;
    double probability = 0.0;
};

// Result of answerable check (RAG gate).
struct AnswerableResult {
    bool answerable = false;
    double probability = 0.0;
};

// Result of consult (decision with memory).
struct ConsultResult {
    std::string choice;
    double probability = 0.0;
    bool abstain = false;
    std::string notes_used;
};

// Result of decide_program (full pipeline).
struct DecideProgramResult {
    std::string results;  // JSON string
};

// Abstract interface for decision models.
class DecisionModel {
public:
    virtual ~DecisionModel() = default;

    // Returns the display name of this model.
    virtual std::string name() const = 0;

    // Returns true if this model is available/healthy.
    virtual bool is_available() = 0;

    // Single-label classification (routing, triage).
    virtual ClassifyResult classify(
        const std::string& text,
        const std::map<std::string, std::string>& categories,
        double threshold = 0.5) = 0;

    // Multi-label classification.
    virtual ClassifyMultiResult classify_multi(
        const std::string& text,
        const std::map<std::string, std::string>& categories,
        double threshold = 0.7) = 0;

    // Entailment check (ground RAG, validate summaries).
    virtual VerifyResult verify(
        const std::string& source,
        const std::string& claim) = 0;

    // Safety gate (prompt injection, PII, toxic, spam).
    virtual GuardResult guard(
        const std::string& text,
        const std::map<std::string, std::string>& checks,
        double threshold = 0.5) = 0;

    // Context pruning (90% recall, 80% precision).
    virtual SelectRelevantResult select_relevant(
        const std::string& question,
        const std::vector<std::string>& items,
        const std::string& kind = "chunks",
        double min_p = 0.6) = 0;

    // Semantic dedup (94% accuracy).
    virtual SameIntentResult same_intent(
        const std::string& a,
        const std::string& b,
        double threshold = 0.5) = 0;

    // Guard + route + tags + urgency (saves 3-4 calls).
    virtual TriageResult triage(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") = 0;

    // RAG gate (90% at threshold 0.3).
    virtual AnswerableResult answerable(
        const std::string& question,
        const std::string& context,
        double threshold = 0.3) = 0;

    // Decisions with persistent memory.
    virtual ConsultResult consult(
        const std::string& question,
        const std::map<std::string, std::string>& options,
        const std::string& topic = "",
        const std::string& context = "") = 0;

    // Full pipeline (token-efficient).
    virtual DecideProgramResult decide_program(
        const std::string& program_json,
        const std::string& state,
        bool compact = true) = 0;
};

// Create a DecisionModel based on type and config.
std::unique_ptr<DecisionModel> CreateDecisionModel(
    DecisionModelType type,
    const SystemOneConfig& config);

} // namespace systemone
} // namespace pluto
