// MainWindow session/workspace persistence — extracted for maintainability.

#include "MainWindow.h"

#include <Alert.h>
#include <Menu.h>
#include <MenuItem.h>
#include <ScrollBar.h>
#include <SplitView.h>
#include <TabView.h>

#include <fstream>
#include <sstream>

#include <sys/stat.h>

#include "../json.h"
#include "../storage/SessionStore.h"
#include "../util.h"
#include "../workspace.h"

namespace pluto {
namespace gui {

namespace {
const uint32 kMsgLoadSession = 'plLd';

std::string WorkspaceJsonPath() {
	std::string ws = WorkspaceRoot();
	if (!ws.empty()) {
		std::string dir = ws + "/.pluto";
		::mkdir(dir.c_str(), 0755);
		return dir + "/workspace.json";
	}
	return SettingsDir() + "/workspace.json";
}
}  // namespace


void MainWindow::_RebuildSessionsMenu() {
	if (!fSessionsMenu) return;
	while (BMenuItem* it = fSessionsMenu->RemoveItem((int32)0))
		delete it;

	auto all = SessionStore::list();
	for (const auto& m : all) {
		std::string label = m.title.empty() ? m.filename : m.title;
		BMessage* msg = new BMessage(kMsgLoadSession);
		msg->AddString("path", m.path.c_str());
		fSessionsMenu->AddItem(new BMenuItem(label.c_str(), msg));
	}
}


void MainWindow::_SaveSession() {
	if (!_ActiveConv()) return;
	_ActiveConv()->SaveSession();
	_RebuildSessionsMenu();
}


void MainWindow::_LoadSessionFromPath(const std::string& path) {
	if (!_ActiveConv()) return;
	_ActiveConv()->LoadSession(path);
	_ResetSidePanel();
	_UpdateTitle();
	_UpdateStatus();
}


void MainWindow::_SaveWorkspace() {
	std::string path = WorkspaceJsonPath();
	if (path.empty()) return;

	json::Value tabs;
	int32 count = fTabView ? fTabView->CountTabs() : 0;
	for (int32 i = 0; i < count; ++i) {
		BView* v = fTabView->ViewForTab(i);
		auto* conv = static_cast<ConversationView*>(v);
		if (!conv) continue;
		json::Value tab;
		std::string sp = conv->GetSessionPath();
		if (!sp.empty()) tab.set("session", sp);
		tab.set("model", conv->GetModel());
		
		// Save per-tab state
		tab.set("pinned", conv->IsPinned() ? true : false);
		tab.set("title", conv->GetSessionTitle());
		
		// Save scroll position
		if (conv->DocView()) {
			BScrollBar* vbar = conv->DocView()->ScrollBar(B_VERTICAL);
			if (vbar) {
				tab.set("scroll_pos", (double)vbar->Value());
			}
		}
		
		// Save input field state
		tab.set("input_text", conv->GetInputText());
		
		tabs.push(std::move(tab));
	}

	json::Value root;
	root.set("tabs", std::move(tabs));
	root.set("active", (double)(fTabView ? fTabView->Selection() : 0));
	root.set("model", fConfig->model);
	
	// Save global split weight
	if (fSplitView) {
		root.set("split_weight", (double)fSplitView->ItemWeightAt(0));
	}

	std::ofstream f(path, std::ios::trunc);
	if (f) f << root.serialize() << "\n";
}


bool MainWindow::_RestoreWorkspace() {
	std::string path = WorkspaceJsonPath();
	if (path.empty()) return false;

	std::ifstream f(path);
	if (!f) return false;

	std::ostringstream ss;
	ss << f.rdbuf();
	std::string content = ss.str();
	if (content.empty()) return false;

	try {
		json::Value root = json::Value::parse(content);
		const json::Value* tabs = root.find("tabs");
		if (!tabs || !tabs->is_arr() || tabs->as_arr().empty())
			return false;

		// Store scroll positions to restore after layout
		std::vector<float> scrollPositions;
		std::vector<std::string> inputTexts;

		// Restore tabs.
		for (const auto& tab : tabs->as_arr()) {
			ConversationView* conv = _CreateTab();
			if (!conv) continue;
			std::string sp = tab.str("session");
			if (!sp.empty()) conv->LoadSession(sp);
			
			// Restore per-tab state
			if (tab.find("pinned") && tab.find("pinned")->as_bool()) {
				conv->SetPinned(true);
			}
			if (tab.find("title")) {
				conv->SetSessionTitle(tab.str("title"));
			}
			if (tab.find("input_text")) {
				inputTexts.push_back(tab.str("input_text"));
			} else {
				inputTexts.push_back("");
			}
			if (tab.find("scroll_pos")) {
				scrollPositions.push_back((float)tab.find("scroll_pos")->as_num());
			} else {
				scrollPositions.push_back(0.0f);
			}
		}

		// Restore active tab.
		const json::Value* active = root.find("active");
		if (active && active->is_num() && fTabView) {
			int idx = (int)active->as_num();
			if (idx >= 0 && idx < fTabView->CountTabs())
				fTabView->Select(idx);
		}
		
		// Restore split weight
		if (root.find("split_weight") && fSplitView) {
			float weight = (float)root.find("split_weight")->as_num();
			if (weight > 0.1f && weight < 10.0f) {
				fSplitView->SetItemWeightAt(0, weight);
			}
		}

		// Restore scroll positions and input texts after layout
		// We need to wait for the views to be laid out first
		for (int32 i = 0; i < fTabView->CountTabs() && i < (int32)scrollPositions.size(); ++i) {
			BView* v = fTabView->ViewForTab(i);
			if (v) {
				auto* conv = static_cast<ConversationView*>(v);
				if (conv && i < (int32)scrollPositions.size()) {
					conv->RestoreScrollPosition(scrollPositions[i]);
				}
			}
		}
		
		// Restore input text for active tab
		if (fTabView && !inputTexts.empty()) {
			int32 activeIdx = fTabView->Selection();
			if (activeIdx >= 0 && activeIdx < (int32)inputTexts.size() && !inputTexts[activeIdx].empty()) {
				BView* v = fTabView->ViewForTab(activeIdx);
				if (v) {
					auto* conv = static_cast<ConversationView*>(v);
					if (conv) {
						// The input text will be set when the tab is focused
						conv->SetInputText(inputTexts[activeIdx]);
					}
				}
			}
		}
		
		// Update tab labels to reflect pinned state
		_RefreshTabLabels();
		
		return true;
	} catch (const std::exception&) {
		return false;
	}
}


void MainWindow::_ExportMarkdown() {
	auto* conv = _ActiveConv();
	if (!conv) return;

	std::string ws = WorkspaceRoot();
	std::string dir = ws.empty() ? HomeDir() + "/Desktop" : ws;
	std::string path = dir + "/pluto-export.md";

	std::ofstream f(path, std::ios::trunc);
	if (!f) {
		_Append("Export failed: cannot write " + path + "\n\n", rgb_color{245, 101, 101, 255});
		return;
	}

	for (const auto& m : conv->Messages()) {
		f << "## " << m.role << "\n\n";
		for (const auto& b : m.content) {
			if (b.type == ContentBlock::TEXT)
				f << b.text << "\n\n";
			else if (b.type == ContentBlock::TOOL_USE)
				f << "**[tool: " << b.tool_name << "]**\n\n";
			else if (b.type == ContentBlock::TOOL_RESULT)
				f << "```\n" << b.text.substr(0, 500) << "\n```\n\n";
		}
	}
	_Append("Exported to: " + path + "\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
}


void MainWindow::_ExportJsonl() {
	auto* conv = _ActiveConv();
	if (!conv) return;

	std::string ws = WorkspaceRoot();
	std::string dir = ws.empty() ? HomeDir() + "/Desktop" : ws;
	std::string path = dir + "/pluto-export.jsonl";

	std::ofstream f(path, std::ios::trunc);
	if (!f) {
		_Append("Export failed: cannot write " + path + "\n\n", rgb_color{245, 101, 101, 255});
		return;
	}

	for (const auto& m : conv->Messages()) {
		json::Value j;
		j.set("role", m.role);
		std::string text;
		for (const auto& b : m.content) {
			if (b.type == ContentBlock::TEXT) text += b.text;
		}
		j.set("content", text);
		f << j.serialize() << "\n";
	}
	_Append("Exported to: " + path + "\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
}


}  // namespace gui
}  // namespace pluto
