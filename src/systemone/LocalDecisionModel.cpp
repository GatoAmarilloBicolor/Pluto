#include "LocalDecisionModel.h"
#include "../util.h"
#include "TokenSaver.h"

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

    std::set<std::string> ta, tb;
    std::string word;
    std::istringstream sa(la);
    while (sa >> word) {
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

bool LocalDecisionModel::contains_keyword(const std::string& text,
                                          std::initializer_list<const char*> keywords) {
    std::string lt = to_lower(text);
    for (const char* kw : keywords) {
        if (lt.find(kw) != std::string::npos) return true;
    }
    return false;
}

// ---------- Classification ----------

ClassifyResult LocalDecisionModel::classify(
    const std::string& text,
    const std::map<std::string, std::string>& categories,
    double threshold) {

    ClassifyResult result;
    if (categories.empty()) {
        result.abstain = true;
        return result;
    }

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

SentimentResult LocalDecisionModel::classify_sentiment(
    const std::string& text) {

    SentimentResult result;
    std::map<std::string, std::string> categories;
    categories["positive"] = "positive sentiment, happy, good, great, excellent";
    categories["negative"] = "negative sentiment, sad, bad, terrible, awful";
    categories["neutral"] = "neutral sentiment, neither positive nor negative";
    categories["mixed"] = "mixed sentiment, both positive and negative";

    auto r = classify(text, categories, 0.3);
    result.sentiment = r.label;
    result.score = r.probability;
    if (result.sentiment == "positive") result.score = std::abs(result.score);
    if (result.sentiment == "negative") result.score = -std::abs(result.score);
    result.confidence = r.confidence;
    return result;
}

PriorityResult LocalDecisionModel::classify_priority(
    const std::string& text) {

    PriorityResult result;
    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down, security breach";
    categories["high"] = "high priority, urgent, important, asap";
    categories["medium"] = "medium priority, normal, standard";
    categories["low"] = "low priority, minor, trivial, cosmetic";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.escalate = (r.label == "critical" || r.label == "high");
    return result;
}

TopicResult LocalDecisionModel::classify_topic(
    const std::string& text) {

    TopicResult result;
    std::map<std::string, std::string> categories;
    categories["coding"] = "programming, code, software development, debugging";
    categories["documentation"] = "documentation, README, guides, tutorials";
    categories["bug"] = "bug report, error, issue, crash";
    categories["feature"] = "feature request, enhancement, new functionality";
    categories["infrastructure"] = "infrastructure, deployment, devops, ci/cd";
    categories["security"] = "security, vulnerability, patch, exploit";

    auto r = classify(text, categories, 0.3);
    result.topic = r.label;
    result.confidence = r.confidence;
    return result;
}

UrgencyResult LocalDecisionModel::classify_urgency(
    const std::string& text) {

    UrgencyResult result;
    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, immediate action required";
    categories["high"] = "high urgency, urgent, important, time-sensitive";
    categories["medium"] = "medium urgency, normal priority";
    categories["low"] = "low urgency, can wait, minor";

    auto r = classify(text, categories, 0.3);
    result.urgency = r.label;
    result.score = r.probability;
    result.needs_immediate_action = (r.label == "critical");
    return result;
}

// ---------- Verification & Safety ----------

VerifyResult LocalDecisionModel::verify(
    const std::string& source,
    const std::string& claim) {

    VerifyResult result;
    double overlap = keyword_overlap(source, claim);
    result.supported = (overlap >= 0.3);
    result.probability = overlap;
    result.explanation = overlap >= 0.3 ?
        "Source supports claim with " + std::to_string(static_cast<int>(overlap * 100)) + "% keyword overlap" :
        "Source does not sufficiently support claim";
    return result;
}

GuardResult LocalDecisionModel::guard(
    const std::string& text,
    const std::map<std::string, std::string>& checks,
    double threshold) {

    GuardResult result;
    result.risk = "LOW";
    result.probability = 0.0;

    bool toxic = contains_keyword(text, {"hate","kill","suicide","abuse","harass",
        "threat","violence","attack","stupid","idiot"});
    bool pii = contains_keyword(text, {"ssn","social security","credit card","password",
        "api key","secret","token","private key","address",
        "phone number","email","dob","date of birth"});
    bool urgent = contains_keyword(text, {"urgent","emergency","immediate","asap","critical",
        "production","down","outage","breach","security",
        "human","manager","escalate","now","right now"});

    if (toxic || pii) {
        result.risk = "HIGH";
        result.probability = 0.8;
        if (toxic) result.hits.push_back("toxic");
        if (pii) result.hits.push_back("pii");
    } else if (urgent) {
        result.risk = "MEDIUM";
        result.probability = 0.5;
        result.hits.push_back("urgent");
    }

    return result;
}

// ---------- Context Pruning & Token Savings ----------

SelectRelevantResult LocalDecisionModel::select_relevant(
    const std::string& question,
    const std::vector<std::string>& items,
    const std::string& kind,
    double min_p) {

    SelectRelevantResult result;
    result.total_items = static_cast<int>(items.size());

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

    std::sort(scored.begin(), scored.end(),
        [](const ScoredItem& a, const ScoredItem& b) {
            return a.score > b.score;
        });

    for (const auto& si : scored) {
        result.selected_indices.push_back(si.index);
    }

    // Estimate compression
    int original_tokens = 0;
    for (const auto& m : items) original_tokens += estimate_tokens(m);
    int compressed_tokens = 0;
    for (int idx : result.selected_indices) {
        compressed_tokens += estimate_tokens(items[idx]);
    }
    result.compression_ratio = static_cast<double>(compressed_tokens) /
                               static_cast<double>(original_tokens);
    result.tokens_saved = original_tokens - compressed_tokens;

    return result;
}

KeyPointsResult LocalDecisionModel::extract_key_points(
    const std::string& text,
    int max_points) {

    KeyPointsResult result;
    result.original_tokens = estimate_tokens(text);

    std::string key_sentences = extract_key_sentences(text, max_points);
    result.points.push_back(key_sentences);

    result.compressed_tokens = estimate_tokens(key_sentences);
    result.compression_ratio = compute_compression_ratio(text, key_sentences);
    return result;
}

SummarizeResult LocalDecisionModel::summarize_text(
    const std::string& text,
    int max_tokens) {

    SummarizeResult result;
    result.original_tokens = estimate_tokens(text);

    result.summary = compress_message(text, max_tokens);
    result.compressed_tokens = estimate_tokens(result.summary);
    result.compression_ratio = compute_compression_ratio(text, result.summary);
    return result;
}

ExtractEntitiesResult LocalDecisionModel::extract_entities(
    const std::string& text) {

    ExtractEntitiesResult result;
    // Simple keyword-based entity extraction
    std::map<std::string, std::string> categories;
    categories["person"] = "person name, human, individual";
    categories["organization"] = "company, organization, institution";
    categories["location"] = "place, city, country, location";
    categories["date"] = "date, time, when";
    categories["technology"] = "technology, software, hardware, tool";

    auto r = classify_multi(text, categories, 0.3);
    result.entities = r.active_labels;

    for (const auto& e : result.entities) {
        result.entity_types.push_back(e);
        result.confidence_scores[e] = 0.7;
    }

    return result;
}

std::vector<RankItem> LocalDecisionModel::rank_importance(
    const std::string& question,
    const std::vector<std::string>& items) {

    std::vector<RankItem> result;

    for (size_t i = 0; i < items.size(); i++) {
        RankItem ri;
        ri.index = static_cast<int>(i);
        ri.score = keyword_overlap(question, items[i]);
        ri.reason = "keyword overlap score: " + std::to_string(ri.score);
        result.push_back(ri);
    }

    std::sort(result.begin(), result.end(),
        [](const RankItem& a, const RankItem& b) {
            return a.score > b.score;
        });

    return result;
}

ContextHeadroomResult LocalDecisionModel::compute_headroom(
    int total_tokens,
    int used_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::compute_context_headroom(
        total_tokens, used_tokens, compaction_threshold);
}

std::vector<int> LocalDecisionModel::compact_messages(
    const std::vector<std::string>& messages,
    int max_tokens,
    double compaction_threshold) {
    return ::pluto::systemone::select_messages_to_compact(
        messages, "", max_tokens, compaction_threshold);
}

// ---------- Semantic Dedup ----------

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

// ---------- Decision & Triage ----------

std::string LocalDecisionModel::best_route(const std::string& text,
                              const std::map<std::string, std::string>& routes,
                              double& best_score_out) {
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
    best_score_out = best_score;
    return best_route;
}

TriageResult LocalDecisionModel::triage(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    TriageResult result;
    result.guard_risk = "LOW";

    double best_score = 0.0;
    result.route = best_route(text, routes, best_score);

    bool toxic = contains_keyword(text, {"hate","kill","suicide","abuse","harass",
        "threat","violence","attack","stupid","idiot"});
    bool pii = contains_keyword(text, {"ssn","social security","credit card","password",
        "api key","secret","token","private key","address",
        "phone number","email","dob","date of birth"});
    if (toxic || pii) {
        result.guard_risk = "HIGH";
    } else if (contains_keyword(text, {"urgent","emergency","immediate","asap","critical",
        "production","down","outage","breach","security",
        "human","manager","escalate","now","right now"})) {
        result.guard_risk = "MEDIUM";
    }

    result.urgent = contains_keyword(text, {"urgent","emergency","immediate","asap","critical",
        "production","down","outage","breach","security",
        "human","manager","escalate","now","right now"});
    result.probability = result.urgent ? 0.8 : 0.3;

    for (const auto& [tag, desc] : tags) {
        if (keyword_overlap(text, tag) > 0.2 ||
            keyword_overlap(text, desc) > 0.2) {
            result.tags.push_back(tag);
        }
    }

    return result;
}

FullTriageResult LocalDecisionModel::triage_full(
    const std::string& text,
    const std::map<std::string, std::string>& routes,
    const std::map<std::string, std::string>& tags,
    const std::string& urgent_question) {

    FullTriageResult result;
    result.guard_risk = "LOW";

    double best_score = 0.0;
    (void)best_score;
    result.route = best_route(text, routes, best_score);

    bool toxic = contains_keyword(text, {"hate","kill","suicide","abuse","harass",
        "threat","violence","attack","stupid","idiot"});
    bool pii = contains_keyword(text, {"ssn","social security","credit card","password",
        "api key","secret","token","private key","address",
        "phone number","email","dob","date of birth"});
    if (toxic || pii) {
        result.guard_risk = "HIGH";
    } else if (contains_keyword(text, {"urgent","emergency","immediate","asap","critical",
        "production","down","outage","breach","security",
        "human","manager","escalate","now","right now"})) {
        result.guard_risk = "MEDIUM";
    }

    result.urgent = contains_keyword(text, {"urgent","emergency","immediate","asap","critical",
        "production","down","outage","breach","security",
        "human","manager","escalate","now","right now"});
    result.probability = result.urgent ? 0.8 : 0.3;

    for (const auto& [tag, desc] : tags) {
        if (keyword_overlap(text, tag) > 0.2 ||
            keyword_overlap(text, desc) > 0.2) {
            result.tags.push_back(tag);
        }
    }

    if (result.urgent) {
        result.recommended_actions.push_back("escalate_to_human");
    }
    if (result.guard_risk == "HIGH") {
        result.recommended_actions.push_back("security_review");
    }

    result.estimated_tokens_saved = estimate_tokens(text) / 4;  // Estimate
    result.reasoning = "Triage completed: route=" + result.route +
        ", risk=" + result.guard_risk +
        ", urgent=" + (result.urgent ? "true" : "false");

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
        result.reasoning = "No context provided";
        return result;
    }

    double overlap = keyword_overlap(question, context);
    result.answerable = (overlap >= threshold);
    result.probability = overlap;
    result.reasoning = overlap >= threshold ?
        "Question can be answered from context" :
        "Question cannot be answered from available context";
    return result;
}

RouteDecisionResult LocalDecisionModel::decide_route(
    const std::string& text,
    const std::map<std::string, std::string>& routes) {

    RouteDecisionResult result;
    result.confidence = 0.0;

    double score = 0.0;
    result.route = best_route(text, routes, score);
    result.confidence = score;
    result.reasoning = "Selected route: " + result.route + " (score: " +
        std::to_string(score) + ")";

    return result;
}

PriorityDecisionResult LocalDecisionModel::decide_priority(
    const std::string& text) {

    PriorityDecisionResult result;
    std::map<std::string, std::string> categories;
    categories["critical"] = "critical emergency, production down";
    categories["high"] = "high priority, urgent, important";
    categories["medium"] = "medium priority, normal";
    categories["low"] = "low priority, minor";

    auto r = classify(text, categories, 0.3);
    result.priority = r.label;
    result.score = r.probability;
    result.should_escalate = (r.label == "critical" || r.label == "high");
    result.reasoning = "Classified as " + r.label + " with confidence " +
        std::to_string(r.confidence);
    return result;
}

ActionDecisionResult LocalDecisionModel::decide_action(
    const std::string& text,
    const std::vector<std::string>& available_actions) {

    ActionDecisionResult result;
    result.confidence = 0.0;

    std::map<std::string, std::string> categories;
    for (const auto& action : available_actions) {
        categories[action] = "action: " + action;
    }

    auto r = classify(text, categories, 0.3);
    result.action = r.label;
    result.confidence = r.confidence;
    result.reasoning = "Selected action: " + r.label;
    result.estimated_tokens = estimate_tokens(text) + estimate_tokens(result.action);
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
    result.results = "{\"executed\":\"local\",\"steps\":0,\"note\":\"local heuristic model\"}";
    result.tokens_saved = 0;
    result.steps_executed = 0;
    return result;
}

} // namespace systemone
} // namespace pluto
