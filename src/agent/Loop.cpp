#include "Loop.h"
#include "../rag.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace pluto {

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

// Rough token estimate: 4 chars per token. Used only for the local
// truncation heuristic, not for billing.
size_t estimate_tokens(const std::vector<Message>& msgs) {
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

// Find the end of the first complete "round" starting at index 0.
// A round is: user → assistant [→ user(tool_result) → assistant]*.
// We must drop entire rounds to avoid orphaned tool_result messages
// that reference a deleted tool_use id (which the API would reject).
// Returns the number of messages in the first complete round, or 0
// if no safe boundary was found.
size_t first_round_end(const std::vector<Message>& msgs) {
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

// Drop the oldest complete rounds until estimated tokens fit under
// `target`. Never drops the most recent user turn. Preserves
// tool_use/tool_result pairs as atomic units to avoid API errors.
// Returns the number of rounds dropped.
int prune_to_fit(std::vector<Message>& msgs, size_t target) {
	int dropped = 0;
	while (estimate_tokens(msgs) > target && msgs.size() > 2) {
		size_t boundary = first_round_end(msgs);
		if (boundary == 0 || boundary >= msgs.size()) break;
		msgs.erase(msgs.begin(), msgs.begin() + boundary);
		++dropped;
	}
	return dropped;
}

// Run a hook command in a child process with env vars set safely.
// This avoids shell injection: env var values are passed via setenv()
// in the child process, never interpolated into the command string.
int run_hook_safe(const char* cmd, const char* env_name, const std::string& env_value) {
	pid_t pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		// Child: set env var safely, then exec the hook via /bin/sh.
		::setenv(env_name, env_value.c_str(), 1);
		::execl("/bin/sh", "sh", "-c", cmd, nullptr);
		::_exit(127);
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
              std::atomic<bool>* cancel) {
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
			// Next is user — check it has tool_results.
			bool has_result = false;
			for (const auto& b : messages[i + 1].content)
				if (b.type == ContentBlock::TOOL_RESULT) { has_result = true; break; }
			if (!has_result) needs_repair = true;
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

	// Snapshot size at entry. On any failure during this turn we rewind to
	// here so the user's original prompt is preserved but half-built
	// assistant/tool_result turns are dropped — otherwise a failed turn
	// leaves garbage in history that breaks subsequent requests.
	const size_t entry_size = messages.size();

	// Local context-window truncation. We use a soft cap at 80% of the
	// configured context window, estimated cheaply from char counts.
	if (cfg.context_window > 0) {
		size_t target = static_cast<size_t>(cfg.context_window) * 80 / 100;
		int dropped = prune_to_fit(messages, target);
		if (dropped > 0) {
			std::fprintf(stderr,
				"[pluto] context window pressure: dropped %d oldest pair(s)\n",
				dropped);
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
		static thread_local bool tools_disabled = false;
		static thread_local std::string tools_disabled_model;
		if (tools_disabled && tools_disabled_model != cfg.model) {
			tools_disabled = false;
			tools_disabled_model.clear();
		}
		bool skip_tools = tools_disabled;
		req.tools = skip_tools ? std::vector<ToolDef>{} : tool_defs;
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
				case StreamEvent::TOOL_USE_START: {
					active_text_idx = -1;
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
						ui.on_usage(ev.input_tokens, ev.output_tokens,
						            ev.cache_read_tokens,
						            ev.cache_creation_tokens);
					}
					break;
				case StreamEvent::ERROR:
					had_error = true;
					// Detect "no tool use support" errors from providers.
					if (ev.error.find("tool use") != std::string::npos
					    || ev.error.find("tool_use") != std::string::npos
					    || ev.error.find("tools") != std::string::npos) {
						tool_use_unsupported = true;
					}
					if (ui.on_error) ui.on_error(ev.error);
					break;
				case StreamEvent::RETRY_INFO:
					if (ui.on_status) ui.on_status(ev.text);
					break;
			}
		});

		if (had_error) {
			if (tool_use_unsupported && !req.tools.empty()) {
				// Retry without tools — the model doesn't support them.
				// Remember for subsequent turns until model changes.
				tools_disabled = true;
				tools_disabled_model = cfg.model;
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
				messages.resize(entry_size);
				return;
			}
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
				if (ui.on_tool_result)
					ui.on_tool_result(block.tool_name, block.tool_input, denied);
				user_results.content.push_back(result);
				continue;
			}

			run_hooks(cfg.hooks_pre_tool, "pre", block.tool_name);
			ToolResult tr = tool->execute(block.tool_input);
			run_hooks(cfg.hooks_post_tool, "post", block.tool_name);
			cap_tool_output(tr.output);
			result.is_error = tr.is_error;
			result.text = tr.output;
			if (ui.on_tool_result)
				ui.on_tool_result(block.tool_name, block.tool_input, tr);
			user_results.content.push_back(result);
		}

		messages.push_back(user_results);
		// Loop: next iteration feeds tool_results back to the model.
	}
}

bool MaybeCompact(Provider& provider,
                   const Config& cfg,
                   std::vector<Message>& messages,
                   int cumulative_input_tokens,
                   const UiCallbacks& ui) {
	if (cfg.context_window <= 0) return false;
	int threshold = cfg.context_window * 90 / 100;
	if (cumulative_input_tokens < threshold) return false;
	if (messages.size() <= 2) return false;  // nothing useful to compact

	// Keep the last user turn intact. Summarize everything before it.
	// Find the split point: we want to summarize all but the last 2 messages
	// (the final user turn and, if present, the preceding assistant response).
	size_t keep_from = messages.size() >= 4 ? messages.size() - 2 : messages.size() - 1;

	// Build the text to summarize.
	std::string conversation_text;
	for (size_t i = 0; i < keep_from; ++i) {
		conversation_text += "[" + messages[i].role + "]\n";
		for (const auto& b : messages[i].content) {
			if (b.type == ContentBlock::TEXT && !b.text.empty()) {
				conversation_text += b.text + "\n";
			} else if (b.type == ContentBlock::TOOL_USE) {
				conversation_text += "(tool_use " + b.tool_name + ")\n";
			} else if (b.type == ContentBlock::TOOL_RESULT) {
				std::string out = b.text;
				if (out.size() > 200) out = out.substr(0, 200) + "...";
				conversation_text += "(tool_result: " + out + ")\n";
			}
		}
	}

	// Cap summary input to avoid blowing up the compaction request itself.
	if (conversation_text.size() > 60000)
		conversation_text = conversation_text.substr(conversation_text.size() - 60000);

	if (ui.on_status) ui.on_status("Compacting conversation...");

	// Ask the model to summarize.
	CompletionRequest req;
	req.model = cfg.model;
	req.system = "You are a conversation summarizer. Produce a concise summary "
		"of the conversation below, preserving key decisions, code changes, "
		"file paths, and important context. Be factual and brief.";
	req.max_tokens = 2048;
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

	if (ui.on_status) {
		ui.on_status("Compacted: " + std::to_string(keep_from)
			+ " messages summarized");
	}
	return true;
}

}  // namespace pluto
