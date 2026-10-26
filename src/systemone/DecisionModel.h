#pragma once

#include "SystemOneConfig.h"

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace pluto {
namespace systemone {

// ============================================================================
// RESULT STRUCTS — Classification
// ============================================================================

// Single-label classification (routing, triage).
struct ClassifyResult {
    std::string label;
    double probability = 0.0;
    double confidence = 0.0;
    bool abstain = false;
};

// Multi-label classification.
struct ClassifyMultiResult {
    std::vector<std::string> active_labels;
    std::map<std::string, double> probabilities;
};

// Sentiment classification.
struct SentimentResult {
    std::string sentiment;  // positive, negative, neutral, mixed
    double score = 0.0;     // -1.0 to +1.0
    double confidence = 0.0;
};

// Priority classification.
struct PriorityResult {
    std::string priority;   // low, medium, high, critical
    double score = 0.0;     // 0.0 to 1.0
    bool escalate = false;
};

// Topic classification.
struct TopicResult {
    std::string topic;
    double confidence = 0.0;
    std::vector<std::string> subtopics;
};

// Urgency classification.
struct UrgencyResult {
    std::string urgency;    // low, medium, high, critical
    double score = 0.0;
    bool needs_immediate_action = false;
};

// ============================================================================
// RESULT STRUCTS — Verification & Safety
// ============================================================================

// Entailment check (ground RAG, validate summaries).
struct VerifyResult {
    bool supported = false;
    double probability = 0.0;
    std::string explanation;
};

// Safety gate (prompt injection, PII, toxic, spam).
struct GuardResult {
    std::string risk = "LOW";  // LOW, MEDIUM, HIGH, CRITICAL
    std::vector<std::string> hits;
    double probability = 0.0;
    std::map<std::string, double> check_probabilities;
};

// ============================================================================
// RESULT STRUCTS — Context Pruning & Token Savings
// ============================================================================

// Context pruning (90% recall, 80% precision).
struct SelectRelevantResult {
    std::vector<int> selected_indices;
    int total_items = 0;
    double compression_ratio = 1.0;
    int tokens_saved = 0;
};

// Semantic dedup (94% accuracy).
struct SameIntentResult {
    bool same = false;
    double probability = 0.0;
};

// Token-efficient extraction of key points.
struct KeyPointsResult {
    std::vector<std::string> points;
    int original_tokens = 0;
    int compressed_tokens = 0;
    double compression_ratio = 1.0;
};

// Token-efficient summarization.
struct SummarizeResult {
    std::string summary;
    int original_tokens = 0;
    int compressed_tokens = 0;
    double compression_ratio = 1.0;
    std::vector<std::string> key_entities;
};

// Entity extraction.
struct ExtractEntitiesResult {
    std::vector<std::string> entities;
    std::vector<std::string> entity_types;
    std::map<std::string, double> confidence_scores;
};

// Importance ranking for context window management.
struct RankItem {
    int index = 0;
    double score = 0.0;
    std::string reason;
};

// Context window pressure / headroom computation.
struct ContextHeadroomResult {
    int total_tokens = 0;
    int used_tokens = 0;
    int available_tokens = 0;
    double pressure_ratio = 0.0;  // 0.0 = empty, 1.0 = full
    bool needs_compaction = false;
    int recommended_compact_tokens = 0;
    std::vector<int> suggested_compact_indices;
};

// ============================================================================
// RESULT STRUCTS — Decision & Triage
// ============================================================================

// Guard + route + tags + urgency (saves 3-4 calls).
struct TriageResult {
    std::string guard_risk = "LOW";
    std::string route;
    std::vector<std::string> tags;
    bool urgent = false;
    double probability = 0.0;
    std::string reasoning;
};

// Full triage with more detail.
struct FullTriageResult {
    std::string guard_risk = "LOW";
    std::string route;
    std::vector<std::string> tags;
    bool urgent = false;
    double probability = 0.0;
    std::string reasoning;
    std::vector<std::string> recommended_actions;
    int estimated_tokens_saved = 0;
};

// RAG gate (90% at threshold 0.3).
struct AnswerableResult {
    bool answerable = false;
    double probability = 0.0;
    std::string reasoning;
};

// Decisions with persistent memory.
struct ConsultResult {
    std::string choice;
    double probability = 0.0;
    bool abstain = false;
    std::string notes_used;
};

// Decision routing (which path to take).
struct RouteDecisionResult {
    std::string route;
    double confidence = 0.0;
    std::vector<std::string> alternatives;
    std::string reasoning;
};

// Priority decision.
struct PriorityDecisionResult {
    std::string priority;
    double score = 0.0;
    bool should_escalate = false;
    std::string reasoning;
};

// Action decision (what to do next).
struct ActionDecisionResult {
    std::string action;
    std::vector<std::string> parameters;
    double confidence = 0.0;
    std::string reasoning;
    int estimated_tokens = 0;
};

// ============================================================================
// RESULT STRUCTS — Decision Program
// ============================================================================

// Full pipeline (token-efficient).
struct DecideProgramResult {
    std::string results;  // JSON string
    int tokens_saved = 0;
    int steps_executed = 0;
};

// ============================================================================
// ABSTRACT INTERFACE
// ============================================================================

class DecisionModel {
public:
    virtual ~DecisionModel() = default;

    // Returns the display name of this model.
    virtual std::string name() const = 0;

    // Returns true if this model is available/healthy.
    virtual bool is_available() = 0;

    // ---------- Classification ----------

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

    // Sentiment analysis.
    virtual SentimentResult classify_sentiment(
        const std::string& text) = 0;

    // Priority classification.
    virtual PriorityResult classify_priority(
        const std::string& text) = 0;

    // Topic classification.
    virtual TopicResult classify_topic(
        const std::string& text) = 0;

    // Urgency classification.
    virtual UrgencyResult classify_urgency(
        const std::string& text) = 0;

    // ---------- Verification & Safety ----------

    // Entailment check (ground RAG, validate summaries).
    virtual VerifyResult verify(
        const std::string& source,
        const std::string& claim) = 0;

    // Safety gate (prompt injection, PII, toxic, spam).
    virtual GuardResult guard(
        const std::string& text,
        const std::map<std::string, std::string>& checks,
        double threshold = 0.5) = 0;

    // ---------- Context Pruning & Token Savings ----------

    // Context pruning (90% recall, 80% precision).
    virtual SelectRelevantResult select_relevant(
        const std::string& question,
        const std::vector<std::string>& items,
        const std::string& kind = "chunks",
        double min_p = 0.6) = 0;

    // Extract key points from text (token-efficient).
    virtual KeyPointsResult extract_key_points(
        const std::string& text,
        int max_points = 5) = 0;

    // Summarize text (token-efficient).
    virtual SummarizeResult summarize_text(
        const std::string& text,
        int max_tokens = 200) = 0;

    // Extract entities from text.
    virtual ExtractEntitiesResult extract_entities(
        const std::string& text) = 0;

    // Rank items by importance for context window management.
    virtual std::vector<RankItem> rank_importance(
        const std::string& question,
        const std::vector<std::string>& items) = 0;

    // Compute context window headroom.
    virtual ContextHeadroomResult compute_headroom(
        int total_tokens,
        int used_tokens,
        double compaction_threshold = 0.85) = 0;

    // Compact messages to fit within context window.
    virtual std::vector<int> compact_messages(
        const std::vector<std::string>& messages,
        int max_tokens,
        double compaction_threshold = 0.85) = 0;

    // ---------- Semantic Dedup ----------

    // Semantic dedup (94% accuracy).
    virtual SameIntentResult same_intent(
        const std::string& a,
        const std::string& b,
        double threshold = 0.5) = 0;

    // ---------- Decision & Triage ----------

    // Guard + route + tags + urgency (saves 3-4 calls).
    virtual TriageResult triage(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") = 0;

    // Full triage with more detail.
    virtual FullTriageResult triage_full(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") = 0;

    // RAG gate (90% at threshold 0.3).
    virtual AnswerableResult answerable(
        const std::string& question,
        const std::string& context,
        double threshold = 0.3) = 0;

    // Route decision (which path to take).
    virtual RouteDecisionResult decide_route(
        const std::string& text,
        const std::map<std::string, std::string>& routes) = 0;

    // Priority decision.
    virtual PriorityDecisionResult decide_priority(
        const std::string& text) = 0;

    // Action decision (what to do next).
    virtual ActionDecisionResult decide_action(
        const std::string& text,
        const std::vector<std::string>& available_actions) = 0;

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

}  // namespace systemone
}  // namespace pluto
