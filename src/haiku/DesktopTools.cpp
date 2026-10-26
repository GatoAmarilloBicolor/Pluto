// Native Haiku desktop integration tools: clipboard and node monitor.
// Both rely on libbe.

#include "DesktopTools.h"

#ifdef __HAIKU__
#include <Clipboard.h>
#include <InterfaceDefs.h>
#include <Roster.h>
#include <Size.h>
#include <Directory.h>
#include <Entry.h>
#include <Node.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <Point.h>
#include <PropertyInfo.h>
#include <Rect.h>
#include <Statable.h>
#include <Looper.h>
#include <Messenger.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../json.h"
#include "../workspace.h"

namespace pluto {

#ifdef __HAIKU__

// ---------------- clipboard ----------------

class ClipboardTool : public Tool {
public:
	std::string name() const override { return "clipboard"; }
	std::string description() const override {
		return "Read or write the Haiku system clipboard. With "
		       "`action=get` returns the current text content. With "
		       "`action=set` writes the given `text`. Use this to react "
		       "to what the user just copied or to surface a result for "
		       "them to paste elsewhere.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"action":{"type":"string","description":"get or set"},"text":{"type":"string","description":"Text to set when action=set"}},"required":["action"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string a = v.str("action");
			return "clipboard " + a;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string action, text;
		try {
			auto v = json::Value::parse(input_json);
			action = v.str("action");
			text   = v.str("text");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}

		BClipboard clipboard("system");
		if (!clipboard.Lock()) {
			r.is_error = true;
			r.output = "cannot lock clipboard";
			return r;
		}

		if (action == "get") {
			BMessage* data = clipboard.Data();
			if (!data) {
				clipboard.Unlock();
				r.output = "(empty)";
				return r;
			}
			const char* s = nullptr;
			ssize_t len = 0;
			if (data->FindData("text/plain", B_MIME_TYPE,
			                   reinterpret_cast<const void**>(&s), &len) == B_OK
			    && s != nullptr && len > 0) {
				r.output = std::string(s, static_cast<size_t>(len));
			} else {
				r.output = "(no text content)";
			}
			clipboard.Unlock();
			return r;
		}

		if (action == "set") {
			clipboard.Clear();
			BMessage* data = clipboard.Data();
			if (!data) {
				clipboard.Unlock();
				r.is_error = true;
				r.output = "no clipboard data slot";
				return r;
			}
			data->AddData("text/plain", B_MIME_TYPE,
				text.data(), text.size());
			clipboard.Commit();
			clipboard.Unlock();
			r.output = "set " + std::to_string(text.size()) + " bytes";
			return r;
		}

		clipboard.Unlock();
		r.is_error = true;
		r.output = "unknown action: " + action + " (use get or set)";
		return r;
	}
};

// ---------------- node_monitor ----------------

class NodeWatcher : public BLooper {
public:
	struct Event {
		int32       opcode;
		std::string name;
	};

	NodeWatcher() : BLooper("pluto-node-watch") { Run(); }

	void MessageReceived(BMessage* msg) override {
		if (msg->what != B_NODE_MONITOR) {
			BLooper::MessageReceived(msg);
			return;
		}
		int32 opcode = 0;
		msg->FindInt32("opcode", &opcode);
		const char* name = nullptr;
		msg->FindString("name", &name);

		std::lock_guard<std::mutex> lk(fMu);
		fEvents.push_back({opcode, name ? name : ""});
	}

	std::vector<Event> drain() {
		std::lock_guard<std::mutex> lk(fMu);
		std::vector<Event> out;
		out.swap(fEvents);
		return out;
	}

private:
	std::mutex         fMu;
	std::vector<Event> fEvents;
};

class NodeMonitorTool : public Tool {
public:
	std::string name() const override { return "node_monitor"; }
	std::string description() const override {
		return "Watch a directory via BNodeMonitor for create/remove/change "
		       "events for `duration_ms` milliseconds (default 5000, "
		       "max 60000). Returns the list of events. Use this to "
		       "build watch-and-react workflows: continuous linting, "
		       "auto-format on save, react to downloads, etc.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"path":{"type":"string","description":"Directory to watch"},"duration_ms":{"type":"integer","description":"How long to watch, default 5000, max 60000"}},"required":["path"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "node_monitor " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path;
		int duration_ms = 5000;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
			if (const auto* d = v.find("duration_ms"); d && d->is_num())
				duration_ms = static_cast<int>(d->as_num());
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty()) {
			r.is_error = true; r.output = "path required"; return r;
		}
		if (!WorkspaceAllows(path)) {
			r.is_error = true;
			r.output = path + " is outside workspace";
			return r;
		}
		if (duration_ms < 100)   duration_ms = 100;
		if (duration_ms > 60000) duration_ms = 60000;

		BNode node(path.c_str());
		if (node.InitCheck() != B_OK || !node.IsDirectory()) {
			r.is_error = true;
			r.output = "not a directory: " + path;
			return r;
		}

		node_ref nref;
		if (node.GetNodeRef(&nref) != B_OK) {
			r.is_error = true;
			r.output = "GetNodeRef failed";
			return r;
		}

		NodeWatcher* watcher = new NodeWatcher();
		BMessenger target(watcher);

		uint32_t flags = B_WATCH_DIRECTORY | B_WATCH_NAME | B_WATCH_STAT;
		if (watch_node(&nref, flags, target) != B_OK) {
			watcher->Lock();
			watcher->Quit();
			r.is_error = true;
			r.output = "watch_node failed";
			return r;
		}

		snooze(static_cast<bigtime_t>(duration_ms) * 1000LL);

		watch_node(&nref, B_STOP_WATCHING, target);
		auto events = watcher->drain();

		std::string out;
		out  = "watched ";
		out += path;
		out += " for ";
		out += std::to_string(duration_ms);
		out += " ms — ";
		out += std::to_string(events.size());
		out += " events\n";
		int shown = 0;
		for (const auto& ev : events) {
			if (shown++ >= 100) { out += "  …\n"; break; }
			const char* op = "?";
			switch (ev.opcode) {
				case B_ENTRY_CREATED:  op = "+"; break;
				case B_ENTRY_REMOVED:  op = "-"; break;
				case B_ENTRY_MOVED:    op = "→"; break;
				case B_STAT_CHANGED:   op = "*"; break;
				case B_ATTR_CHANGED:   op = "@"; break;
			}
			out += "  ";
			out += op;
			out += " ";
			out += ev.name.empty() ? "(unnamed)" : ev.name;
			out += "\n";
		}

		watcher->Lock();
		watcher->Quit();

		r.output = std::move(out);
		return r;
	}
};

// ---------------- desktop_script ----------------
//
// Sends a BMessage scripting command to a running Haiku application.
// Uses BRoster to find the app and BMessenger to deliver the message.
//

class DesktopScriptTool : public Tool {
public:
	std::string name() const override { return "desktop_script"; }
	std::string description() const override {
		return "Send a BMessage scripting command to a running Haiku "
		       "application. This allows querying and manipulating "
		       "windows, views, and properties of any app that "
		       "implements the standard Be scripting protocol.\n"
		       "\n"
		       "Actions: get, set, count, create, delete, execute, suites.\n"
		       "The 'suites' action lists all properties the target "
		       "supports — use it first to discover what you can do.\n"
		       "\n"
		       "Examples:\n"
		       "  app=application/x-vnd.Be-TRAK action=suites\n"
		       "  app=application/x-vnd.Be-TRAK action=get property=Window "
		       "name=Desktop\n"
		       "  app=application/x-vnd.Haiku-Terminal action=get "
		       "property=Title specifier=index index=0\n"
		       "  app=application/x-vnd.Be-TRAK action=count "
		       "property=Window";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"app":{"type":"string","description":"Application MIME signature, e.g. application/x-vnd.Be-TRAK"},"action":{"type":"string","description":"get, set, count, create, delete, execute, or suites"},"property":{"type":"string","description":"Property name, e.g. Window, Title, Frame"},"name":{"type":"string","description":"Name specifier for B_NAME_SPECIFIER"},"index":{"type":"integer","description":"Index specifier for B_INDEX_SPECIFIER"},"value":{"type":"string","description":"Value to set (for set action)"}},"required":["app","action"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string p = "desktop_script " + v.str("action");
			std::string prop = v.str("property");
			if (!prop.empty()) p += " " + prop;
			p += " on " + v.str("app");
			return p;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string app_sig, action, property, spec_name, value_str;
		int spec_index = -1;
		try {
			auto v = json::Value::parse(input_json);
			app_sig   = v.str("app");
			action    = v.str("action");
			property  = v.str("property");
			spec_name = v.str("name");
			value_str = v.str("value");
			if (const auto* idx = v.find("index");
			    idx && idx->is_num())
				spec_index = static_cast<int>(idx->as_num());
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (app_sig.empty() || action.empty()) {
			r.is_error = true;
			r.output = "app and action required";
			return r;
		}

		// Resolve action → what code
		uint32_t what = 0;
		if      (action == "get")     what = B_GET_PROPERTY;
		else if (action == "set")     what = B_SET_PROPERTY;
		else if (action == "count")   what = B_COUNT_PROPERTIES;
		else if (action == "create")  what = B_CREATE_PROPERTY;
		else if (action == "delete")  what = B_DELETE_PROPERTY;
		else if (action == "execute") what = B_EXECUTE_PROPERTY;
		else if (action == "suites")  what = B_GET_PROPERTY;
		else {
			r.is_error = true;
			r.output = "unknown action: " + action
				+ " (use get/set/count/create/delete/execute/suites)";
			return r;
		}

		// Find the running application
		BMessenger target(app_sig.c_str());
		if (!target.IsValid()) {
			r.is_error = true;
			r.output = "app not running or invalid signature: " + app_sig;
			return r;
		}

		// Build scripting message
		BMessage msg(what);
		std::string prop = (action == "suites") ? "Suites" : property;
		if (prop.empty() && action != "suites") {
			r.is_error = true;
			r.output = "property required for action " + action;
			return r;
		}

		if (!prop.empty()) {
			if (!spec_name.empty()) {
				msg.AddSpecifier(prop.c_str(), spec_name.c_str());
			} else if (spec_index >= 0) {
				msg.AddSpecifier(prop.c_str(), spec_index);
			} else {
				msg.AddSpecifier(prop.c_str());
			}
		}

		// For set, add the value as "data"
		if (action == "set" && !value_str.empty()) {
			msg.AddString("data", value_str.c_str());
		}

		// Send and wait for reply
		BMessage reply;
		status_t st = target.SendMessage(&msg, &reply, 2000000, 2000000);
		if (st != B_OK) {
			r.is_error = true;
			r.output = "send failed: " + std::string(strerror(st));
			return r;
		}

		// Check for error in reply
		int32 error = B_OK;
		if (reply.FindInt32("error", &error) == B_OK && error != B_OK) {
			r.is_error = true;
			const char* emsg = nullptr;
			if (reply.FindString("message", &emsg) == B_OK && emsg)
				r.output = std::string("error: ") + emsg;
			else
				r.output = "error: " + std::string(strerror(error));
			return r;
		}

		// Format reply as readable text
		r.output = format_reply(reply);
		return r;
	}

private:
	static std::string format_reply(const BMessage& reply, int depth = 0) {
		if (depth > 8) return "(recursion limit reached)\n";
		std::string out;
		char* name;
		type_code type;
		int32 count;
		std::string indent(depth * 2, ' ');
		for (int32 i = 0;
		     reply.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK;
		     ++i) {
			if (std::string(name) == "when"
			 || std::string(name) == "error") continue;
			for (int32 j = 0; j < count; ++j) {
				out += indent;
				out += name;
				if (count > 1) {
					out += "[";
					out += std::to_string(j);
					out += "]";
				}
				out += " = ";
				switch (type) {
					case B_STRING_TYPE: {
						const char* s = nullptr;
						if (reply.FindString(name, j, &s) == B_OK && s)
							out += s;
						break;
					}
					case B_INT32_TYPE: {
						int32 v;
						if (reply.FindInt32(name, j, &v) == B_OK)
							out += std::to_string(v);
						break;
					}
					case B_INT64_TYPE: {
						int64 v;
						if (reply.FindInt64(name, j, &v) == B_OK)
							out += std::to_string((long long)v);
						break;
					}
					case B_FLOAT_TYPE: {
						float v;
						if (reply.FindFloat(name, j, &v) == B_OK)
							out += std::to_string(v);
						break;
					}
					case B_DOUBLE_TYPE: {
						double v;
						if (reply.FindDouble(name, j, &v) == B_OK)
							out += std::to_string(v);
						break;
					}
					case B_BOOL_TYPE: {
						bool v;
						if (reply.FindBool(name, j, &v) == B_OK)
							out += v ? "true" : "false";
						break;
					}
					case B_RECT_TYPE: {
						BRect rect;
						if (reply.FindRect(name, j, &rect) == B_OK) {
							char buf[80];
							std::snprintf(buf, sizeof(buf),
								"BRect(%.0f, %.0f, %.0f, %.0f)",
								rect.left, rect.top,
								rect.right, rect.bottom);
							out += buf;
						}
						break;
					}
					case B_POINT_TYPE: {
						BPoint pt;
						if (reply.FindPoint(name, j, &pt) == B_OK) {
							char buf[40];
							std::snprintf(buf, sizeof(buf),
								"BPoint(%.0f, %.0f)", pt.x, pt.y);
							out += buf;
						}
						break;
					}
					case B_RGB_COLOR_TYPE: {
						rgb_color c;
						if (reply.FindData(name, B_RGB_COLOR_TYPE, j,
							(const void**)&c, nullptr) != B_OK) {
							// Fallback: read raw 4 bytes.
							const void* data = nullptr;
							ssize_t sz = 0;
							if (reply.FindData(name, B_RGB_COLOR_TYPE, j,
								&data, &sz) == B_OK && sz >= 4) {
								const uint8* p = static_cast<const uint8*>(data);
								char buf[32];
								std::snprintf(buf, sizeof(buf),
									"rgb_color(%d, %d, %d, %d)",
									p[0], p[1], p[2], p[3]);
								out += buf;
							}
						} else {
							char buf[40];
							std::snprintf(buf, sizeof(buf),
								"rgb_color(%d, %d, %d, %d)",
								c.red, c.green, c.blue, c.alpha);
							out += buf;
						}
						break;
					}
					case B_SIZE_TYPE: {
						BSize sz;
						if (reply.FindSize(name, j, &sz) == B_OK) {
							char buf[40];
							std::snprintf(buf, sizeof(buf),
								"BSize(%.0f, %.0f)", sz.width, sz.height);
							out += buf;
						}
						break;
					}
					case B_MESSAGE_TYPE: {
						BMessage sub;
						if (reply.FindMessage(name, j, &sub) == B_OK) {
							out += "{\n";
							out += format_reply(sub, depth + 1);
							out += indent + "}";
						}
						break;
					}
					default: {
						char tc[5] = {
							(char)((type >> 24) & 0xff),
							(char)((type >> 16) & 0xff),
							(char)((type >> 8) & 0xff),
							(char)(type & 0xff), 0
						};
						out += "(";
						out += tc;
						out += ")";
						break;
					}
				}
				out += "\n";
			}
		}
		if (out.empty()) out = "(empty reply)\n";
		return out;
	}
};

// ---- Launch App ----

class LaunchAppTool : public Tool {
public:
	std::string name() const override { return "launch_app"; }
	std::string description() const override {
		return "Launch an application by its MIME signature (e.g. "
			"application/x-vnd.Haiku-WebPositive). If the app is already "
			"running, it is brought to front.";
	}
	std::string input_schema_json() const override {
		return R"JSON({
			"type": "object",
			"properties": {
				"sig": {
					"type": "string",
					"description": "Application MIME signature, e.g. application/x-vnd.Haiku-WebPositive"
				}
			},
			"required": ["sig"]
		})JSON";
	}
	std::string preview(const std::string& input) const override {
		return "launch_app " + input;
	}
	ToolResult execute(const std::string& input_json) override {
		json::Value args;
		try { args = json::Value::parse(input_json); }
		catch (const std::exception&) { return {"error: invalid JSON", true}; }
		std::string sig = args.str("sig");
		if (sig.empty()) return {"error: 'sig' is required", true};

		// Check if already running.
		app_info info;
		if (be_roster->GetAppInfo(sig.c_str(), &info) == B_OK) {
			// Already running — activate it.
			be_roster->ActivateApp(info.team);
			return {"application already running, brought to front: " + sig, false};
		}

		status_t err = be_roster->Launch(sig.c_str());
		if (err == B_OK || err == B_ALREADY_RUNNING)
			return {"launched: " + sig, false};

		char buf[128];
		std::snprintf(buf, sizeof(buf), "error: failed to launch %s (status %d)",
			sig.c_str(), (int)err);
		return {buf, true};
	}
};

std::unique_ptr<Tool> MakeClipboardTool()      { return std::make_unique<ClipboardTool>(); }
std::unique_ptr<Tool> MakeNodeMonitorTool()   { return std::make_unique<NodeMonitorTool>(); }
std::unique_ptr<Tool> MakeDesktopScriptTool() { return std::make_unique<DesktopScriptTool>(); }
std::unique_ptr<Tool> MakeLaunchAppTool()     { return std::make_unique<LaunchAppTool>(); }

#else  // __HAIKU__

// Stubs for non-Haiku builds.
std::unique_ptr<Tool> MakeClipboardTool()      { return nullptr; }
std::unique_ptr<Tool> MakeNodeMonitorTool()   { return nullptr; }
std::unique_ptr<Tool> MakeDesktopScriptTool() { return nullptr; }
std::unique_ptr<Tool> MakeLaunchAppTool()     { return nullptr; }

#endif  // __HAIKU__

}  // namespace pluto
