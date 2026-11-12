#pragma once

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"
#include "../events.h"

#include <functional>
#include <string>
#include <vector>
#include <unordered_set>

namespace pluto {

enum class Permission { Allow, AllowAlways, Deny };

using PermissionFn = std::function<Permission(const Tool&, const std::string& preview)>;

PermissionFn AutoAllow();
PermissionFn InteractivePrompt();

struct UiCallbacks {
    std::function<void(const std::string& text)> on_text;
    std::function<void(const std::string& tool_name, const std::string& preview)> on_tool_start;
    // `tool_input_json` is the raw JSON passed to the tool — useful for
    // rendering inline diffs (edit_file) or argument echo in the UI.
    std::function<void(const std::string& tool_name,
                        const std::string& tool_input_json,
                        const ToolResult&)> on_tool_result;
    // Image block received from the model. base64_data + media_type.
    std::function<void(const std::string& base64_data,
                        const std::string& media_type)> on_image;
    // Document block received from the model. base64_data + media_type + filename.
    std::function<void(const std::string& base64_data,
                        const std::string& media_type,
                        const std::string& filename)> on_document;
    std::function<void(const std::string& err)> on_error;
    std::function<void(int input_tokens, int output_tokens,
                        int cache_read_tokens, int cache_creation_tokens)> on_usage;
    // Informational status updates (e.g. "HTTP 429, retrying in 2s…").
    // Optional — ignored if null.
    std::function<void(const std::string& status)> on_status;
    // RAG context was injected. topics is a list of snippet topics used.
    // Optional — ignored if null.
    std::function<void(const std::vector<std::string>& topics)> on_rag_inject;
};

// Compaction result for telemetry
struct CompactionResult {
    bool performed = false;
    int messages_before = 0;
    int messages_after = 0;
    int tokens_before = 0;
    int tokens_after = 0;
    int summary_tokens = 0;
    std::string model_used;
};

// Run one user turn through the agent loop: sends messages, handles any
// tool_use cycle, returns when stop_reason is end_turn or on error. The
// messages vector is mutated with the full assistant turn and tool_result
// user turns.
void RunTurn(Provider& provider,
              const Config& cfg,
              ToolRegistry& tools,
              std::vector<Message>& messages,
              PermissionFn permission,
              const UiCallbacks& ui,
              std::atomic<bool>* cancel = nullptr,
              EventBus* event_bus = nullptr);

// Auto-compact messages when cumulative token usage exceeds a threshold.
// Replaces earlier conversation rounds with an LLM-generated summary,
// preserving the most recent user turn. Returns true if compaction was
// performed. `cumulative_input_tokens` is the running total reported by
// the API (not the local estimate). Threshold default: 90% of
// cfg.context_window.
bool MaybeCompact(Provider& provider,
                   const Config& cfg,
                   std::vector<Message>& messages,
                   int cumulative_input_tokens,
                   const UiCallbacks& ui,
                   CompactionResult* result = nullptr);

 // Internal helper functions for testing.
 // Uses provider's estimate_tokens() for accurate counting (includes tools, images, etc.)
inline int estimate_tokens(const Provider& provider, const std::vector<Message>& msgs) {
    int total = 0;
    for (const auto& m : msgs) {
        total += provider.estimate_tokens(m.role);
        for (const auto& b : m.content) {
            switch (b.type) {
                case ContentBlock::TEXT:
                    total += provider.estimate_tokens(b.text);
                    break;
                case ContentBlock::TOOL_USE:
                    total += provider.estimate_tokens(b.tool_name)
                           + provider.estimate_tokens(b.tool_input)
                           + 3;
                    break;
                case ContentBlock::TOOL_RESULT:
                    total += provider.estimate_tokens(b.text) + 2;
                    break;
                case ContentBlock::IMAGE:
                    total += 85;  // Fixed cost for image
                    break;
                case ContentBlock::DOCUMENT:
                    total += provider.estimate_tokens(b.doc_data) / 4 + 10;
                    break;
                case ContentBlock::AUDIO:
                    total += 50;
                    break;
            }
        }
    }
    return total;
}

// Legacy estimate using rough 3.5 chars/token (for when provider unavailable)
inline size_t estimate_tokens_rough(const std::vector<Message>& msgs) {
    size_t chars = 0;
    for (const auto& m : msgs) {
        chars += m.role.size();
        for (const auto& b : m.content) {
            chars += b.text.size() + b.tool_input.size()
                   + b.tool_id.size() + b.tool_name.size() + 16;
        }
    }
    return chars / 3;  // 3.5 chars/token ≈ 3 for integer math
}

// Message importance scoring for semantic pruning
struct MessageScore {
    size_t index;
    int tokens;
    double importance;  // 0.0 - 1.0, higher = more important
    bool is_boundary;   // Part of tool_use/result pair boundary
};

// Score a message for importance based on content analysis
inline double score_message_importance(const Message& msg, bool is_system_prompt, bool is_recent_user) {
    double score = 0.5;  // Base score
    
    // System prompts are very important
    if (is_system_prompt) return 1.0;
    
    // Recent user messages are important
    if (is_recent_user) score += 0.3;
    
    for (const auto& b : msg.content) {
        switch (b.type) {
            case ContentBlock::TOOL_USE:
                // Tool use messages are important (show agent actions)
                score += 0.15;
                break;
            case ContentBlock::TOOL_RESULT:
                // Tool results are important (show outcomes)
                score += 0.1;
                // Error results are more important for debugging
                if (b.is_error) score += 0.1;
                break;
            case ContentBlock::TEXT:
                // Text with file paths, code, or key terms is more important
                if (!b.text.empty()) {
                    // Check for file paths
                    if (b.text.find('/') != std::string::npos && 
                        (b.text.find(".cpp") != std::string::npos ||
                         b.text.find(".h") != std::string::npos ||
                         b.text.find(".py") != std::string::npos ||
                         b.text.find(".js") != std::string::npos ||
                         b.text.find(".json") != std::string::npos)) {
                        score += 0.1;
                    }
                    // Check for key decision markers
                    if (b.text.find("error") != std::string::npos ||
                        b.text.find("fail") != std::string::npos ||
                        b.text.find("success") != std::string::npos ||
                        b.text.find("fix") != std::string::npos ||
                        b.text.find("change") != std::string::npos) {
                        score += 0.05;
                    }
                    // Longer messages may have more context
                    if (b.text.size() > 500) score += 0.05;
                }
                break;
            case ContentBlock::IMAGE:
                score += 0.1;
                break;
            case ContentBlock::DOCUMENT:
                score += 0.05;
                break;
            default:
                break;
        }
    }
    
    // Cap at 1.0
    return std::min(1.0, score);
}

// Find the first complete round boundary (user -> assistant[+tools] -> user[+tool_results])
inline size_t first_round_end(const std::vector<Message>& msgs) {
    size_t i = 0;
    if (i >= msgs.size() || msgs[i].role != "user") return 0;
    ++i;  // skip opening user
    while (i < msgs.size()) {
        if (i >= msgs.size() || msgs[i].role != "assistant") return 0;
        // Check if this assistant message contains tool_use blocks.
        bool has_tool_use = false;
        for (const auto& b : msgs[i].content)
            if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
        ++i;  // past assistant
        if (!has_tool_use) return i;  // clean boundary — no pending tool calls
        // Assistant used tools -> next must be user(tool_result).
        if (i >= msgs.size() || msgs[i].role != "user") return 0;
        ++i;  // past user(tool_result), loop back to expect assistant
    }
    return 0;  // ran out of messages without finding a clean boundary
}

// Enhanced pruning with semantic importance scoring
// Preserves: system prompt, first user message, recent messages, high-importance messages
// Respects tool_use/tool_result pair boundaries
inline int prune_to_fit(Provider& provider, std::vector<Message>& msgs, size_t target) {
    int dropped = 0;
    
    // Quick check - if already under target, nothing to do
    if (static_cast<size_t>(estimate_tokens(provider, msgs)) <= target) {
        return 0;
    }
    
    // Need at least 2 messages (system + user or user + assistant)
    if (msgs.size() <= 2) {
        return 0;
    }
    
    // Score all messages
    std::vector<MessageScore> scores;
    scores.reserve(msgs.size());
    
    bool seen_first_user = false;
    for (size_t i = 0; i < msgs.size(); ++i) {
        bool is_system = (i == 0 && msgs[i].role == "system");
        bool is_first_user = (!seen_first_user && msgs[i].role == "user");
        bool is_recent_user = (msgs[i].role == "user" && i >= msgs.size() - 3);
        
        if (is_first_user) seen_first_user = true;
        
        MessageScore s;
        s.index = i;
        s.tokens = estimate_tokens(provider, std::vector<Message>{msgs[i]});
        s.importance = score_message_importance(msgs[i], is_system, is_recent_user);
        s.is_boundary = false;
        scores.push_back(s);
    }
    
    // Mark boundary messages (tool_use/result pairs must stay together)
    for (size_t i = 0; i + 1 < msgs.size(); ++i) {
        bool has_tool_use = false;
        for (const auto& b : msgs[i].content) {
            if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
        }
        if (has_tool_use && msgs[i+1].role == "user") {
            bool has_tool_result = false;
            for (const auto& b : msgs[i+1].content) {
                if (b.type == ContentBlock::TOOL_RESULT) { has_tool_result = true; break; }
            }
            if (has_tool_result) {
                scores[i].is_boundary = true;
                scores[i+1].is_boundary = true;
                // Boost importance of boundary pairs
                scores[i].importance = std::max(scores[i].importance, 0.7);
                scores[i+1].importance = std::max(scores[i+1].importance, 0.7);
            }
        }
    }
    
    // Also mark first user message as boundary to preserve conversation start
    if (!msgs.empty() && msgs[0].role == "user") {
        scores[0].is_boundary = true;
        scores[0].importance = 1.0;
    }
    
    // System prompt is always preserved
    bool has_system = (!msgs.empty() && msgs[0].role == "system");
    
    // Sort by importance (ascending) so we drop least important first
    // But we must respect boundaries - can't drop just one of a pair
    std::vector<size_t> drop_candidates;
    for (size_t i = 0; i < scores.size(); ++i) {
        // Never drop system prompt
        if (has_system && i == 0) continue;
        // Never drop boundary messages individually
        if (scores[i].is_boundary) continue;
        drop_candidates.push_back(i);
    }
    
    // Sort candidates by importance (least important first)
    std::sort(drop_candidates.begin(), drop_candidates.end(),
              [&scores](size_t a, size_t b) {
                  return scores[a].importance < scores[b].importance;
              });
    
    // Drop messages until under target
    size_t current_tokens = static_cast<size_t>(estimate_tokens(provider, msgs));
    std::vector<bool> to_drop(msgs.size(), false);
    
    for (size_t idx : drop_candidates) {
        if (current_tokens <= target) break;
        
        to_drop[idx] = true;
        current_tokens -= scores[idx].tokens;
        ++dropped;
    }
    
    // If still over target, we must drop boundary pairs (least important first)
    if (current_tokens > target) {
        std::vector<std::pair<size_t, size_t>> boundary_pairs;
        for (size_t i = 0; i + 1 < scores.size(); ++i) {
            if (scores[i].is_boundary && scores[i+1].is_boundary) {
                boundary_pairs.emplace_back(i, i+1);
            }
        }
        
        // Sort boundary pairs by combined importance
        std::sort(boundary_pairs.begin(), boundary_pairs.end(),
                  [&scores](const auto& a, const auto& b) {
                      double imp_a = scores[a.first].importance + scores[a.second].importance;
                      double imp_b = scores[b.first].importance + scores[b.second].importance;
                      return imp_a < imp_b;
                  });
        
        for (const auto& [i1, i2] : boundary_pairs) {
            if (current_tokens <= target) break;
            if (to_drop[i1] || to_drop[i2]) continue;  // Already marked
            
            to_drop[i1] = true;
            to_drop[i2] = true;
            current_tokens -= scores[i1].tokens + scores[i2].tokens;
            dropped += 2;
        }
    }
    
    // Apply drops
    if (dropped > 0) {
        std::vector<Message> new_msgs;
        new_msgs.reserve(msgs.size() - dropped);
        for (size_t i = 0; i < msgs.size(); ++i) {
            if (!to_drop[i]) {
                new_msgs.push_back(std::move(msgs[i]));
            }
        }
        msgs = std::move(new_msgs);
    }
    
    return dropped;
}

// Truncate tool output to max_tool_output with a truncation message.
inline void cap_tool_output(std::string& out, const Config& cfg) {
    size_t kMaxToolOutput = cfg.max_tool_output;
    if (out.size() <= kMaxToolOutput) return;
    size_t dropped = out.size() - kMaxToolOutput;
    out.resize(kMaxToolOutput);
    out += "\n... [truncated " + std::to_string(dropped) + " bytes]";
}

}  // namespace pluto
