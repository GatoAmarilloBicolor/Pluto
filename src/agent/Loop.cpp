#include "Loop.h"

#include <cctype>
#include <cstdio>
#include <memory>
#include <set>
#include <string>

namespace pluto {

namespace {

const char* kYellow = "\x1b[33m";
const char* kReset  = "\x1b[0m";

}  // namespace

PermissionFn auto_allow() {
	return [](const Tool&, const std::string&) { return Permission::Allow; };
}

PermissionFn interactive_prompt() {
	auto always = std::make_shared<std::set<std::string>>();
	return [always](const Tool& tool, const std::string&) -> Permission {
		if (always->count(tool.name())) return Permission::Allow;
		std::printf("  %s[y]allow  [a]always  [n]deny: %s", kYellow, kReset);
		std::fflush(stdout);
		char buf[32];
		if (!std::fgets(buf, sizeof(buf), stdin)) return Permission::Deny;
		char c = 0;
		for (char* p = buf; *p; ++p) {
			if (!std::isspace(static_cast<unsigned char>(*p))) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
				break;
			}
		}
		if (c == 'a') { always->insert(tool.name()); return Permission::Allow; }
		if (c == 'n' || c == 'd') return Permission::Deny;
		return Permission::Allow;  // empty/y defaults to allow
	};
}

namespace {
// Hard cap on a single tool_result payload to protect the context window.
// ~48 KB ≈ 12k tokens, enough for most diagnostic output, small enough that
// even a dozen tool calls can't blow a 200k context.
constexpr size_t kMaxToolOutput = 48 * 1024;

void cap_tool_output(std::string& out) {
	if (out.size() <= kMaxToolOutput) return;
	size_t dropped = out.size() - kMaxToolOutput;
	out.resize(kMaxToolOutput);
	out += "\n... [truncated " + std::to_string(dropped) + " bytes]";
}
}  // namespace

void run_turn(Provider& provider,
              const Config& cfg,
              ToolRegistry& tools,
              std::vector<Message>& messages,
              PermissionFn permission,
              const UiCallbacks& ui) {
	// Snapshot size at entry. On any failure during this turn we rewind to
	// here so the user's original prompt is preserved but half-built
	// assistant/tool_result turns are dropped — otherwise a failed turn
	// leaves garbage in history that breaks subsequent requests.
	const size_t entry_size = messages.size();

	std::vector<ToolDef> tool_defs;
	for (const Tool* t : tools.list()) {
		ToolDef d;
		d.name = t->name();
		d.description = t->description();
		d.input_schema_json = t->input_schema_json();
		tool_defs.push_back(std::move(d));
	}

	for (int iter = 0; iter < 25; ++iter) {
		CompletionRequest req;
		req.model = cfg.model;
		req.system = cfg.system_prompt;
		req.messages = messages;
		req.tools = tool_defs;
		req.max_tokens = cfg.max_tokens;

		Message assistant;
		assistant.role = "assistant";
		ContentBlock* active_text = nullptr;

		bool had_error = false;
		std::string stop_reason;

		provider.stream(req, [&](const StreamEvent& ev) {
			switch (ev.type) {
				case StreamEvent::TEXT_DELTA: {
					if (!active_text ||
					    assistant.content.empty() ||
					    assistant.content.back().type != ContentBlock::TEXT) {
						ContentBlock tb;
						tb.type = ContentBlock::TEXT;
						assistant.content.push_back(std::move(tb));
						active_text = &assistant.content.back();
					}
					active_text->text += ev.text;
					if (ui.on_text) ui.on_text(ev.text);
					break;
				}
				case StreamEvent::TOOL_USE_START: {
					active_text = nullptr;
					ContentBlock b;
					b.type = ContentBlock::TOOL_USE;
					b.tool_id = ev.tool_id;
					b.tool_name = ev.tool_name;
					assistant.content.push_back(std::move(b));
					break;
				}
				case StreamEvent::TOOL_USE_INPUT_DELTA:
					break;  // accumulated in provider; full blob arrives on END
				case StreamEvent::TOOL_USE_END: {
					for (auto it = assistant.content.rbegin();
					     it != assistant.content.rend(); ++it) {
						if (it->type == ContentBlock::TOOL_USE &&
						    it->tool_id == ev.tool_id) {
							it->tool_input = ev.tool_input;
							break;
						}
					}
					break;
				}
				case StreamEvent::MESSAGE_END:
					stop_reason = ev.stop_reason;
					if (ui.on_usage && (ev.input_tokens > 0 || ev.output_tokens > 0)) {
						ui.on_usage(ev.input_tokens, ev.output_tokens);
					}
					break;
				case StreamEvent::ERROR:
					had_error = true;
					if (ui.on_error) ui.on_error(ev.error);
					break;
			}
		});

		if (had_error) {
			messages.resize(entry_size);
			return;
		}

		messages.push_back(assistant);
		if (stop_reason != "tool_use") return;

		Message user_results;
		user_results.role = "user";

		for (const auto& block : assistant.content) {
			if (block.type != ContentBlock::TOOL_USE) continue;

			ContentBlock result;
			result.type = ContentBlock::TOOL_RESULT;
			result.tool_id = block.tool_id;

			Tool* tool = tools.find(block.tool_name);
			if (!tool) {
				result.is_error = true;
				result.text = "unknown tool: " + block.tool_name;
				user_results.content.push_back(result);
				continue;
			}

			std::string preview = tool->preview(block.tool_input);
			if (ui.on_tool_start) ui.on_tool_start(block.tool_name, preview);

			Permission p = permission(*tool, preview);
			if (p == Permission::Deny) {
				result.is_error = true;
				result.text = "user denied tool execution";
				ToolResult denied;
				denied.is_error = true;
				denied.output = result.text;
				if (ui.on_tool_result) ui.on_tool_result(block.tool_name, denied);
				user_results.content.push_back(result);
				continue;
			}

			ToolResult tr = tool->execute(block.tool_input);
			cap_tool_output(tr.output);
			result.is_error = tr.is_error;
			result.text = tr.output;
			if (ui.on_tool_result) ui.on_tool_result(block.tool_name, tr);
			user_results.content.push_back(result);
		}

		messages.push_back(user_results);
		// Loop: next iteration feeds tool_results back to the model.
	}
}

}  // namespace pluto
