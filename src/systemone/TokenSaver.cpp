#include "TokenSaver.h"
#include "DecisionModel.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>

namespace pluto {
namespace systemone {

// ============================================================================
// TOKEN SAVER IMPLEMENTATION
// ============================================================================

int estimate_tokens(const std::string& text) {
    // Approximation: ~4 characters per token for English text
    if (text.empty()) return 0;
    return static_cast<int>(std::ceil(text.size() / 4.0));
}

double compute_compression_ratio(
    const std::string& original,
    const std::string& compressed) {
    if (original.empty()) return 1.0;
    if (compressed.empty()) return 0.0;
    return static_cast<double>(compressed.size()) /
           static_cast<double>(original.size());
}

int estimate_tokens_saved(
    const std::vector<std::string>& original_messages,
    const std::vector<std::string>& compressed_messages) {
    int saved = 0;
    size_t n = std::min(original_messages.size(), compressed_messages.size());
    for (size_t i = 0; i < n; i++) {
        int orig = estimate_tokens(original_messages[i]);
        int comp = estimate_tokens(compressed_messages[i]);
        if (orig > comp) saved += (orig - comp);
    }
    return saved;
}

ContextHeadroomResult compute_context_headroom(
    int total_tokens,
    int used_tokens,
    double compaction_threshold) {
    ContextHeadroomResult result;
    result.total_tokens = total_tokens;
    result.used_tokens = used_tokens;
    result.available_tokens = total_tokens - used_tokens;
    result.pressure_ratio = total_tokens > 0
        ? static_cast<double>(used_tokens) / static_cast<double>(total_tokens)
        : 0.0;

    result.needs_compaction = result.pressure_ratio >= compaction_threshold;

    if (result.needs_compaction) {
        // Recommend compacting to 70% of window to leave headroom
        int target_tokens = static_cast<int>(total_tokens * 0.70);
        result.recommended_compact_tokens =
            std::max(0, used_tokens - target_tokens);
    } else {
        result.recommended_compact_tokens = 0;
    }

    return result;
}

std::string extract_key_sentences(
    const std::string& text,
    int max_sentences) {
    if (text.empty() || max_sentences <= 0) return "";

    // Simple sentence splitter: split on . ! ?
    std::vector<std::string> sentences;
    std::string current;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        current += c;
        if (c == '.' || c == '!' || c == '?') {
            // Trim whitespace
            size_t start = current.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                current = current.substr(start);
            }
            if (!current.empty() && current != "." && current != "!" && current != "?") {
                sentences.push_back(current);
            }
            current.clear();
        }
    }
    // Don't forget trailing text
    if (!current.empty()) {
        size_t start = current.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) {
            current = current.substr(start);
            if (!current.empty()) sentences.push_back(current);
        }
    }

    if (sentences.empty()) return text;

    // Score sentences by length and position (earlier = more important)
    struct ScoredSentence {
        int index;
        std::string text;
        double score;
    };

    std::vector<ScoredSentence> scored;
    for (size_t i = 0; i < sentences.size(); i++) {
        double score = 1.0;
        // Earlier sentences are more important
        score += 1.0 / (static_cast<double>(i) + 1.0);
        // Longer sentences tend to have more information
        score += static_cast<double>(sentences[i].size()) / 100.0;
        scored.push_back({static_cast<int>(i), sentences[i], score});
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const ScoredSentence& a, const ScoredSentence& b) {
            return a.score > b.score;
        });

    // Take top max_sentences, then sort by original index for coherence
    std::vector<ScoredSentence> selected;
    size_t take = std::min(static_cast<size_t>(max_sentences), scored.size());
    for (size_t i = 0; i < take; i++) {
        selected.push_back(scored[i]);
    }
    std::sort(selected.begin(), selected.end(),
        [](const ScoredSentence& a, const ScoredSentence& b) {
            return a.index < b.index;
        });

    // Join selected sentences
    std::string result;
    for (size_t i = 0; i < selected.size(); i++) {
        if (i > 0) result += " ";
        result += selected[i].text;
    }
    return result;
}

std::string compress_message(
    const std::string& message,
    int max_tokens) {
    if (message.empty()) return "";

    int current_tokens = estimate_tokens(message);
    if (current_tokens <= max_tokens) return message;

    // Estimate how many sentences we can keep
    // Rough heuristic: keep sentences until we hit max_tokens
    std::vector<std::string> sentences;
    std::string current;
    for (size_t i = 0; i < message.size(); i++) {
        char c = message[i];
        current += c;
        if (c == '.' || c == '!' || c == '?') {
            size_t start = current.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                current = current.substr(start);
            }
            if (!current.empty()) sentences.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        size_t start = current.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) {
            current = current.substr(start);
            if (!current.empty()) sentences.push_back(current);
        }
    }

    if (sentences.empty()) {
        // Fallback: truncate to max_tokens * 4 chars
        size_t max_chars = static_cast<size_t>(max_tokens * 4);
        if (message.size() > max_chars) {
            return message.substr(0, max_chars) + "...";
        }
        return message;
    }

    // Greedily add sentences until we hit token budget
    std::string result;
    int used_tokens = 0;
    for (const auto& s : sentences) {
        int s_tokens = estimate_tokens(s);
        if (used_tokens + s_tokens + 1 > max_tokens) break;
        if (!result.empty()) result += " ";
        result += s;
        used_tokens += s_tokens + 1;
    }

    if (result.empty()) {
        // Even one sentence is too long; truncate it
        size_t max_chars = static_cast<size_t>(max_tokens * 4);
        if (sentences[0].size() > max_chars) {
            return sentences[0].substr(0, max_chars) + "...";
        }
        return sentences[0];
    }

    return result;
}

std::vector<int> select_messages_to_compact(
    const std::vector<std::string>& messages,
    const std::string& current_question,
    int max_tokens,
    double compaction_threshold) {
    std::vector<int> to_compact;

    if (messages.empty()) return to_compact;

    // Compute current total tokens
    int total = 0;
    for (const auto& m : messages) {
        total += estimate_tokens(m);
    }

    ContextHeadroomResult headroom = compute_context_headroom(
        total, total, compaction_threshold);

    if (!headroom.needs_compaction) return to_compact;

    // Score each message by relevance to current question
    // Older and less relevant messages are better candidates for compaction
    struct ScoredMessage {
        int index;
        double score;  // lower = more likely to compact
        int tokens;
    };

    std::vector<ScoredMessage> scored;
    for (size_t i = 0; i < messages.size(); i++) {
        double relevance = compute_compression_ratio(
            messages[i], extract_key_sentences(messages[i], 1));
        // Older messages (lower index) are more compactable
        double age_factor = 1.0 - (static_cast<double>(i) /
                                  static_cast<double>(messages.size()));
        double score = relevance * 0.7 + age_factor * 0.3;
        scored.push_back({static_cast<int>(i), score,
                         estimate_tokens(messages[i])});
    }

    // Sort by score ascending (lowest score = most compactable)
    std::sort(scored.begin(), scored.end(),
        [](const ScoredMessage& a, const ScoredMessage& b) {
            return a.score < b.score;
        });

    // Select messages to compact until we're under threshold
    int tokens_to_free = headroom.recommended_compact_tokens;
    int freed = 0;
    for (const auto& sm : scored) {
        if (freed >= tokens_to_free) break;
        to_compact.push_back(sm.index);
        freed += sm.tokens;
    }

    return to_compact;
}

std::vector<TokenSaveResult> execute_token_save_program(
    const std::vector<TokenSaveOp>& ops,
    const std::map<std::string, std::string>& context) {
    std::vector<TokenSaveResult> results;

    for (const auto& op : ops) {
        TokenSaveResult result;
        result.id = op.id;
        result.op = op.op;

        // Evaluate condition if present
        if (!op.when.empty()) {
            // Simple condition evaluation: check if context contains key
            auto it = context.find(op.when);
            if (it == context.end() || it->second.empty()) {
                result.success = false;
                result.error = "condition not met: " + op.when;
                results.push_back(result);
                continue;
            }
        }

        int original_tokens = 0;
        int compressed_tokens = 0;

        if (op.op == "compress") {
            auto target_it = context.find(op.target);
            if (target_it != context.end()) {
                original_tokens = estimate_tokens(target_it->second);
                std::string compressed = compress_message(
                    target_it->second,
                    std::stoi(op.params.count("max_tokens") ?
                             op.params.at("max_tokens") : "100"));
                compressed_tokens = estimate_tokens(compressed);
                result.result_text = compressed;
            }
        } else if (op.op == "extract") {
            auto target_it = context.find(op.target);
            if (target_it != context.end()) {
                original_tokens = estimate_tokens(target_it->second);
                int max_sentences = std::stoi(
                    op.params.count("max_sentences") ?
                    op.params.at("max_sentences") : "3");
                std::string extracted = extract_key_sentences(
                    target_it->second, max_sentences);
                compressed_tokens = estimate_tokens(extracted);
                result.result_text = extracted;
            }
        } else if (op.op == "summarize") {
            auto target_it = context.find(op.target);
            if (target_it != context.end()) {
                original_tokens = estimate_tokens(target_it->second);
                int max_tokens = std::stoi(
                    op.params.count("max_tokens") ?
                    op.params.at("max_tokens") : "200");
                std::string summarized = compress_message(
                    target_it->second, max_tokens);
                compressed_tokens = estimate_tokens(summarized);
                result.result_text = summarized;
            }
        } else if (op.op == "prune") {
            // Pruning removes low-importance items; represented as empty result
            result.success = true;
            results.push_back(result);
            continue;
        } else if (op.op == "rank") {
            // Ranking doesn't change text, just provides ordering
            result.success = true;
            results.push_back(result);
            continue;
        } else {
            result.success = false;
            result.error = "unknown op: " + op.op;
            results.push_back(result);
            continue;
        }

        result.success = true;
        result.original_tokens = original_tokens;
        result.compressed_tokens = compressed_tokens;
        result.tokens_saved = original_tokens - compressed_tokens;
        results.push_back(result);
    }

    return results;
}

ContextOptimizationResult optimize_context_window(
    const std::vector<std::string>& messages,
    const std::string& current_question,
    int max_tokens,
    double compaction_threshold) {
    ContextOptimizationResult result;
    result.optimized_messages = messages;

    // Compute original tokens
    for (const auto& m : messages) {
        result.original_tokens += estimate_tokens(m);
    }

    // Compute headroom
    ContextHeadroomResult headroom = compute_context_headroom(
        max_tokens, result.original_tokens, compaction_threshold);

    if (!headroom.needs_compaction) {
        result.final_tokens = result.original_tokens;
        return result;
    }

    // Select messages to compact
    std::vector<int> to_compact = select_messages_to_compact(
        messages, current_question, max_tokens, compaction_threshold);

    // Compact selected messages
    for (int idx : to_compact) {
        if (idx < 0 || idx >= static_cast<int>(messages.size())) continue;
        std::string compressed = compress_message(
            messages[idx],
            static_cast<int>(messages[idx].size() * 0.5));  // Compress to 50%
        result.optimized_messages[idx] = compressed;
    }

    // Compute final tokens
    for (const auto& m : result.optimized_messages) {
        result.final_tokens += estimate_tokens(m);
    }

    result.total_tokens_saved = result.original_tokens - result.final_tokens;

    return result;
}

}  // namespace systemone
}  // namespace pluto
