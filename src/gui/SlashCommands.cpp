// MainWindow slash command handling — extracted for maintainability.
// This file implements MainWindow::_HandleSlashCommand().

#include "MainWindow.h"

#include <Clipboard.h>
#include <Message.h>
#include <StringView.h>
#include <image.h>

#include <ctime>
#include <thread>

#include "../config.h"
#include "../routine.h"
#include "../storage/SessionStore.h"
#include "../wiki.h"
#include "../workspace.h"
#include "../map/Graph.h"
#include "../map/Scanner.h"
#include "../map/BfsAttrs.h"
#include "ChatColors.h"

namespace pluto {
namespace gui {

// Slash command handling in GUI input field.
// Returns true if the line was handled as a command (no turn to send).
bool MainWindow::_HandleSlashCommand(const std::string& text) {
	if (text.empty() || text[0] != '/') return false;

	if (text == "/reset" || text == "/clear") {
		_ResetConversation();
		return true;
	}

	if (text == "/tools") {
		// Build set of disabled tools from active mode.
		std::set<std::string> disabled;
		if (!fConfig->active_mode.empty()) {
			const pluto::ModeEntry* m = pluto::FindMode(*fConfig, fConfig->active_mode);
			if (m) {
				for (const auto& dt : m->disabled_tools)
					disabled.insert(dt);
			}
		}
		_Append("Registered tools:\n", kColDim, B_BOLD_FACE);
		for (const Tool* t : fTools->list()) {
			bool hidden = disabled.count(t->name()) > 0;
			std::string desc = t->description();
			size_t nl = desc.find('\n');
			if (nl != std::string::npos) desc = desc.substr(0, nl);
			if (desc.size() > 70) desc = desc.substr(0, 67) + "…";
			std::string line = "  " + t->name() + " — " + desc;
			if (hidden)
				line += " (disabled by " + fConfig->active_mode + " mode)";
			line += "\n";
			_Append(line, kColDim);
		}
		_Append("\n", kColDim);
		return true;
	}

	if (text == "/cost") {
		auto* ac = _ActiveConv();
		if (!ac) return true;
		BString info;
		long tIn = ac->GetTokensIn();
		long tOut = ac->GetTokensOut();
		info.SetToFormat("Turns: %ld\nInput:  %ld tokens\nOutput: %ld tokens\n"
			"Total:  %ld tokens\nCost:   $%.4f\n\n",
			(long)(tIn > 0 || tOut > 0 ? 1 : 0),
			tIn, tOut, tIn + tOut,
			ac->EstimateCost());
		_Append(info.String(), kColDim);
		return true;
	}

	if (text == "/copy") {
		auto* ac = _ActiveConv();
		if (!ac) return true;
		const auto& msgs = ac->Messages();
		std::string body;
		for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
			if (it->role != "assistant") continue;
			for (const auto& b : it->content)
				if (b.type == ContentBlock::TEXT) body += b.text;
			break;
		}
		if (body.empty()) {
			_Append("(nothing to copy)\n\n", kColDim);
			return true;
		}
		BClipboard clipboard("system");
		if (!clipboard.Lock()) {
			_Append("[copy] cannot lock clipboard\n\n", kColDim);
			return true;
		}
		clipboard.Clear();
		BMessage* data = clipboard.Data();
		if (data) {
			data->AddData("text/plain", B_MIME_TYPE,
				body.data(), body.size());
			clipboard.Commit();
		}
		clipboard.Unlock();
		BString info;
		info.SetToFormat("copied %zu bytes to clipboard\n\n", body.size());
		_Append(info.String(), kColDim);
		return true;
	}

	if (text == "/hotkeys") {
		_Append(
			"Keyboard shortcuts:\n"
			"  Cmd+N              new conversation\n"
			"  Cmd+T              new tab\n"
			"  Cmd+W              close tab\n"
			"  Cmd+Left / Right   previous / next tab\n"
			"  Cmd+S              save session\n"
			"  Cmd+R              resume last session\n"
			"  Cmd+B              session browser\n"
			"  Cmd+F              find in conversation\n"
			"  Cmd+,              preferences\n"
			"  Cmd+= / -          zoom in / out\n"
			"  Cmd+0              reset zoom\n"
			"  Cmd+Q              quit\n"
			"  Enter              send message\n"
			"  Shift+Enter        newline in input\n"
			"  Escape             cancel mid-stream\n\n",
			kColDim);
		return true;
	}

	if (text.rfind("/model", 0) == 0) {
		if (text.size() <= 7) {
			std::string m = _ActiveConv()
				? _ActiveConv()->GetModel() : fConfig->model;
			_Append("Model: " + m + "\n\n", kColDim);
		} else {
			std::string newModel = text.substr(7);
			while (!newModel.empty() && newModel.front() == ' ')
				newModel.erase(0, 1);
			// Update active tab's model + auto-detect context + safety cap.
			if (_ActiveConv()) {
				Config& tc = _ActiveConv()->MutableConfig();
				tc.model = newModel;
				AutoDetectProvider(tc);
				int guessed_ctx = pluto::GuessContextWindow(newModel);
				if (guessed_ctx > 0) tc.context_window = guessed_ctx;
				{
					int out_limit = pluto::GuessMaxOutputTokens(newModel);
					int ctx_cap = tc.context_window * 3 / 4;
					int cap = std::min(out_limit, ctx_cap);
					if (tc.max_tokens > cap)
						tc.max_tokens = std::max(4096, cap);
				}
				auto p = pluto::CreateProvider(tc);
				if (p) _ActiveConv()->SetProvider(std::move(p));
				// Sync to global config for persistence.
				fConfig->model = tc.model;
				fConfig->provider = tc.provider;
				fConfig->api_key_env = tc.api_key_env;
				fConfig->max_tokens = tc.max_tokens;
				fConfig->context_window = tc.context_window;
				pluto::SaveConfig(*fConfig);
			}
			_Append("Model switched to: " + newModel + "\n\n", kColDim);
			_UpdateTitle();
			_UpdateStatus();
		}
		return true;
	}

	if (text == "/plan") {
		if (fConfig->active_mode == "plan")
			fConfig->active_mode.clear();
		else
			fConfig->active_mode = "plan";
		_Append(std::string("Plan mode ") +
			(fConfig->active_mode == "plan" ? "ON" : "OFF") +
			(fConfig->active_mode == "plan"
				? " — read-only analysis, no file writes\n\n"
				: " — full tool access restored\n\n"),
			kColDim);
		_UpdateStatus();
		return true;
	}

	if (text == "/modes") {
		if (fConfig->modes.empty()) {
			_Append("No modes available.\n\n", kColDim);
		} else {
			_Append("Available modes:\n", kColDim, B_BOLD_FACE);
			for (const auto& m : fConfig->modes) {
				std::string mark = (m.name == fConfig->active_mode) ? "* " : "  ";
				_Append(mark + m.name + " — " + m.description + "\n", kColDim);
			}
			_Append("\n", kColDim);
		}
		return true;
	}

	if (text.rfind("/mode", 0) == 0 && (text.size() == 5 || text[5] == ' ')) {
		std::string arg;
		if (text.size() > 6) arg = text.substr(6);
		while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
		while (!arg.empty() && arg.back() == ' ') arg.pop_back();

		if (arg.empty()) {
			if (fConfig->active_mode.empty()) {
				_Append("No mode active — full tool access.\n\n", kColDim);
			} else {
				const pluto::ModeEntry* m = pluto::FindMode(*fConfig, fConfig->active_mode);
				_Append("Mode: " + fConfig->active_mode + " — "
					+ (m ? m->description : "unknown") + "\n\n", kColDim);
			}
		} else if (arg == "off" || arg == "none" || arg == "default") {
			// Restore global model if the mode had overridden it.
			const pluto::ModeEntry* prev = pluto::FindMode(*fConfig, fConfig->active_mode);
			fConfig->active_mode.clear();
			if (prev && !prev->model.empty() && _ActiveConv()) {
				pluto::Config& tc = _ActiveConv()->MutableConfig();
				tc.model = fConfig->model;
				pluto::AutoDetectProvider(tc);
				auto p = pluto::CreateProvider(tc);
				if (p) _ActiveConv()->SetProvider(std::move(p));
			}
			_Append("Mode cleared — full tool access.\n\n", kColDim);
		} else {
			const pluto::ModeEntry* m = pluto::FindMode(*fConfig, arg);
			if (!m) {
				_Append("Unknown mode \"" + arg + "\" — use /modes to list.\n\n", kColDim);
			} else if (fConfig->active_mode == arg) {
				// Restore global model if the mode had overridden it.
				if (!m->model.empty() && _ActiveConv()) {
					pluto::Config& tc = _ActiveConv()->MutableConfig();
					tc.model = fConfig->model;
					pluto::AutoDetectProvider(tc);
					auto p = pluto::CreateProvider(tc);
					if (p) _ActiveConv()->SetProvider(std::move(p));
				}
				fConfig->active_mode.clear();
				_Append("Mode " + arg + " OFF — full tool access.\n\n", kColDim);
			} else {
				fConfig->active_mode = arg;
				_Append("Mode: " + m->name + " — " + m->description + "\n", kColDim);
				if (!m->model.empty() && _ActiveConv()) {
					pluto::Config& tc = _ActiveConv()->MutableConfig();
					tc.model = m->model;
					pluto::AutoDetectProvider(tc);
					int ctx = pluto::GuessContextWindow(m->model);
					if (ctx > 0) tc.context_window = ctx;
					int out_limit = pluto::GuessMaxOutputTokens(m->model);
					int ctx_cap = tc.context_window * 3 / 4;
					int cap = std::min(out_limit, ctx_cap);
					if (tc.max_tokens > cap)
						tc.max_tokens = std::max(4096, cap);
					auto p = pluto::CreateProvider(tc);
					if (p) _ActiveConv()->SetProvider(std::move(p));
					_Append("  model -> " + m->model + "\n", kColDim);
				}
				_Append("\n", kColDim);
			}
		}
		_UpdateStatus();
		return true;
	}

	if (text.rfind("/init", 0) == 0 && (text.size() == 5 || text[5] == ' ')) {
		std::string root = pluto::WorkspaceRoot();
		if (root.empty()) {
			char buf[PATH_MAX];
			if (getcwd(buf, sizeof(buf))) root = buf;
		}
		bool force = (text.find("--force") != std::string::npos);

		std::string md_path = root + "/pluto.md";
		struct stat init_st;
		if (!force && ::stat(md_path.c_str(), &init_st) == 0) {
			_Append("pluto.md already exists (" + md_path + ")\n",
				kColDim, B_BOLD_FACE);
			_Append("Use /init --force to overwrite.\n\n", kColDim);
			return true;
		}

		// Scan the project.
		_Append("Scanning project…\n", kColDim);
		pluto::ProjectInfo info = pluto::DetectProject(root);

		// Show scan results in chat.
		if (info.total_files > 0) {
			char buf[64];
			snprintf(buf, sizeof(buf), "  Scanned %d files, %d lines\n",
				info.total_files, info.total_lines);
			_Append(buf, kColDim);
		}
		_Append("Generating pluto.md with AI analysis…\n\n", kColDim);

		// Show scan results in the side panel.
		{
			fSideTitle->SetText("Project scan");
			std::string summary;
			std::string name = root;
			size_t sl = name.rfind('/');
			if (sl != std::string::npos) name = name.substr(sl + 1);

			summary += "━━ Project: " + name + " ━━\n\n";
			if (!info.git_remote.empty())
				summary += "  Repo:      " + info.git_remote + "\n";
			if (!info.main_language.empty())
				summary += "  Language:  " + info.main_language + "\n";
			if (info.total_files > 0) {
				char buf[64];
				snprintf(buf, sizeof(buf), "  Files:     %d  (%d lines)\n",
					info.total_files, info.total_lines);
				summary += buf;
			}
			if (!info.subdirs.empty()) {
				summary += "\n━━ Directories ━━\n\n";
				for (const auto& d : info.subdirs)
					summary += "  " + d + "/\n";
			}
			if (!info.lang_stats.empty()) {
				summary += "\n━━ Languages ━━\n\n";
				for (const auto& ls : info.lang_stats) {
					if (ls.files == 0) continue;
					char row[80];
					snprintf(row, sizeof(row), "  %-18s %5d files  %7d lines\n",
						ls.language.c_str(), ls.files, ls.lines);
					summary += row;
				}
			}
			int32 n = static_cast<int32>(summary.size());
			fSideView->SetText(summary.c_str(), n);
			BFont mono = *be_fixed_font;
			fSideView->SetFontAndColor(0, n, &mono, B_FONT_ALL, nullptr);
		}

		// Build the LLM prompt with all collected project context.
		std::string prompt;
		prompt += "Analyze this project and generate a `pluto.md` project memory file.\n\n";
		prompt += "The file will be automatically injected into your system prompt in every future conversation about this project. ";
		prompt += "Write it in markdown. Include:\n";
		prompt += "- Project name and brief description\n";
		prompt += "- Architecture overview (main components, how they connect)\n";
		prompt += "- Key conventions (naming, code style, patterns used)\n";
		prompt += "- Build commands (compile, test, run)\n";
		prompt += "- Important files and directories and their purpose\n";
		prompt += "- Any gotchas or things to watch out for\n\n";
		prompt += "Be specific to THIS project based on the code I'm providing. Don't be generic.\n";
		prompt += "Output ONLY the markdown content for pluto.md, nothing else.\n\n";

		prompt += "── PROJECT INFO ──\n";
		{
			std::string name = root;
			size_t sl = name.rfind('/');
			if (sl != std::string::npos) name = name.substr(sl + 1);
			prompt += "Name: " + name + "\n";
		}
		if (!info.git_remote.empty())
			prompt += "Git remote: " + info.git_remote + "\n";
		if (!info.main_language.empty())
			prompt += "Primary language: " + info.main_language + "\n";
		if (info.total_files > 0) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Total: %d files, %d lines\n",
				info.total_files, info.total_lines);
			prompt += buf;
		}
		if (!info.subdirs.empty()) {
			prompt += "Directories: ";
			for (size_t i = 0; i < info.subdirs.size(); ++i) {
				if (i > 0) prompt += ", ";
				prompt += info.subdirs[i];
			}
			prompt += "\n";
		}
		if (!info.lang_stats.empty()) {
			prompt += "\nLanguage breakdown:\n";
			for (const auto& ls : info.lang_stats) {
				if (ls.files == 0) continue;
				char row[80];
				snprintf(row, sizeof(row), "  %s: %d files, %d lines\n",
					ls.language.c_str(), ls.files, ls.lines);
				prompt += row;
			}
		}
		if (!info.interesting_files.empty()) {
			prompt += "\nProject files found: ";
			for (size_t i = 0; i < info.interesting_files.size(); ++i) {
				if (i > 0) prompt += ", ";
				prompt += info.interesting_files[i];
			}
			prompt += "\n";
		}
		if (!info.readme_excerpt.empty()) {
			prompt += "\n── README ──\n" + info.readme_excerpt + "\n";
		}
		if (!info.dependencies_excerpt.empty()) {
			prompt += "\n── BUILD/DEPENDENCIES ──\n" + info.dependencies_excerpt + "\n";
		}
		if (!info.source_samples.empty()) {
			prompt += "\n── KEY SOURCE FILES ──\n" + info.source_samples + "\n";
		}

		// Save the path — OnTurnDone will write the response to this file.
		fInitPendingPath = md_path;
		fInitMsgCount = _ActiveConv()->Messages().size();

		// Send as a user message to the active conversation.
		_ActiveConv()->AppendText("You\n", kColUserRole, B_BOLD_FACE);
		_Append("/init — AI project analysis\n\n", kColUserBody);

		std::vector<ContentBlock> blocks;
		ContentBlock tb;
		tb.type = ContentBlock::TEXT;
		tb.text = prompt;
		blocks.push_back(std::move(tb));
		_ActiveConv()->SendInput(std::move(blocks));

		return true;
	}

	if (text.rfind("/title", 0) == 0 && (text.size() == 6 || text[6] == ' ')) {
		if (text.size() <= 7) {
			std::string st = _ActiveConv()->GetSessionTitle();
			_Append("Title: " + (st.empty()
				? std::string("(none)") : st) + "\n\n", kColDim);
		} else {
			std::string newTitle = text.substr(7);
			while (!newTitle.empty() && newTitle.front() == ' ')
				newTitle.erase(0, 1);
			_ActiveConv()->SetSessionTitle(newTitle);
			_Append("Title: " + newTitle + "\n\n", kColDim);
			_UpdateTitle();
		}
		return true;
	}

	if (text.rfind("/tag", 0) == 0 && (text.size() == 4 || text[4] == ' ')) {
		if (_ActiveConv()->GetSessionPath().empty()) {
			_Append("(no session — save first with Cmd+S)\n\n", kColDim);
			return true;
		}
		std::string tag;
		if (text.size() > 5) {
			tag = text.substr(5);
			while (!tag.empty() && tag.front() == ' ') tag.erase(0, 1);
		}
		if (tag.empty()) {
			auto tags = pluto::SessionStore::get_tags(_ActiveConv()->GetSessionPath());
			if (tags.empty()) {
				_Append("(no tags)\n\n", kColDim);
			} else {
				std::string line = "Tags: ";
				for (size_t i = 0; i < tags.size(); ++i) {
					if (i > 0) line += ", ";
					line += tags[i];
				}
				_Append(line + "\n\n", kColDim);
			}
			return true;
		}
		std::string err;
		if (!pluto::SessionStore::add_tag(_ActiveConv()->GetSessionPath(), tag, err))
			_Append("[tag] " + err + "\n\n", kColErr);
		else
			_Append("Tagged: " + tag + "\n\n", kColDim);
		return true;
	}

	if (text.rfind("/untag ", 0) == 0) {
		if (_ActiveConv()->GetSessionPath().empty()) {
			_Append("(no session loaded)\n\n", kColDim);
			return true;
		}
		std::string tag = text.substr(7);
		while (!tag.empty() && tag.front() == ' ') tag.erase(0, 1);
		if (tag.empty()) return true;
		std::string err;
		if (!pluto::SessionStore::remove_tag(_ActiveConv()->GetSessionPath(), tag, err))
			_Append("[untag] " + err + "\n\n", kColErr);
		else
			_Append("Untagged: " + tag + "\n\n", kColDim);
		return true;
	}

	if (text == "/test-side") {
		// Render a test document in the side panel to verify markdown styling.
		std::string md =
			"# Pluto Detail Panel\n\n"
			"## Code Block\n\n"
			"```cpp\n"
			"#include <Window.h>\n"
			"#include <Application.h>\n\n"
			"int main() {\n"
			"    BApplication app(\"application/x-demo\");\n"
			"    BWindow* w = new BWindow(\n"
			"        BRect(100, 100, 500, 400),\n"
			"        \"Hello\", B_TITLED_WINDOW,\n"
			"        B_QUIT_ON_WINDOW_CLOSE);\n"
			"    w->Show();\n"
			"    app.Run();\n"
			"    return 0;\n"
			"}\n"
			"```\n\n"
			"## Inline Formatting\n\n"
			"This is **bold**, *italic*, and `inline code`.\n"
			"A [link to Haiku](https://haiku-os.org) and ~~struck~~.\n\n"
			"## Blockquote\n\n"
			"> Every OS deserves its own AI client.\n"
			"> Especially the one with BFS.\n\n"
			"## Lists\n\n"
			"- First item with `code`\n"
			"- Second item with **bold**\n"
			"- Third item with [link](https://example.com)\n\n"
			"1. Numbered one\n"
			"2. Numbered two\n"
			"3. Numbered three\n\n"
			"## Task List\n\n"
			"- [x] Syntax highlighting\n"
			"- [x] Code block borders\n"
			"- [ ] World domination\n\n"
			"## Table\n\n"
			"| Feature | Status | Notes |\n"
			"| --- | --- | --- |\n"
			"| Markdown | Done | Full rendering |\n"
			"| Syntax | Done | C++/Python/JS |\n"
			"| Tables | Done | Auto-width |\n\n"
			"---\n\n"
			"*End of test.*\n";
		fSideTitle->SetText("\xF0\x9F\xA7\xAA Test — Markdown Rendering");
		_RenderMarkdown(md);
		_Append("Side panel test rendered. Check the detail panel.\n\n", kColDim);
		return true;
	}

	if (text.rfind("/wiki", 0) == 0 && (text.size() == 5 || text[5] == ' ')) {
		std::string arg;
		if (text.size() > 6) arg = text.substr(6);
		while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
		while (!arg.empty() && arg.back() == ' ') arg.pop_back();

		if (arg.empty()) {
			// Show wiki stats + list.
			std::string dir = pluto::WikiDefaultDir();
			if (dir.empty()) {
				_Append("Wiki empty. Use /wiki <url-or-file> to ingest.\n\n", kColDim);
			} else {
				pluto::WikiInit(dir);
				int n = pluto::WikiEntryCount();
				char buf[128];
				snprintf(buf, sizeof(buf), "Wiki: %d entries in %s/\n", n, dir.c_str());
				_Append(buf, kColDim, B_BOLD_FACE);
				for (const auto& e : pluto::WikiEntries()) {
					std::string line = "  " + e.topic;
					if (!e.tags.empty()) {
						line += "  [";
						for (size_t i = 0; i < e.tags.size(); ++i) {
							if (i > 0) line += ", ";
							line += e.tags[i];
						}
						line += "]";
					}
					line += "\n";
					_Append(line, kColDim);
				}
				_Append("\n", kColDim);
			}
		} else {
			// Ingest URL or file in background.
			_Append("Wiki ingest: " + arg + "\n", kColDim);
			std::string input_arg = arg;
			// Run ingest in a background thread to avoid blocking the GUI.
			std::thread([this, input_arg]() {
				// Find our own executable.
				std::string self_exe;
				{
					image_info info;
					int32 cookie = 0;
					if (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK)
						self_exe = info.name;
					else
						self_exe = "pluto";
				}
				std::string cmd = "'" + self_exe + "' wiki ingest '"
					+ input_arg + "' 2>&1";
				FILE* pipe = popen(cmd.c_str(), "r");
				std::string output;
				if (pipe) {
					char buf[512];
					while (fgets(buf, sizeof(buf), pipe))
						output += buf;
					pclose(pipe);
				}
				// Post result back to the window thread.
				BMessage* msg = new BMessage('wiki');
				msg->AddString("result", output.c_str());
				PostMessage(msg);
			}).detach();
		}
		return true;
	}

	if (text.rfind("/map", 0) == 0 && (text.size() == 4 || text[4] == ' ')) {
		std::string root = WorkspaceRoot();
		if (root.empty()) {
			char buf[PATH_MAX];
			if (getcwd(buf, sizeof(buf))) root = buf;
		}
		std::string graph_path = root + "/graph.json";

		std::string arg;
		if (text.size() > 4) {
			arg = text.substr(4);
			while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
		}

		if (arg.empty()) {
			// Scan workspace and open graph viewer with progress updates.
			_Append("Scanning workspace...\n", kColDim);
			BMessenger winMsgr(this);
			std::thread([winMsgr, root, graph_path]() {
				auto graph = map::ScanWorkspace(root,
					[&winMsgr](int current, int total, const std::string& file) {
						BMessage progress('grPr');
						char buf[256];
						snprintf(buf, sizeof(buf), "Scanning %d/%d: %s",
							current, total, file.c_str());
						progress.AddString("status", buf);
						progress.AddInt32("current", current);
						progress.AddInt32("total", total);
						winMsgr.SendMessage(&progress);
					});
				graph.SaveToFile(graph_path);
				map::StampBfsAttributes(graph);

				// Open the graph window on the GUI thread.
				BMessage msg('grVw');
				msg.AddString("path", graph_path.c_str());
				winMsgr.SendMessage(&msg);
			}).detach();
		} else if (arg.rfind("path ", 0) == 0) {
			auto graph = map::Graph::LoadFromFile(graph_path);
			if (graph.Nodes().empty()) {
				_Append("No graph found. Run /map first.\n\n", kColToolRole);
				return true;
			}
			std::string rest = arg.substr(5);
			while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
			auto sp = rest.find(' ');
			if (sp == std::string::npos) {
				_Append("Usage: /map path <from> <to>\n\n", kColDim);
				return true;
			}
			std::string from_pat = rest.substr(0, sp);
			std::string to_pat = rest.substr(sp + 1);
			auto from_nodes = graph.FindNodes(from_pat);
			auto to_nodes = graph.FindNodes(to_pat);
			if (from_nodes.empty() || to_nodes.empty()) {
				_Append("Node not found.\n\n", kColToolRole);
				return true;
			}
			auto path = graph.PathBetween(from_nodes[0]->id, to_nodes[0]->id);
			if (path.empty()) {
				_Append("No path found.\n\n", kColDim);
				return true;
			}
			std::string out = "Path (" + std::to_string(path.size() - 1) + " hops):\n";
			for (size_t i = 0; i < path.size(); ++i) {
				out += "  " + path[i];
				if (i < path.size() - 1) out += " ->";
				out += "\n";
			}
			out += "\n";
			_Append(out, kColAsstBody);
		} else {
			// /map query ... or /map view — pass as user message for LLM
			auto graph = map::Graph::LoadFromFile(graph_path);
			if (graph.Nodes().empty()) {
				_Append("No graph found. Run /map first.\n\n", kColToolRole);
				return true;
			}
			// Fall through as false to let the prompt be sent to the LLM.
			return false;
		}
		return true;
	}

	if (text == "/help") {
		_Append(
			"GUI commands:\n"
			"  /tools             list registered tools\n"
			"  /model [ID]        show or switch model\n"
			"  /cost              show session token totals\n"
			"  /copy              copy last assistant reply to clipboard\n"
			"  /hotkeys           list keyboard shortcuts\n"
			"  /init [--force]    create pluto.md project memory\n"
			"  /wiki [url|file]   show wiki or ingest content\n"
			"  /map               scan workspace and show knowledge graph\n"
			"  /map path A B      find shortest path between two nodes\n"
			"  /title [new]       show or set session title\n"
			"  /tag [name]        show tags or add one to session\n"
			"  /untag <name>      remove a tag from session\n"
			"  /routines          list available routines\n"
			"  /routine <name>    run a routine in this chat\n"
			"  /commands          list custom slash commands\n"
			"  /help              show this help\n"
			"\n"
			"Custom commands from ~/config/settings/pluto/commands/\n"
			"are also available as /<name> [args].\n\n",
			kColDim);
		return true;
	}

	if (text == "/commands") {
		if (fCustomCommands.empty()) {
			_Append("(no custom commands)\n\n", kColDim);
		} else {
			_Append("Custom commands:\n", kColDim, B_BOLD_FACE);
			for (const auto& c : fCustomCommands) {
				std::string line = "  /" + c.name;
				if (!c.description.empty()) line += " — " + c.description;
				line += "\n";
				_Append(line, kColDim);
			}
			_Append("\n", kColDim);
		}
		return true;
	}

	// /routines — list available routines.
	if (text == "/routines") {
		auto routines = pluto::ListRoutines();
		if (routines.empty()) {
			_Append("(no routines)\n\n", kColDim);
		} else {
			_Append("Routines:\n", kColDim, B_BOLD_FACE);
			for (const auto& r : routines) {
				std::string line = "  " + r.name;
				line += "  [" + r.schedule + "]";
				if (!r.enabled) line += "  (paused)";
				line += "\n";
				_Append(line, kColDim);
			}
			_Append("\nUse /routine <name> to run one.\n\n", kColDim);
		}
		return true;
	}

	// /routine <name> — expand and send routine as a prompt.
	if (text.rfind("/routine ", 0) == 0) {
		std::string name = text.substr(9);
		while (!name.empty() && name.front() == ' ') name.erase(0, 1);
		while (!name.empty() && name.back() == ' ') name.pop_back();
		if (name.empty()) {
			_Append("Usage: /routine <name>\n\n", kColDim);
			return true;
		}
		auto routines = pluto::ListRoutines();
		for (const auto& r : routines) {
			if (r.name == name) {
				std::string prompt = pluto::BuildRoutinePrompt(r);
				fInput->SetText(prompt.c_str());
				return false;  // re-sent as normal prompt
			}
		}
		_Append("Routine not found: " + name + "\n", kColDim);
		_Append("Use /routines to see available routines.\n\n", kColDim);
		return true;
	}

	// Try custom slash commands.
	for (const auto& c : fCustomCommands) {
		size_t end = text.find(' ');
		std::string name = text.substr(1, end == std::string::npos
			? std::string::npos : end - 1);
		if (name != c.name) continue;
		std::string args = end == std::string::npos
			? std::string() : text.substr(end + 1);
		std::string expanded = c.body;
		size_t pos = 0;
		while ((pos = expanded.find("{{args}}", pos)) != std::string::npos) {
			expanded.replace(pos, 8, args);
			pos += args.size();
		}
		// Treat expanded text as a normal prompt — fall through below.
		fInput->SetText(expanded.c_str());
		return false;  // will be re-sent as normal text
	}

	return false;
}

}  // namespace gui
}  // namespace pluto
