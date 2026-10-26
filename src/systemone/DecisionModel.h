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
        double threshold = 0.5) {
        ClassifyResult r;
        r.label = "unknown";
        r.probability = 0.0;
        r.confidence = 0.0;
        r.abstain = true;
        return r;
    }

    // Multi-label classification.
    virtual ClassifyMultiResult classify_multi(
        const std::string& text,
        const std::map<std::string, std::string>& categories,
        double threshold = 0.7) {
        ClassifyMultiResult r;
        return r;
    }

    // Sentiment analysis.
    virtual SentimentResult classify_sentiment(
        const std::string& text) {
        SentimentResult r;
        r.sentiment = "neutral";
        r.score = 0.0;
        r.confidence = 0.0;
        return r;
    }

    // Priority classification.
    virtual PriorityResult classify_priority(
        const std::string& text) {
        PriorityResult r;
        r.priority = "medium";
        r.score = 0.5;
        r.escalate = false;
        return r;
    }

    // Topic classification.
    virtual TopicResult classify_topic(
        const std::string& text) {
        TopicResult r;
        r.topic = "unknown";
        r.confidence = 0.0;
        return r;
    }

    // Urgency classification.
    virtual UrgencyResult classify_urgency(
        const std::string& text) {
        UrgencyResult r;
        r.urgency = "low";
        r.score = 0.0;
        r.needs_immediate_action = false;
        return r;
    }

    // ---------- Verification & Safety ----------

    // Entailment check (ground RAG, validate summaries).
    virtual VerifyResult verify(
        const std::string& source,
        const std::string& claim) {
        VerifyResult r;
        r.supported = false;
        r.probability = 0.0;
        r.explanation = "Not implemented";
        return r;
    }

    // Safety gate (prompt injection, PII, toxic, spam).
    virtual GuardResult guard(
        const std::string& text,
        const std::map<std::string, std::string>& checks,
        double threshold = 0.5) {
        GuardResult r;
        r.risk = "LOW";
        r.probability = 0.0;
        return r;
    }

    // ---------- Context Pruning & Token Savings ----------

    // Context pruning (90% recall, 80% precision).
    virtual SelectRelevantResult select_relevant(
        const std::string& question,
        const std::vector<std::string>& items,
        const std::string& kind = "chunks",
        double min_p = 0.6) {
        SelectRelevantResult r;
        r.total_items = 0;
        r.compression_ratio = 1.0;
        r.tokens_saved = 0;
        return r;
    }

    // Extract key points from text (token-efficient).
    virtual KeyPointsResult extract_key_points(
        const std::string& text,
        int max_points = 5) {
        KeyPointsResult r;
        r.original_tokens = 0;
        r.compressed_tokens = 0;
        r.compression_ratio = 1.0;
        return r;
    }

    // Summarize text (token-efficient).
    virtual SummarizeResult summarize_text(
        const std::string& text,
        int max_tokens = 200) {
        SummarizeResult r;
        r.original_tokens = 0;
        r.compressed_tokens = 0;
        r.compression_ratio = 1.0;
        return r;
    }

    // Extract entities from text.
    virtual ExtractEntitiesResult extract_entities(
        const std::string& text) {
        return {};
    }

    // Rank items by importance for context window management.
    virtual std::vector<RankItem> rank_importance(
        const std::string& question,
        const std::vector<std::string>& items) {
        return {};
    }

    // Compute context window headroom.
    virtual ContextHeadroomResult compute_headroom(
        int total_tokens,
        int used_tokens,
        double compaction_threshold = 0.85) {
        ContextHeadroomResult r;
        r.total_tokens = total_tokens;
        r.used_tokens = used_tokens;
        r.available_tokens = total_tokens - used_tokens;
        r.pressure_ratio = total_tokens > 0 ? double(used_tokens) / total_tokens : 0.0;
        r.needs_compaction = r.pressure_ratio >= compaction_threshold;
        return r;
    }

    // Compact messages to fit within context window.
    virtual std::vector<int> compact_messages(
        const std::vector<std::string>& messages,
        int max_tokens,
        double compaction_threshold = 0.85) {
        return {};
    }

    // ---------- Semantic Dedup ----------

    // Semantic dedup (94% accuracy).
    virtual SameIntentResult same_intent(
        const std::string& a,
        const std::string& b,
        double threshold = 0.5) {
        SameIntentResult r;
        r.same = false;
        r.probability = 0.0;
        return r;
    }

    // ---------- Decision & Triage ----------

    // Guard + route + tags + urgency (saves 3-4 calls).
    virtual TriageResult triage(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") {
        TriageResult r;
        r.guard_risk = "LOW";
        r.urgent = False;
        r.probability = 0.0;
        return r;
    }

    // Full triage with more detail.
    virtual FullTriageResult triage_full(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") {
        FullTriageResult r;
        r.guard_risk = "LOW";
        r.urgent = false;
        r.probability = 0.0;
        return r;
    }

    // RAG gate (90% at threshold 0.3).
    virtual AnswerableResult answerable(
        const std::string& question,
        const std::string& context,
        double threshold = 0.3) {
        AnswerableResult r;
        r.answerable = False;
        r.probability = 0.0;
        r.reasoning = "Not implemented";
        return r;
    }

    // Route decision (which path to take).
    virtual RouteDecisionResult decide_route(
        const std::string& text,
        const std::map<std::string, std::string>& routes) {
        RouteDecisionResult r;
        r.route = "unknown";
        r.confidence = 0.0;
        return r;
    }

    // Priority decision.
    virtual PriorityDecisionResult decide_priority(
        const std::string& text) {
        PriorityDecisionResult r;
        r.priority = "medium";
        r.score = 0.5;
        r.should_escalate = false;
        return r;
    }

    // Action decision (what to do next).
    virtual ActionDecisionResult decide_action(
        const std::string& text,
        const std::vector<std::string>& available_actions) {
        ActionDecisionResult r;
        r.action = "none";
        r.confidence = 0.0;
        return r;
    }

    // Decisions with persistent memory.
    virtual ConsultResult consult(
        const std::string& question,
        const std::map<std::string, std::string>& options,
        const std::string& topic = "",
        const std::string& context = "") {
        ConsultResult r;
        r.choice = "";
        r.probability = 0.0;
        r.abstain = True;
        r.notes_used = "";
        return r;
    }

    // Full pipeline (token-efficient).
    virtual DecideProgramResult decide_program(
        const std::string& program_json,
        const std::string& state,
        bool compact = True) {
        DecideProgramResult r;
        r.results = "{}";
        r.tokens_saved = 0;
        r.steps_executed = 0;
        return r;
    }
};

// Create a DecisionModel based on type and config.
std::unique_ptr<DecisionModel> CreateDecisionModel(
    DecisionModelType type,
    const SystemOneConfig& config);

}  // namespace systemone
}  // namespace pluto
