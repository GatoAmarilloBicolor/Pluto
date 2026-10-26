#include "LocalDecisionModel.h"
#include "../util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

namespace pluto {
namespace systemone {

LocalDecisionModel::LocalDecisionModel() {}

std::string LocalDecisionModel::to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return out;
}

double LocalDecisionModel::keyword_overlap(const std::string& a, const std::string& b) {
    std::string la = to_lower(a);
    std::string lb = to_lower(b);

    // Tokenize into words
    std::set<std::string> ta, tb;
    std::string word;
    std::istringstream sa(la);
    while (sa >> word) {
        // Strip punctuation
        std::string clean;
        for (char c : word) {
            if (std::isalnum(static_cast<unsigned char>(c))) clean += c;
        }
        if (!clean.empty()) ta.insert(clean);
    }
    std::istringstream sb(lb);
    while (sb >> word) {
        std::string clean;
        for (char c : word) {
            if (std::isalnum(static_cast<unsigned char>(c))) clean += c;
        }
        if (!clean.empty()) tb.insert(clean);
    }

    if (ta.empty() || tb.empty()) return 0.0;

    int intersection = 0;
    for (const auto& w : ta) {
        if (tb.count(w)) intersection++;
    }

    return static_cast<double>(intersection) / static_cast<double>(std::min(ta.size(), tb.size()));
}

bool LocalDecisionModel::contains_urgent_keywords(const std::string& text) {
    std::string lt = to_lower(text);
    const char* urgent[] = {
        "urgent", "emergency", "immediate", "asap", "critical",
        "production", "down", "outage", "breach", "security",
        "human", "manager", "escalate", "now", "right now"
    };
    for (const char* kw : urgent) {
        if (lt.find(kw) != std::string::npos) return true;
    }
    return false;
}

bool LocalDecisionModel::contains_toxic_keywords(const std::string& text) {
    std::string lt = to_lower(text);
    const char* toxic[] = {
        "hate", "kill", "suicide", "abuse", "harass",
        "threat", "violence", "attack", "stupid", "idiot"
    };
    for (const char* kw : toxic) {
        if (lt.find(kw) != std::string::npos) return true;
    }
    return false;
}

bool LocalDecisionModel::contains_pii_keywords(const std::string& text) {
    std::string lt = to_lower(text);
    const char* pii[] = {
        "ssn", "social security", "credit card", "password",
        "api key", "secret", "token", "private key", "address",
        "phone number", "email", "dob", "date of birth"
    };
    for (const char* kw : pii) {
        if (lt.find(kw) != std::string::npos) return true;
    }
    return false;
}

// ---------- Tool implementations ----------

ClassifyResult LocalDecisionModel::classify(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    ClassifyResult result;
    if (categories.empty()) {
        result.abstain = true;
        return result;
    }

    // Score each category by keyword overlap
    std::string best_label;
    double best_score = 0.0;

    for (const auto& [label, desc] : categories) {
        double score = keyword_overlap(text, label) * 0.5 +
                       keyword_overlap(text, desc) * 0.5;
        if (score > best_score) {
            best_score = score;
            best_label = label;
        }
    }

    result.label = best_label;
    result.probability = best_score;
    result.confidence = best_score;
    result.abstain = (best_score < threshold);
    return result;
}

ClassifyMultiResult LocalDecisionModel::classify_multi(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    ClassifyMultiResult result;
    for (const auto& [label, desc] : categories) {
        double score = keyword_overlap(text, label) * 0.5 +
                       keyword_overlap(text, desc) * 0.5;
        result.probabilities[label] = score;
        if (score >= threshold) {
            result.active_labels.push_back(label);
        }
    }
    return result;
}

VerifyResult LocalDecisionModel::verify(
    const std::string& source,
    const std::string& claim) {

    VerifyResult result;
    double overlap = keyword_overlap(source, claim);
    result.supported = (overlap >= 0.3);
    result.probability = overlap;
    return result;
}

GuardResult LocalDecisionModel::guard(
    const std::string& text,
    const std::map<std::string, std::string>& checks,
    double threshold) {

    GuardResult result;
    result.risk = "LOW";
    result.probability = 0.0;

    bool toxic = contains_toxic_keywords(text);
    bool pii = contains_pii_keywords(text);

    if (toxic || pii) {
        result.risk = "HIGH";
        result.probability = 0.8;
        if (toxic) result.hits.push_back("toxic");
        if (pii) result.hits.push_back("pii");
    } else if (contains_urgent_keywords(text)) {
        result.risk = "MEDIUM";
        result.probability = 0.5;
        result.hits.push_back("urgent");
    }

    return result;
}

SelectRelevantResult LocalDecisionModel::select_relevant(
    const std::string& question,
    const std::vector<std::string>& items,
    const std::string& kind,
    double min_p) {

    SelectRelevantResult result;
    result.total_items = static_cast<int>(items.size());

    // Score each item by keyword overlap with question
    struct ScoredItem {
        int index;
        double score;
    };
    std::vector<ScoredItem> scored;

    for (size_t i = 0; i < items.size(); i++) {
        double score = keyword_overlap(question, items[i]);
        if (score >= min_p) {
            scored.push_back({static_cast<int>(i), score});
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const ScoredItem& a, const ScoredItem& b) {
            return a.score > b.score;
        });

    for (const auto& si : scored) {
        result.selected_indices.push_back(si.index);
    }
    return result;
}

SameIntentResult LocalDecisionModel::same_intent(
    const std::string& a,
    const std::string& b,
    double threshold) {

    SameIntentResult result;
    double overlap = keyword_overlap(a, b);
    result.same = (overlap >= threshold);
    result.probability = overlap;
    return result;
}

TriageResult LocalDecisionModel::triage(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    TriageResult result;
    result.guard_risk = "LOW";

    // Classify route by keyword overlap
    std::string best_route;
    double best_score = 0.0;
    for (const auto& [route, desc] : routes) {
        double score = keyword_overlap(text, route) * 0.6 +
                       keyword_overlap(text, desc) * 0.4;
        if (score > best_score) {
            best_score = score;
            best_route = route;
        }
    }
    result.route = best_route;

    // Guard check
    bool toxic = contains_toxic_keywords(text);
    bool pii = contains_pii_keywords(text);
    if (toxic || pii) {
        result.guard_risk = "HIGH";
    } else if (contains_urgent_keywords(text)) {
        result.guard_risk = "MEDIUM";
    }

    // Urgency
    result.urgent = contains_urgent_keywords(text);

    // Tags
    for (const auto& [tag, desc] : tags) {
        if (keyword_overlap(text, tag) > 0.2 ||
            keyword_overlap(text, desc) > 0.2) {
            result.tags.push_back(tag);
        }
    }

    return result;
}

AnswerableResult LocalDecisionModel::answerable(
    const std::string& question,
    const std::string& context,
    double threshold) {

    AnswerableResult result;
    if (context.empty()) {
        result.answerable = false;
        result.probability = 0.1;
        return result;
    }

    double overlap = keyword_overlap(question, context);
    result.answerable = (overlap >= threshold);
    result.probability = overlap;
    return result;
}

ConsultResult LocalDecisionModel::consult(
    const std::string& question,
    const std::map<std::string, std::string>& options,
    const std::string& topic,
    const std::string& context) {

    ConsultResult result;
    result.abstain = true;

    if (options.empty()) return result;

    // Score options by relevance to question
    std::string best_choice;
    double best_score = 0.0;

    for (const auto& [opt, desc] : options) {
        double score = keyword_overlap(question, opt) * 0.6 +
                       keyword_overlap(question, desc) * 0.4;
        if (!context.empty()) {
            score += keyword_overlap(context, opt) * 0.2 * 0.1;
        }
        if (score > best_score) {
            best_score = score;
            best_choice = opt;
        }
    }

    if (best_score > 0.3) {
        result.choice = best_choice;
        result.probability = best_score;
        result.abstain = false;
    }
    return result;
}

DecideProgramResult LocalDecisionModel::decide_program(
    const std::string& program_json,
    const std::string& state,
    bool compact) {

    DecideProgramResult result;
    // Local model executes a simplified version of the program
    // For now, return a basic result indicating local execution
    result.results = "{\"executed\":\"local\",\"steps\":0,\"note\":\"local heuristic model\"}";
    return result;
}

} // namespace systemone
} // namespace pluto
