#pragma once

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"

#include <functional>
#include <string>
#include <vector>

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
              std::atomic<bool>* cancel = nullptr);

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
                   const UiCallbacks& ui);

 // Internal helper functions for testing.
inline size_t estimate_tokens(const std::vector<Message>& msgs) {
    size_t chars = 0;
    for (const auto& m : msgs) {
        chars += m.role.size();
        for (const auto& b : m.content) {
            chars += b.text.size() + b.tool_input.size()
                   + b.tool_id.size() + b.tool_name.size() + 16;
        }
    }
    return chars / 4;
}

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
        // Assistant used tools → next must be user(tool_result).
        if (i >= msgs.size() || msgs[i].role != "user") return 0;
        ++i;  // past user(tool_result), loop back to expect assistant
    }
    return 0;  // ran out of messages without finding a clean boundary
}

inline int prune_to_fit(std::vector<Message>& msgs, size_t target) {
    int dropped = 0;
    while (estimate_tokens(msgs) > target && msgs.size() > 2) {
        size_t boundary = first_round_end(msgs);
        if (boundary == 0 || boundary >= msgs.size()) break;
        msgs.erase(msgs.begin(), msgs.begin() + boundary);
        ++dropped;
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
