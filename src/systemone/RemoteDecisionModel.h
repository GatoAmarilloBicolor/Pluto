#pragma once

#include "DecisionModel.h"
#include "SystemOneConfig.h"

#include <string>
#include <map>
#include <memory>
#include <curl/curl.h>

namespace pluto {
namespace systemone {

// Remote decision model that communicates with kev/laya/jev servers via HTTP.
class RemoteDecisionModel : public DecisionModel {
public:
    RemoteDecisionModel(DecisionModelType type, const SystemOneConfig& config);
    ~RemoteDecisionModel() override;

    std::string name() const override;
    bool is_available() override;

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

    FullTriageResult triage_full(
        const std::string& text,
        const std::map<std::string, std::string>& routes,
        const std::map<std::string, std::string>& tags,
        const std::string& urgent_question = "Does this need a human within the hour?") override;

    AnswerableResult answerable(
        const std::string& question,
        const std::string& context,
        double threshold = 0.3) override;

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
    DecisionModelType fType;
    SystemOneConfig fConfig;
    std::string fServerUrl;
    std::string fApiKey;
    bool fAvailable = false;
    CURL* fCurl = nullptr;

    std::string resolve_server_url();
    bool init_curl();
    std::string post_jsonrpc(const std::string& method, const std::string& params_json);
    bool parse_classify_result(const std::string& json_str, ClassifyResult& out);
    bool parse_classify_multi_result(const std::string& json_str, ClassifyMultiResult& out);
    bool parse_verify_result(const std::string& json_str, VerifyResult& out);
    bool parse_guard_result(const std::string& json_str, GuardResult& out);
    bool parse_select_relevant_result(const std::string& json_str, SelectRelevantResult& out);
    bool parse_same_intent_result(const std::string& json_str, SameIntentResult& out);
    bool parse_triage_result(const std::string& json_str, TriageResult& out);
    bool parse_answerable_result(const std::string& json_str, AnswerableResult& out);
    bool parse_consult_result(const std::string& json_str, ConsultResult& out);
    bool parse_decide_program_result(const std::string& json_str, DecideProgramResult& out);
};

} // namespace systemone
} // namespace pluto