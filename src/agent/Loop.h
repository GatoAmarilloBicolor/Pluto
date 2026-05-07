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

}  // namespace pluto
