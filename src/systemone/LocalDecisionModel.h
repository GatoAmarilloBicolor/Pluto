#pragma once

#include "DecisionModel.h"
#include "SystemOneConfig.h"

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pluto {
namespace systemone {

// Local decision model that uses keyword-based heuristics and simple
// statistical analysis. Works offline without any external server.
// Useful as a fallback when remote servers are unavailable.
class LocalDecisionModel : public DecisionModel {
public:
    LocalDecisionModel();
    ~LocalDecisionModel() override = default;

    std::string name() const override { return "local"; }
    bool is_available() override { return true; }  // Always available

    ClassifyResult classify(
        const std::string& text,
        const std::map<std::string, std::string>& categories,
        double threshold = 0.5) override;

    ClassifyMultiResult classify_multi(
        const std::string& text,
        const std::map<std::string, std::string>& categories,
        double threshold = 0.7) override;

    SentimentResult classify_sentiment(
        const std::string& text) override;

    PriorityResult classify_priority(
        const std::string& text) override;

    TopicResult classify_topic(
        const std::string& text) override;

    UrgencyResult classify_urgency(
        const std::string& text) override;

    VerifyResult verify(
        const std::string& source,
        const std::string& claim) override;

    GuardResult guard(
        const std::string& text,
        const std::map<std::string, std::string>& checks,
        double threshold = 0.5) override;

    SelectRelevantResult select_relevant(
        const std::string& question,
        const std::vector<std::string>& items,
        const std::string& kind = "chunks",
        double min_p = 0.6) override;

    KeyPointsResult extract_key_points(
        const std::string& text,
        int max_points = 5) override;

    SummarizeResult summarize_text(
        const std::string& text,
        int max_tokens = 200) override;

    ExtractEntitiesResult extract_entities(
        const std::string& text) override;

    std::vector<RankItem> rank_importance(
        const std::string& question,
        const std::vector<std::string>& items) override;

    ContextHeadroomResult compute_headroom(
        int total_tokens,
        int used_tokens,
        double compaction_threshold = 0.85) override;

    std::vector<int> compact_messages(
        const std::vector<std::string>& messages,
        int max_tokens,
        double compaction_threshold = 0.85) override;

    SameIntentResult same_intent(
        const std::string& a,
        const std::string& b,
        double threshold = 0.5) override;

    TriageResult triage(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") override;

    AnswerableResult answerable(
        const std::string& question,
        const std::string& context,
        double threshold = 0.3) override;

    FullTriageResult triage_full(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") override;

    RouteDecisionResult decide_route(
        const std::string& text,
        const std::map<std::string, std::string>& routes) override;

    PriorityDecisionResult decide_priority(
        const std::string& text) override;

    ActionDecisionResult decide_action(
        const std::string& text,
        const std::vector<std::string>& available_actions) override;

    ConsultResult consult(
        const std::string& question,
        const std::map<std::string, std::string>& options,
        const std::string& topic = "",
        const std::string& context = "") override;

    DecideProgramResult decide_program(
        const std::string& program_json,
        const std::string& state,
        bool compact = true) override;

private:
    std::string to_lower(const std::string& s);
    double keyword_overlap(const std::string& a, const std::string& b);
    bool contains_keyword(const std::string& text,
                          std::initializer_list<const char*> keywords);
    std::string best_route(const std::string& text,
                           const std::map<std::string, std::string>& routes,
                           double& best_score_out);
};

} // namespace systemone
} // namespace pluto