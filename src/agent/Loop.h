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

PermissionFn auto_allow();
PermissionFn interactive_prompt();

struct UiCallbacks {
	std::function<void(const std::string& text)> on_text;
	std::function<void(const std::string& tool_name, const std::string& preview)> on_tool_start;
	std::function<void(const std::string& tool_name, const ToolResult&)> on_tool_result;
	std::function<void(const std::string& err)> on_error;
	std::function<void(int input_tokens, int output_tokens)> on_usage;
};

// Run one user turn through the agent loop: sends messages, handles any
// tool_use cycle, returns when stop_reason is end_turn or on error. The
// messages vector is mutated with the full assistant turn and tool_result
// user turns.
void run_turn(Provider& provider,
              const Config& cfg,
              ToolRegistry& tools,
              std::vector<Message>& messages,
              PermissionFn permission,
              const UiCallbacks& ui);

}  // namespace pluto
