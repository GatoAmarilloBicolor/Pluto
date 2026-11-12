#include "Loop.h"
#include "../rag.h"
#include "../haiku/Notifications.h"
#include "../systemone/SystemOneClient.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <chrono>
#include <unordered_set>
#include <mutex>

#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>

namespace pluto {

#ifdef __HAIKU__
// Function-local static (Meyers singleton) to avoid static initialization order issues
NotificationCenter& GetNotificationCenter() {
    static NotificationCenter g_notify_center;
    return g_notify_center;
}
#endif

namespace {

const char* kYellow = "\x1b[33m";
const char* kReset  = "\x1b[0m";

}  // namespace

PermissionFn AutoAllow() {
	return [](const Tool&, const std::string&) { return Permission::Allow; };
}

PermissionFn InteractivePrompt() {
	auto always = std::make_shared<std::set<std::string>>();
	return [always](const Tool& tool, const std::string&) -> Permission {
		if (always->count(tool.name())) return Permission::Allow;
		std::printf("  %s[y]allow  [a]always  [n]deny: %s", kYellow, kReset);
		std::fflush(stdout);
		char buf[32];
		// Use poll() to check for stdin input with timeout, allowing cancel flag check
		struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
		while (true) {
			if (pluto::IsCancelRequested()) return Permission::Deny;
			int ret = poll(&pfd, 1, 100);  // 100ms timeout
			if (ret < 0) {
				if (errno == EINTR) continue;
				return Permission::Deny;
			}
			if (ret > 0 && (pfd.revents & POLLIN)) {
				if (!std::fgets(buf, sizeof(buf), stdin)) return Permission::Deny;
				break;
			}
		}
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



// Rough token estimate: 4 chars per token. Used only for the local
// truncation heuristic, not for billing.

// Find the end of the first complete "round" starting at index 0.
// A round is: user → assistant [→ user(tool_result) → assistant]*.
// We must drop entire rounds to avoid orphaned tool_result messages
// that reference a deleted tool_use id (which the API would reject).
// Returns the number of messages in the first complete round, or 0
// if no safe boundary was found.

// Drop the oldest complete rounds until estimated tokens fit under
// `target`. Never drops the most recent user turn. Preserves
// tool_use/tool_result pairs as atomic units to avoid API errors.
// Returns the number of rounds dropped.

// Run a hook command in a child process with env vars set safely.
// This avoids shell injection: env var values are passed via setenv()
// in the child process, never interpolated into the command string.
// Command is split into argv array (no shell).
int run_hook_safe(const char* cmd, const char* env_name, const std::string& env_value) {
	// Split command into argv (simple shell-like parsing, no shell execution)
	std::vector<std::string> argv;
	std::string current;
	bool in_single = false;
	bool in_double = false;
	bool escaped = false;
	
	for (size_t i = 0; cmd[i]; ++i) {
		char c = cmd[i];
		if (escaped) {
			current += c;
			escaped = false;
		} else if (c == '\\') {
			escaped = true;
		} else if (c == '\'' && !in_double) {
			in_single = !in_single;
		} else if (c == '"' && !in_single) {
			in_double = !in_double;
		} else if ((c == ' ' || c == '\t') && !in_single && !in_double) {
			if (!current.empty()) {
				argv.push_back(current);
				current.clear();
			}
		} else {
			current += c;
		}
	}
	if (!current.empty()) argv.push_back(current);
	
	if (argv.empty()) return -1;
	
	pid_t pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		// Child: set env var safely, then exec directly (no shell)
		::setenv(env_name, env_value.c_str(), 1);
		std::vector<char*> args;
		args.reserve(argv.size() + 1);
		for (const auto& s : argv) args.push_back(const_cast<char*>(s.c_str()));
		args.push_back(nullptr);
		execvp(args[0], args.data());
		::_exit(127); // exec failed
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void run_hooks(const std::vector<std::string>& hooks, const char* phase,
               const std::string& tool_name) {
	for (const auto& cmd : hooks) {
		if (cmd.empty()) continue;
		std::fprintf(stderr, "[hook %s] %s\n", phase, cmd.c_str());
		int rc = run_hook_safe(cmd.c_str(), "PLUTO_TOOL", tool_name);
		if (rc != 0)
			std::fprintf(stderr, "[hook %s] exit %d\n", phase, rc);
	}
}
}  // namespace

void RunTurn(Provider& provider,
              const Config& cfg,
              ToolRegistry& tools,
              std::vector<Message>& messages,
              PermissionFn permission,
              const UiCallbacks& ui,
              std::atomic<bool>* cancel,
              EventBus* event_bus) {
	// Repair ALL orphaned tool_use blocks in the conversation history.
	// The API requires every assistant message with tool_use to be
	// immediately followed by a user message containing matching
	// tool_result blocks. Scan the entire history and insert synthetic
	// results wherever they're missing (e.g. after cancel, crash, etc).
	for (size_t i = 0; i < messages.size(); ++i) {
		if (messages[i].role != "assistant") continue;
		bool has_tool_use = false;
		for (const auto& b : messages[i].content)
			if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
		if (!has_tool_use) continue;

		// Check if next message is a user with matching tool_results.
		bool needs_repair = false;
		if (i + 1 >= messages.size()) {
			needs_repair = true;
		} else if (messages[i + 1].role != "user") {
			needs_repair = true;
		} else {
			// Next is user — check it has matching tool_results for all tool_uses.
			std::set<std::string> expected_ids;
			for (const auto& b : messages[i].content)
				if (b.type == ContentBlock::TOOL_USE && !b.tool_id.empty())
					expected_ids.insert(b.tool_id);

			std::set<std::string> found_ids;
			for (const auto& b : messages[i + 1].content)
				if (b.type == ContentBlock::TOOL_RESULT && !b.tool_id.empty())
					found_ids.insert(b.tool_id);

			if (expected_ids.empty()) {
				// Fallback if tool_ids were empty: check any TOOL_RESULT exists
				bool has_result = false;
				for (const auto& b : messages[i + 1].content)
					if (b.type == ContentBlock::TOOL_RESULT) { has_result = true; break; }
				if (!has_result) needs_repair = true;
			} else {
				for (const auto& id : expected_ids) {
					if (!found_ids.count(id)) { needs_repair = true; break; }
				}
			}
		}

		if (needs_repair) {
			std::fprintf(stderr,
				"[pluto] repairing orphaned tool_use at message %zu "
				"(next=%s)\n", i,
				i + 1 < messages.size()
					? messages[i + 1].role.c_str() : "END");
			Message synthetic;
			synthetic.role = "user";
			for (const auto& b : messages[i].content) {
				if (b.type != ContentBlock::TOOL_USE) continue;
				std::fprintf(stderr,
					"[pluto]   adding synthetic result for %s\n",
					b.tool_id.c_str());
				ContentBlock r;
				r.type = ContentBlock::TOOL_RESULT;
				r.tool_id = b.tool_id;
				r.is_error = true;
				r.text = "(cancelled)";
				synthetic.content.push_back(std::move(r));
			}
			if (!synthetic.content.empty()) {
				messages.insert(messages.begin() + (int)i + 1, std::move(synthetic));
				++i;  // skip the synthetic message we just inserted
			}
		}
	}

	// Merge consecutive user messages (can happen when max_iter exhausted
	// leaves a trailing user(tool_result) and the next turn adds a new user prompt).
	for (size_t i = 1; i < messages.size(); ++i) {
		if (messages[i].role == "user" && messages[i - 1].role == "user") {
			// Append content from messages[i] into messages[i-1].
			for (auto& b : messages[i].content)
				messages[i - 1].content.push_back(std::move(b));
			messages.erase(messages.begin() + (int)i);
			--i;  // re-check at this position
		}
	}

	// Snapshot size at entry. On any failure during this turn we rewind to
	// here so the user's original prompt is preserved but half-built
	// assistant/tool_result turns are dropped — otherwise a failed turn
	// leaves garbage in history that breaks subsequent requests.
	const size_t entry_size = messages.size();

	// Local context-window truncation using provider's token estimator.
	// Uses cfg.context_soft_limit (default 0.80) of context_window.
	// Runs AFTER repair but BEFORE provider call so compact can take over
	// at context_compact_threshold (default 0.90) with real token counts if needed.
	if (cfg.context_window > 0) {
		double soft_limit = cfg.context_soft_limit > 0 ? cfg.context_soft_limit : 0.80;
		size_t target = static_cast<size_t>(cfg.context_window * soft_limit);
		int current_tokens = estimate_tokens(provider, messages);
		if (static_cast<size_t>(current_tokens) > target) {
			int dropped = prune_to_fit(provider, messages, target);
			if (dropped > 0) {
				std::fprintf(stderr,
					"[pluto] context window pressure: dropped %d oldest pair(s) "
					"(at %.0f%% limit, est. %d tokens)\n",
					dropped, soft_limit * 100, current_tokens);
			}
		}
	}

	// If a mode is active, hide its disabled tools.
	const ModeEntry* active_mode = nullptr;
	std::set<std::string> disabled_tools;
	if (!cfg.active_mode.empty()) {
		active_mode = FindMode(cfg, cfg.active_mode);
		if (active_mode) {
			for (const auto& t : active_mode->disabled_tools)
				disabled_tools.insert(t);
		}
	}
	std::vector<ToolDef> tool_defs;
	for (const Tool* t : tools.list()) {
		if (active_mode && disabled_tools.count(t->name()))
			continue;
		ToolDef d;
		d.name = t->name();
		d.description = t->description();
		d.input_schema_json = t->input_schema_json();
		tool_defs.push_back(std::move(d));
	}

	// RAG: if the latest user message mentions Be classes, inject relevant
	// snippets into the system prompt for this turn. ~500 tokens max.
	std::string rag_block;
	if (RagEntryCount() > 0 && !messages.empty()) {
		// Collect text from the last user message.
		for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
			if (it->role == "user") {
				std::string user_text;
				for (const auto& b : it->content) {
					if (b.type == ContentBlock::TEXT) user_text += b.text + " ";
				}
				auto classes = RagExtractClasses(user_text);
				if (!classes.empty()) {
					auto results = RagSearch(user_text, 3);
					rag_block = RagFormatContext(results);
					if (!rag_block.empty() && ui.on_rag_inject) {
						std::vector<std::string> topics;
						for (const auto& r : results)
							topics.push_back(r.entry->topic);
						ui.on_rag_inject(topics);
					}
				}
				break;
			}
		}
	}

	const int max_iter = (cfg.max_iterations > 0 && cfg.max_iterations <= 100)
		? cfg.max_iterations : 25;
	for (int iter = 0; iter < max_iter; ++iter) {
		CompletionRequest req;
		req.model = cfg.model;
		req.cancel = cancel;
		req.system = cfg.system_prompt;
		if (active_mode && !active_mode->prompt.empty()) {
			req.system += "\n\n<mode>" + active_mode->name + "</mode>\n"
				+ active_mode->prompt;
		}
		if (!rag_block.empty()) {
			req.system += "\n\n" + rag_block;
		}
req.messages = messages;
    // Skip tools for models known not to support tool use,
    // or if a previous turn in this session failed with tool_use error.
    // Reset if the model changes (e.g. via /model switch).
    // Use a thread-safe map to track per-model tool support across sessions.
    static std::mutex tools_disabled_mutex;
    static std::unordered_set<std::string> tools_disabled_models;
    
    bool skip_tools = false;
    {
        std::lock_guard<std::mutex> lock(tools_disabled_mutex);
        skip_tools = tools_disabled_models.count(cfg.model) > 0;
        req.tools = skip_tools ? std::vector<ToolDef>{} : tool_defs;
    }
		req.max_tokens = cfg.max_tokens;

		// Clamp max_tokens to known per-model output limits to avoid
		// HTTP 400 errors from providers.
		auto clamp_output = [](const std::string& m) -> int {
			if (m.find("haiku") != std::string::npos) return 64000;
			if (m.find("sonnet") != std::string::npos) return 64000;
			if (m.find("opus") != std::string::npos) return 32000;
			if (m.find("claude") != std::string::npos) return 64000;
			if (m.find("gpt-4o") != std::string::npos) return 16384;
			if (m.find("gpt-4") != std::string::npos) return 16384;
			if (m.find("deepseek") != std::string::npos) return 8192;
			return 0; // 0 = no clamping for unknown models
		};
		int limit = clamp_output(cfg.model);
		if (limit > 0 && req.max_tokens > limit)
			req.max_tokens = limit;
		req.use_cache = cfg.cache;

		// Dry-run: print a textual preview of what would be sent, then end
		// the turn without calling the provider. Avoids any token spend.
		if (cfg.dry_run) {
			if (ui.on_text) {
				std::string preview;
				preview.reserve(4096);
				preview += "[dry-run] would send to model ";
				preview += req.model;
				preview += "\n";
				preview += "  messages: " + std::to_string(messages.size())
				         + ", tools: " + std::to_string(tool_defs.size()) + "\n";
				if (!cfg.system_prompt.empty()) {
					preview += "  system: " + std::to_string(cfg.system_prompt.size())
					         + " chars\n";
				}
				preview += "\n";
				for (const auto& m : messages) {
					preview += "[" + m.role + "]\n";
					for (const auto& b : m.content) {
						if (b.type == ContentBlock::TEXT) {
							preview += b.text;
							preview += "\n";
						} else if (b.type == ContentBlock::TOOL_USE) {
							preview += "(tool_use " + b.tool_name + " "
							         + b.tool_input + ")\n";
						} else if (b.type == ContentBlock::TOOL_RESULT) {
							preview += "(tool_result";
							if (b.is_error) preview += " ERROR";
							preview += ")\n";
						}
					}
					preview += "\n";
				}
				ui.on_text(preview);
			}
			return;
		}

		Message assistant;
		assistant.role = "assistant";
		// Index of the active text block, or -1 if none.
		// Using an index instead of a pointer avoids dangling after
		// vector reallocation from push_back.
		int active_text_idx = -1;

		bool had_error = false;
		bool tool_use_unsupported = false;
		std::string stop_reason;

		provider.stream(req, [&](const StreamEvent& ev) {
			switch (ev.type) {
				case StreamEvent::TEXT_DELTA: {
					if (active_text_idx < 0 ||
					    assistant.content.empty() ||
					    assistant.content.back().type != ContentBlock::TEXT) {
						ContentBlock tb;
						tb.type = ContentBlock::TEXT;
						assistant.content.push_back(std::move(tb));
						active_text_idx = (int)assistant.content.size() - 1;
					}
					assistant.content[active_text_idx].text += ev.text;
					if (ui.on_text) ui.on_text(ev.text);
					break;
				}
				case StreamEvent::IMAGE_BLOCK: {
					active_text_idx = -1;
					ContentBlock ib;
					ib.type = ContentBlock::IMAGE;
					ib.image_data = ev.image_data;
					ib.image_media_type = ev.image_media_type;
					assistant.content.push_back(std::move(ib));
					// Notify UI about the image.
					if (ui.on_image)
						ui.on_image(ev.image_data, ev.image_media_type);
					else if (ui.on_text)
						ui.on_text("\xf0\x9f\x96\xbc [" + ev.image_media_type + "]\n");
					break;
				}
				case StreamEvent::DOCUMENT_BLOCK: {
					active_text_idx = -1;
					ContentBlock db;
					db.type = ContentBlock::DOCUMENT;
					db.doc_data = ev.document_data;
					db.doc_media_type = ev.document_media_type;
					db.filename = ev.document_filename;
					assistant.content.push_back(std::move(db));
					// Notify UI about the document.
					if (ui.on_document)
						ui.on_document(ev.document_data, ev.document_media_type, ev.document_filename);
					else if (ui.on_text)
						ui.on_text("\xf0\x9f\x93\x84 [" + ev.document_filename + "]\n");
					break;
				}
				case StreamEvent::TOOL_USE_START: {
					active_text_idx = -1;
					ContentBlock b;
					b.type = ContentBlock::TOOL_USE;
					b.tool_id = ev.tool_id;
					b.tool_name = ev.tool_name;
					// Store sequential index for robust matching when tool_id
					// is empty or duplicated (some OpenAI-compat providers).
					b.tool_index = (int)assistant.content.size();
					assistant.content.push_back(std::move(b));
					break;
				}
				case StreamEvent::TOOL_USE_INPUT_DELTA:
					break;  // accumulated in provider; full blob arrives on END
				case StreamEvent::TOOL_USE_END: {
					// Match by sequential index first (robust), fall back to tool_id.
					for (auto it = assistant.content.rbegin();
					     it != assistant.content.rend(); ++it) {
						if (it->type == ContentBlock::TOOL_USE &&
						    (it->tool_index == ev.tool_index ||
						     (!ev.tool_id.empty() && it->tool_id == ev.tool_id))) {
							it->tool_input = ev.tool_input;
							break;
						}
					}
					break;
				}
				case StreamEvent::MESSAGE_END:
					stop_reason = ev.stop_reason;
					if (ui.on_usage && (ev.input_tokens > 0 || ev.output_tokens > 0)) {
						ui.on_usage(ev.input_tokens, ev.output_tokens,
						            ev.cache_read_tokens,
						            ev.cache_creation_tokens);
					}
					break;
case StreamEvent::ERROR:
                    had_error = true;
                    // Detect "no tool use support" errors from providers.
                    // Use specific patterns to avoid false positives
                    // (e.g. "invalid tools schema", network errors mentioning tools).
                    {
                        std::string err = ev.error;
                        std::transform(err.begin(), err.end(), err.begin(),
                            [](unsigned char c){ return std::tolower(c); });
                        // Check for explicit tool support errors (not just "tool" in error)
                        // Must contain "tool" AND one of the specific unsupported phrases
                        bool has_tool_word = err.find("tool") != std::string::npos || 
                                             err.find("function") != std::string::npos;
                        if (has_tool_word && (
                            err.find("tool use not supported") != std::string::npos
                            || err.find("tools not supported") != std::string::npos
                            || err.find("tool use is not available") != std::string::npos
                            || err.find("does not support tool") != std::string::npos
                            || err.find("tool use disabled") != std::string::npos
                            || err.find("tool calling not supported") != std::string::npos
                            || err.find("function calling not supported") != std::string::npos
                            || err.find("tool use not implemented") != std::string::npos
                            || err.find("model does not support") != std::string::npos)) {
                            tool_use_unsupported = true;
                        }
                    }
                    if (ui.on_error) ui.on_error(ev.error);
                    break;
				case StreamEvent::RETRY_INFO:
					if (ui.on_status) ui.on_status(ev.text);
					break;
				case StreamEvent::USAGE:
					// Usage info already handled in MESSAGE_END
					break;
				case StreamEvent::DONE:
					// Stream complete marker
					break;
			}
		});

if (had_error) {
            if (tool_use_unsupported && !req.tools.empty()) {
                // Retry without tools — the model doesn't support them.
                // Remember for subsequent turns.
                {
                    std::lock_guard<std::mutex> lock(tools_disabled_mutex);
                    tools_disabled_models.insert(cfg.model);
                }
                if (ui.on_status)
                    ui.on_status("Retrying without tools…");
                req.tools.clear();
                had_error = false;
                tool_use_unsupported = false;
                stop_reason.clear();
                assistant = Message{};
                assistant.role = "assistant";
                active_text_idx = -1;

				provider.stream(req, [&](const StreamEvent& ev) {
					switch (ev.type) {
						case StreamEvent::TEXT_DELTA: {
							if (active_text_idx < 0 ||
							    assistant.content.empty() ||
							    assistant.content.back().type != ContentBlock::TEXT) {
								ContentBlock tb;
								tb.type = ContentBlock::TEXT;
								assistant.content.push_back(std::move(tb));
								active_text_idx = (int)assistant.content.size() - 1;
							}
							assistant.content[active_text_idx].text += ev.text;
							if (ui.on_text) ui.on_text(ev.text);
							break;
						}
						case StreamEvent::IMAGE_BLOCK: {
							active_text_idx = -1;
							ContentBlock ib;
							ib.type = ContentBlock::IMAGE;
							ib.image_data = ev.image_data;
							ib.image_media_type = ev.image_media_type;
							assistant.content.push_back(std::move(ib));
							if (ui.on_image)
								ui.on_image(ev.image_data, ev.image_media_type);
							break;
						}
						case StreamEvent::DOCUMENT_BLOCK: {
							active_text_idx = -1;
							ContentBlock db;
							db.type = ContentBlock::DOCUMENT;
							db.doc_data = ev.document_data;
							db.doc_media_type = ev.document_media_type;
							db.filename = ev.document_filename;
							assistant.content.push_back(std::move(db));
							if (ui.on_document)
								ui.on_document(ev.document_data, ev.document_media_type, ev.document_filename);
							break;
						}
						case StreamEvent::MESSAGE_END:
							stop_reason = ev.stop_reason;
							if (ui.on_usage && (ev.input_tokens > 0 || ev.output_tokens > 0))
								ui.on_usage(ev.input_tokens, ev.output_tokens,
								            ev.cache_read_tokens, ev.cache_creation_tokens);
							break;
						case StreamEvent::ERROR:
							had_error = true;
							if (ui.on_error) ui.on_error(ev.error);
							break;
						case StreamEvent::RETRY_INFO:
							if (ui.on_status) ui.on_status(ev.text);
							break;
						default: break;
					}
				});
			}
if (had_error) {
#ifdef __HAIKU__
			GetNotificationCenter().Post("Pluto — error",
			                     "Request failed: " + cfg.model);
#endif
				messages.resize(entry_size);
				return;
			}
		}

		// Check stop_reason BEFORE pushing assistant to history.
		// If not "tool_use", the assistant message has no tool_use blocks
		// (or they're incomplete), so don't add it — the repair pass on
		// next turn will handle any orphans.
		if (stop_reason != "tool_use") {
			messages.push_back(assistant);
			return;
		}

		messages.push_back(assistant);

		Message user_results;
		user_results.role = "user";

		for (const auto& block : assistant.content) {
			if (block.type != ContentBlock::TOOL_USE) continue;

			// Check cancellation between tool executions.
			if (cancel && cancel->load(std::memory_order_relaxed))
				return;

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

		// Waterfall permission system: tools/pre-execute
		// Listeners can: allow (return true), deny (set allow=false, return false), ask (prompt user)
		Permission p = Permission::Allow;
		if (event_bus) {
			bool allow = true;
			std::string deny_reason;
			ToolPreExecuteEvent evt{tool, &block.tool_input, &preview, &allow, &deny_reason};
			if (!event_bus->tool_pre_execute.waterfall(evt)) {
				// Waterfall was short-circuited (denied)
				p = Permission::Deny;
			} else if (!allow) {
				p = Permission::Deny;
			}
			if (p == Permission::Deny) {
				result.is_error = true;
				result.text = deny_reason.empty() ? "user denied tool execution" : deny_reason;
				ToolResult denied;
				denied.is_error = true;
				denied.output = result.text;
				if (ui.on_tool_result)
					ui.on_tool_result(block.tool_name, block.tool_input, denied);
				if (event_bus) {
					ToolResultEvent result_evt{tool, &block.tool_input, &denied};
					event_bus->tool_result.emit(result_evt);
				}
				user_results.content.push_back(result);
				continue;
			}
		} else {
			// Legacy permission function
			p = permission(*tool, preview);
			if (p == Permission::Deny) {
				result.is_error = true;
				result.text = "user denied tool execution";
				ToolResult denied;
				denied.is_error = true;
				denied.output = result.text;
				if (ui.on_tool_result)
					ui.on_tool_result(block.tool_name, block.tool_input, denied);
				user_results.content.push_back(result);
				continue;
			}
		}

		// Legacy hooks (for backward compatibility)
		run_hooks(cfg.hooks_pre_tool, "pre", block.tool_name);

		// Waterfall: tools/execute - can skip default execution
		ToolResult tr;
		bool skip_execution = false;
		if (event_bus) {
			ToolExecuteEvent exec_evt{tool, &block.tool_input, &tr, &skip_execution};
			event_bus->tool_execute.waterfall(exec_evt);
		}
		if (!skip_execution) {
			tr = tool->execute(block.tool_input);
		}

		// Legacy hooks
		run_hooks(cfg.hooks_post_tool, "post", block.tool_name);

		// Waterfall: tools/post-execute - can modify result
		if (event_bus) {
			ToolPostExecuteEvent post_evt{tool, &block.tool_input, &tr};
			event_bus->tool_post_execute.waterfall(post_evt);
		}

		cap_tool_output(tr.output, cfg);
		result.is_error = tr.is_error;
		result.text = tr.output;
		if (ui.on_tool_result)
			ui.on_tool_result(block.tool_name, block.tool_input, tr);

		// Waterfall: tools/result - observe final result
		if (event_bus) {
			ToolResultEvent result_evt{tool, &block.tool_input, &tr};
			event_bus->tool_result.emit(result_evt);
		}

		user_results.content.push_back(result);
		}

		messages.push_back(user_results);
		// Loop: next iteration feeds tool_results back to the model.
	}
#ifdef __HAIKU__
	GetNotificationCenter().Post("Pluto — response complete",
	                     "Turn finished successfully in " + cfg.model);
#endif
}

bool MaybeCompact(Provider& provider,
                   const Config& cfg,
                   std::vector<Message>& messages,
                   int cumulative_input_tokens,
                   const UiCallbacks& ui,
                   CompactionResult* result) {
    if (cfg.context_window <= 0) return false;
    
    double compact_threshold = cfg.context_compact_threshold > 0 ? cfg.context_compact_threshold : 0.90;
    int threshold = static_cast<int>(cfg.context_window * compact_threshold);
    if (cumulative_input_tokens < threshold) return false;
    if (messages.size() <= 2) return false;  // nothing useful to compact

    if (result) {
        result->messages_before = static_cast<int>(messages.size());
        result->tokens_before = cumulative_input_tokens;
        result->model_used = cfg.summary_model.empty() ? cfg.model : cfg.summary_model;
    }

    // Find a safe split point that doesn't break tool_use/tool_result pairs.
    // Start from the end and walk backwards to find the last complete "round":
    // user → assistant [→ user(tool_result) → assistant]*
    // We keep the last complete round intact. A round ends at a user message
    // that either has no tool_results, or is the final user turn.
    size_t keep_from = messages.size();
    bool found_boundary = false;
    for (size_t i = messages.size(); i-- > 0; ) {
        // If we hit the very last user message, we stop (keep everything from here).
        if (messages[i].role == "user") {
            // Check if this user message has tool_results.
            bool has_tool_result = false;
            for (const auto& b : messages[i].content)
                if (b.type == ContentBlock::TOOL_RESULT) { has_tool_result = true; break; }
            if (!has_tool_result) {
                // This user message has no tool_results - safe to keep from here.
                keep_from = i;
                found_boundary = true;
                break;
            }
            // This user has tool_results - check if previous is assistant with tool_use.
            if (i > 0 && messages[i - 1].role == "assistant") {
                bool has_tool_use = false;
                for (const auto& b : messages[i - 1].content)
                    if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
                if (has_tool_use) {
                    // This is a tool_use → tool_result pair. Keep both.
                    // Continue walking back to find the previous boundary.
                    keep_from = i - 1;
                    continue;
                }
            }
            // No matching tool_use before this tool_result - safe boundary.
            keep_from = i;
            found_boundary = true;
            break;
        }
        // Assistant message without tool_use is a safe boundary.
        if (messages[i].role == "assistant") {
            bool has_tool_use = false;
            for (const auto& b : messages[i].content)
                if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
            if (!has_tool_use) {
                keep_from = i;
                found_boundary = true;
                break;
            }
            // Assistant with tool_use but no following user with tool_result
            // (should have been repaired) - treat as boundary.
            if (i + 1 >= messages.size() || messages[i + 1].role != "user") {
                keep_from = i;
                found_boundary = true;
                break;
            }
            // Continue walking back.
        }
    }

    // If no boundary found (shouldn't happen), don't compact
    if (!found_boundary) return false;

    // Ensure we keep at least the last 2 messages (final user turn + optional assistant).
    // BUT: only if the kept portion doesn't start with an orphaned tool_use
    if (keep_from < messages.size() - 2) {
        // Check if messages[keep_from] is an assistant with tool_use but no following user
        if (keep_from < messages.size() && messages[keep_from].role == "assistant") {
            bool has_tool_use = false;
            for (const auto& b : messages[keep_from].content)
                if (b.type == ContentBlock::TOOL_USE) { has_tool_use = true; break; }
            if (has_tool_use && (keep_from + 1 >= messages.size() || messages[keep_from + 1].role != "user")) {
                // This would leave an orphaned tool_use - don't force the minimum
            } else {
                keep_from = messages.size() - 2;
            }
        } else {
            keep_from = messages.size() - 2;
        }
    }

    // Score messages in the to-be-compacted region for importance
    // This helps us build a better summary prompt that emphasizes key parts
    struct ScoredBlock {
        std::string text;
        double importance;
        std::string type;  // "user", "assistant", "tool_use", "tool_result"
    };
    std::vector<ScoredBlock> scored_blocks;
    scored_blocks.reserve(keep_from);

    for (size_t i = 0; i < keep_from; ++i) {
        const auto& msg = messages[i];
        std::string role_prefix = "[" + msg.role + "]";
        double base_importance = (msg.role == "user") ? 0.6 : 0.4;
        
        for (const auto& b : msg.content) {
            if (b.type == ContentBlock::TEXT && !b.text.empty()) {
                ScoredBlock sb;
                sb.text = b.text;
                sb.type = msg.role;
                sb.importance = base_importance;
                
                // Boost importance for key content
                if (b.text.find("error") != std::string::npos ||
                    b.text.find("fail") != std::string::npos ||
                    b.text.find("success") != std::string::npos ||
                    b.text.find("fix") != std::string::npos ||
                    b.text.find("change") != std::string::npos ||
                    b.text.find("decision") != std::string::npos) {
                    sb.importance += 0.2;
                }
                // File paths are important
                if (b.text.find('/') != std::string::npos &&
                    (b.text.find(".cpp") != std::string::npos ||
                     b.text.find(".h") != std::string::npos ||
                     b.text.find(".py") != std::string::npos ||
                     b.text.find(".js") != std::string::npos ||
                     b.text.find(".json") != std::string::npos)) {
                    sb.importance += 0.15;
                }
                // Tool results with errors are important
                if (msg.role == "user" && b.type == ContentBlock::TOOL_RESULT) {
                    sb.importance += 0.1;
                }
                sb.importance = std::min(1.0, sb.importance);
                scored_blocks.push_back(std::move(sb));
            } else if (b.type == ContentBlock::TOOL_USE) {
                ScoredBlock sb;
                sb.text = "(tool_use " + b.tool_name + ")";
                sb.type = "tool_use";
                sb.importance = 0.7;
                scored_blocks.push_back(std::move(sb));
            } else if (b.type == ContentBlock::TOOL_RESULT) {
                ScoredBlock sb;
                std::string out = b.text;
                if (out.size() > 300) out = out.substr(0, 300) + "...";
                sb.text = "(tool_result: " + out + ")";
                sb.type = "tool_result";
                sb.importance = b.is_error ? 0.8 : 0.5;
                scored_blocks.push_back(std::move(sb));
            } else if (b.type == ContentBlock::IMAGE) {
                ScoredBlock sb;
                sb.text = "(image: " + b.image_media_type + ")";
                sb.type = "image";
                sb.importance = 0.5;
                scored_blocks.push_back(std::move(sb));
            }
        }
    }

    // Build conversation text with importance markers for the summarizer
    std::string conversation_text;
    conversation_text.reserve(50000);
    
    for (size_t i = 0; i < keep_from; ++i) {
        const auto& msg = messages[i];
        conversation_text += "[" + msg.role + "]\n";
        for (const auto& b : msg.content) {
            if (b.type == ContentBlock::TEXT && !b.text.empty()) {
                conversation_text += b.text + "\n";
            } else if (b.type == ContentBlock::TOOL_USE) {
                conversation_text += "(tool_use " + b.tool_name + ")\n";
            } else if (b.type == ContentBlock::TOOL_RESULT) {
                std::string out = b.text;
                if (out.size() > 200) out = out.substr(0, 200) + "...";
                conversation_text += "(tool_result: " + out + ")\n";
            } else if (b.type == ContentBlock::IMAGE) {
                conversation_text += "(image: " + b.image_media_type + ")\n";
            }
        }
    }

    // Cap summary input to avoid blowing up the compaction request itself.
    if (conversation_text.size() > 60000)
        conversation_text = conversation_text.substr(conversation_text.size() - 60000);

    if (ui.on_status) ui.on_status("Compacting conversation...");

    // Use summary_model if configured (e.g., smaller/faster model for compaction)
    std::string summary_model = cfg.summary_model.empty() ? cfg.model : cfg.summary_model;
    
    // Use custom prompt if provided
    std::string summary_prompt = cfg.summary_prompt.empty() 
        ? "You are a conversation summarizer. Produce a concise, structured summary "
          "of the conversation below, preserving:\n"
          "1. Key decisions and rationale\n"
          "2. Code changes and file paths mentioned\n"
          "3. Errors encountered and their resolutions\n"
          "4. Tool usage patterns and outcomes\n"
          "5. Important context for continuing the conversation\n\n"
          "Format as bullet points. Be factual and brief."
        : cfg.summary_prompt;

    // Ask the model to summarize.
    CompletionRequest req;
    req.model = summary_model;
    req.system = summary_prompt;
    req.max_tokens = cfg.summary_max_tokens > 0 ? cfg.summary_max_tokens : 2048;
    req.use_cache = false;

    Message sum_user;
    sum_user.role = "user";
    ContentBlock tb;
    tb.type = ContentBlock::TEXT;
    tb.text = "Summarize this conversation:\n\n" + conversation_text;
    sum_user.content.push_back(std::move(tb));
    req.messages.push_back(std::move(sum_user));

    std::string summary;
    bool had_error = false;
    provider.stream(req, [&](const StreamEvent& ev) {
        if (ev.type == StreamEvent::TEXT_DELTA)
            summary += ev.text;
        else if (ev.type == StreamEvent::ERROR)
            had_error = true;
    });

    if (had_error || summary.empty()) return false;

    if (result) {
        result->summary_tokens = provider.estimate_tokens(summary);
    }

    // Replace old messages with a single user message containing the summary.
    std::vector<Message> compacted;
    Message summary_msg;
    summary_msg.role = "user";
    ContentBlock sb;
    sb.type = ContentBlock::TEXT;
    sb.text = "[Conversation summary — earlier messages were compacted "
        "to free context space]\n\n" + summary;
    summary_msg.content.push_back(std::move(sb));
    compacted.push_back(std::move(summary_msg));

    // Append the kept tail.
    for (size_t i = keep_from; i < messages.size(); ++i)
        compacted.push_back(std::move(messages[i]));

    messages = std::move(compacted);

    if (result) {
        result->messages_after = static_cast<int>(messages.size());
        result->tokens_after = estimate_tokens(provider, messages);
        result->performed = true;
    }

    if (ui.on_status) {
        ui.on_status("Compacted: " + std::to_string(keep_from)
            + " messages summarized, ~" + std::to_string(result ? result->tokens_before - result->tokens_after : 0)
            + " tokens saved");
    }
    return true;
}

// Backward-compatible overload
bool MaybeCompact(Provider& provider,
                   const Config& cfg,
                   std::vector<Message>& messages,
                   int cumulative_input_tokens,
                   const UiCallbacks& ui) {
    return MaybeCompact(provider, cfg, messages, cumulative_input_tokens, ui, nullptr);
}

}  // namespace pluto
