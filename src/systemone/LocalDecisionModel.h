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
    bool contains_urgent_keywords(const std::string& text);
    bool contains_toxic_keywords(const std::string& text);
    bool contains_pii_keywords(const std::string& text);
};

} // namespace systemone
} // namespace pluto
