// Haiku-native BFS tools: extended attributes and BQuery.
// Requires libbe.

#include "BfsTools.h"

#ifdef __HAIKU__
#include <Directory.h>
#include <Entry.h>
#include <Handler.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <NodeMonitor.h>
#include <OS.h>
#include <Path.h>
#include <Volume.h>
#include <VolumeRoster.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../json.h"
#include "../tools/Tool.h"
#include "../workspace.h"

namespace pluto {

std::string format_type_code(uint32_t type) {
	char t[5] = {
		static_cast<char>((type >> 24) & 0xff),
		static_cast<char>((type >> 16) & 0xff),
		static_cast<char>((type >>  8) & 0xff),
		static_cast<char>((type      ) & 0xff),
		0
	};
	// Fall back to hex if not printable ASCII.
	for (int i = 0; i < 4; ++i) {
		if (t[i] < 0x20 || t[i] > 0x7e) {
			char buf[16];
			std::snprintf(buf, sizeof(buf), "0x%08x", (unsigned)type);
			return buf;
		}
	}
	return std::string("'") + t + "'";
}

std::string format_attr_value(uint32_t type, const void* data, ssize_t size) {
#ifdef __HAIKU__
	switch (type) {
		case B_STRING_TYPE: {
			const char* s = static_cast<const char*>(data);
			ssize_t len = size;
			if (len > 0 && s[len - 1] == 0) --len;
			return std::string(s, len);
		}
		case B_INT32_TYPE: {
			int32 v = 0;
			if ((size_t)size >= sizeof(v)) std::memcpy(&v, data, sizeof(v));
			return std::to_string(v);
		}
		case B_INT64_TYPE: {
			int64 v = 0;
			if ((size_t)size >= sizeof(v)) std::memcpy(&v, data, sizeof(v));
			return std::to_string((long long)v);
		}
		case B_FLOAT_TYPE: {
			float v = 0;
			if ((size_t)size >= sizeof(v)) std::memcpy(&v, data, sizeof(v));
			return std::to_string(v);
		}
		case B_DOUBLE_TYPE: {
			double v = 0;
			if ((size_t)size >= sizeof(v)) std::memcpy(&v, data, sizeof(v));
			return std::to_string(v);
		}
		case B_BOOL_TYPE: {
			bool v = size > 0 && *static_cast<const char*>(data) != 0;
			return v ? "true" : "false";
		}
		default: {
			char buf[96];
			std::snprintf(buf, sizeof(buf),
				"(binary %ld bytes, type %s)", (long)size,
				format_type_code(type).c_str());
			return buf;
		}
	}
#else
	(void)type; (void)data; (void)size;
	return "(binary attr)";
#endif
}

// ---------------- read_attr ----------------

#ifdef __HAIKU__

class ReadAttrTool : public Tool {
public:
	std::string name() const override { return "read_attr"; }
	std::string description() const override {
		return "Read a BFS extended attribute from a file or directory. "
		       "Returns the attribute value as text (typed attributes are "
		       "formatted according to their type).";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("path":{"type":"string","description":"Path to the file"},)"
		       R"("name":{"type":"string","description":"Attribute name"})"
		       R"(},)"
		       R"("required":["path","name"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "read_attr " + v.str("name") + " " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path, name;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
			name = v.str("name");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty() || name.empty()) {
			r.is_error = true; r.output = "path and name required"; return r;
		}
		if (!WorkspaceAllows(path)) {
			LogAccess(path, "read_attr", false);
			r.is_error = true;
			r.output = path + " is outside workspace";
			return r;
		}
		LogAccess(path, "read_attr", true);

		BNode node(path.c_str());
		if (node.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "cannot open node: " + path;
			return r;
		}
		attr_info info;
		if (node.GetAttrInfo(name.c_str(), &info) != B_OK) {
			r.is_error = true;
			r.output = "attribute not found: " + name;
			return r;
		}
		std::vector<char> buf(info.size > 0 ? info.size : 1);
		ssize_t n = node.ReadAttr(name.c_str(), info.type, 0,
			buf.data(), info.size);
		if (n < 0) {
			r.is_error = true;
			r.output = "read failed";
			return r;
		}
		r.output = format_attr_value(info.type, buf.data(), n);
		return r;
	}
};

// ---------------- write_attr ----------------

class WriteAttrTool : public Tool {
public:
	std::string name() const override { return "write_attr"; }
	std::string description() const override {
		return "Write a BFS extended attribute on a file or directory. "
		       "Type defaults to string; other types: int32, int64, float, "
		       "double, bool.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("path":{"type":"string"},)"
		       R"("name":{"type":"string"},)"
		       R"("value":{"type":"string","description":"Value serialized as text"},)"
		       R"("type":{"type":"string","description":"string|int32|int64|float|double|bool"})"
		       R"(},)"
		       R"("required":["path","name","value"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "write_attr " + v.str("name") + " " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path, name, value, type_str = "string";
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
			name = v.str("name");
			value = v.str("value");
			std::string t = v.str("type");
			if (!t.empty()) type_str = t;
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty() || name.empty()) {
			r.is_error = true; r.output = "path and name required"; return r;
		}
		if (!WorkspaceAllows(path)) {
			LogAccess(path, "write_attr", false);
			r.is_error = true;
			r.output = path + " is outside workspace";
			return r;
		}
		LogAccess(path, "write_attr", true);

		BNode node(path.c_str());
		if (node.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "cannot open node: " + path;
			return r;
		}

		ssize_t written = -1;
		if (type_str == "string") {
			written = node.WriteAttr(name.c_str(), B_STRING_TYPE, 0,
				value.c_str(), value.size() + 1);
		} else if (type_str == "int32") {
			int32 v = std::atoi(value.c_str());
			written = node.WriteAttr(name.c_str(), B_INT32_TYPE, 0,
				&v, sizeof(v));
		} else if (type_str == "int64") {
			int64 v = std::atoll(value.c_str());
			written = node.WriteAttr(name.c_str(), B_INT64_TYPE, 0,
				&v, sizeof(v));
		} else if (type_str == "float") {
			float v = static_cast<float>(std::atof(value.c_str()));
			written = node.WriteAttr(name.c_str(), B_FLOAT_TYPE, 0,
				&v, sizeof(v));
		} else if (type_str == "double") {
			double v = std::atof(value.c_str());
			written = node.WriteAttr(name.c_str(), B_DOUBLE_TYPE, 0,
				&v, sizeof(v));
		} else if (type_str == "bool") {
			bool v = (value == "true" || value == "1" || value == "yes");
			written = node.WriteAttr(name.c_str(), B_BOOL_TYPE, 0,
				&v, sizeof(v));
		} else {
			r.is_error = true;
			r.output = "unknown type: " + type_str;
			return r;
		}

		if (written < 0) {
			r.is_error = true;
			r.output = "write failed";
			return r;
		}
		r.output = "wrote " + name + " = " + value + " (" + type_str + ")";
		return r;
	}
};

// ---------------- list_attrs ----------------

class ListAttrsTool : public Tool {
public:
	std::string name() const override { return "list_attrs"; }
	std::string description() const override {
		return "List all BFS extended attributes on a file or directory, "
		       "one per line as: name (type, size).";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{"path":{"type":"string"}},)"
		       R"("required":["path"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "list_attrs " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty()) {
			r.is_error = true; r.output = "path required"; return r;
		}
		if (!WorkspaceAllows(path)) {
			LogAccess(path, "list_attrs", false);
			r.is_error = true;
			r.output = path + " is outside workspace";
			return r;
		}
		LogAccess(path, "list_attrs", true);

		BNode node(path.c_str());
		if (node.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "cannot open node: " + path;
			return r;
		}

		char attr_name[B_ATTR_NAME_LENGTH];
		node.RewindAttrs();
		std::string out;
		int count = 0;
		while (node.GetNextAttrName(attr_name) == B_OK) {
			attr_info info;
			if (node.GetAttrInfo(attr_name, &info) != B_OK) continue;
			out += attr_name;
			out += "  (";
			out += format_type_code(info.type);
			out += ", ";
			out += std::to_string((long)info.size);
			out += " bytes)\n";
			++count;
		}
		if (count == 0) out = "(no attributes)\n";
		r.output = std::move(out);
		return r;
	}
};

// ---------------- remove_attr ----------------

class RemoveAttrTool : public Tool {
public:
	std::string name() const override { return "remove_attr"; }
	std::string description() const override {
		return "Remove a BFS extended attribute from a file or directory.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("path":{"type":"string"},)"
		       R"("name":{"type":"string"})"
		       R"(},)"
		       R"("required":["path","name"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "remove_attr " + v.str("name") + " " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path, name;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
			name = v.str("name");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty() || name.empty()) {
			r.is_error = true; r.output = "path and name required"; return r;
		}
		if (!WorkspaceAllows(path)) {
			LogAccess(path, "remove_attr", false);
			r.is_error = true;
			r.output = path + " is outside workspace";
			return r;
		}
		LogAccess(path, "remove_attr", true);
		BNode node(path.c_str());
		if (node.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "cannot open node: " + path;
			return r;
		}
		status_t s = node.RemoveAttr(name.c_str());
		if (s != B_OK) {
			r.is_error = true;
			r.output = "remove failed";
			return r;
		}
		r.output = "removed " + name;
		return r;
	}
};

// ---------------- query_fs ----------------

class QueryFsTool : public Tool {
public:
	std::string name() const override { return "query_fs"; }
	std::string description() const override {
		return "Run a Haiku BQuery predicate against a BFS volume and "
		       "return matching paths, one per line. Default volume is "
		       "the boot volume.\n"
		       "Syntax: attribute op value [&& / ||]\n"
		       "  name == \"*.cpp\"          — glob match on filename\n"
		       "  size > 1024              — numeric comparison\n"
		       "  BEOS:TYPE == \"text/x-source-code\"\n"
		       "  last_modified > 1777000000  — Unix timestamp (seconds)\n"
		       "  name == \"*.h\" && size < 4096\n"
		       "Dates must be Unix timestamps (use bash: date -d '24 hours ago' +%s). "
		       "There is no now() function — compute the timestamp before calling.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"predicate":{"type":"string","description":"BQuery predicate"},"volume":{"type":"string","description":"Volume name, default boot volume"},"limit":{"type":"integer","description":"Max results, default 100"}},"required":["predicate"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string p = v.str("predicate");
			if (p.size() > 60) p = p.substr(0, 60) + "…";
			return "query_fs " + p;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string predicate, volume_name;
		int limit = 100;
		try {
			auto v = json::Value::parse(input_json);
			predicate = v.str("predicate");
			volume_name = v.str("volume");
			if (const auto* l = v.find("limit"); l && l->is_num())
				limit = static_cast<int>(l->as_num());
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (predicate.empty()) {
			r.is_error = true; r.output = "predicate required"; return r;
		}

		BVolume volume;
		if (volume_name.empty()) {
			BVolumeRoster().GetBootVolume(&volume);
		} else {
			BVolumeRoster roster;
			BVolume v;
			char buf[B_FILE_NAME_LENGTH];
			bool found = false;
			while (roster.GetNextVolume(&v) == B_OK) {
				if (v.GetName(buf) == B_OK && volume_name == buf) {
					volume = v;
					found = true;
					break;
				}
			}
			if (!found) {
				r.is_error = true;
				r.output = "volume not found: " + volume_name;
				return r;
			}
		}
		if (volume.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "invalid volume";
			return r;
		}

		BQuery query;
		query.SetVolume(&volume);
		if (query.SetPredicate(predicate.c_str()) != B_OK) {
			r.is_error = true;
			r.output = "invalid predicate: " + predicate;
			return r;
		}
		status_t fs = query.Fetch();
		if (fs != B_OK) {
			r.is_error = true;
			r.output = "query fetch failed";
			return r;
		}

		std::string out;
		BEntry entry;
		int count = 0;
		while (query.GetNextEntry(&entry) == B_OK) {
			if (count >= limit) {
				out += "… (limit ";
				out += std::to_string(limit);
				out += " reached)\n";
				break;
			}
			BPath p;
			if (entry.GetPath(&p) == B_OK) {
				out += p.Path();
				out += "\n";
				++count;
			}
		}
		if (out.empty()) out = "(no matches)\n";
		r.output = std::move(out);
		return r;
	}
};

// ---------------- live_query ----------------

// BLooper that captures B_QUERY_UPDATE messages from a live BQuery and
// records them as add/remove events. Owned by the tool, runs in its own
// thread (BLooper::Run). Drained at the end of the watch window.
class LiveQueryListener : public BLooper {
public:
	struct Event {
		int32       opcode;       // B_ENTRY_CREATED / B_ENTRY_REMOVED
		std::string name;
		dev_t       device;
		ino_t       directory;
		ino_t       node;
	};

	LiveQueryListener() : BLooper("pluto-live-query") {
		Run();
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what != B_QUERY_UPDATE) {
			BLooper::MessageReceived(msg);
			return;
		}
		Event ev;
		msg->FindInt32("opcode", &ev.opcode);
		const char* n = nullptr;
		if (msg->FindString("name", &n) == B_OK && n) ev.name = n;
		int32 dev = 0;
		msg->FindInt32("device", &dev);
		ev.device = static_cast<dev_t>(dev);
		msg->FindInt64("directory", &ev.directory);
		msg->FindInt64("node", &ev.node);

		std::lock_guard<std::mutex> lk(fMu);
		fEvents.push_back(std::move(ev));
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

class LiveQueryTool : public Tool {
public:
	std::string name() const override { return "live_query"; }
	std::string description() const override {
		return "Run a Haiku BQuery in live mode: returns the initial matches "
		       "and watches for add/remove events for `duration_ms` "
		       "milliseconds (default 5000, max 60000). Use this to detect "
		       "files appearing or disappearing while you wait — e.g. "
		       "monitor a build output, watch downloads, react to incoming "
		       "mail. Predicate syntax is the same as query_fs.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"predicate":{"type":"string","description":"BQuery predicate"},"volume":{"type":"string","description":"Volume name, default boot"},"duration_ms":{"type":"integer","description":"How long to watch, default 5000, max 60000"}},"required":["predicate"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string p = v.str("predicate");
			if (p.size() > 50) p = p.substr(0, 50) + "…";
			return "live_query " + p;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string predicate, volume_name;
		int duration_ms = 5000;
		try {
			auto v = json::Value::parse(input_json);
			predicate = v.str("predicate");
			volume_name = v.str("volume");
			if (const auto* d = v.find("duration_ms"); d && d->is_num())
				duration_ms = static_cast<int>(d->as_num());
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (predicate.empty()) {
			r.is_error = true; r.output = "predicate required"; return r;
		}
		if (duration_ms < 100)   duration_ms = 100;
		if (duration_ms > 60000) duration_ms = 60000;

		BVolume volume;
		if (volume_name.empty()) {
			BVolumeRoster().GetBootVolume(&volume);
		} else {
			BVolumeRoster roster;
			BVolume v;
			char buf[B_FILE_NAME_LENGTH];
			bool found = false;
			while (roster.GetNextVolume(&v) == B_OK) {
				if (v.GetName(buf) == B_OK && volume_name == buf) {
					volume = v; found = true; break;
				}
			}
			if (!found) {
				r.is_error = true;
				r.output = "volume not found: " + volume_name;
				return r;
			}
		}
		if (volume.InitCheck() != B_OK) {
			r.is_error = true;
			r.output = "invalid volume";
			return r;
		}

		LiveQueryListener* listener = new LiveQueryListener();

		BQuery query;
		query.SetVolume(&volume);
		query.SetPredicate(predicate.c_str());
		query.SetTarget(BMessenger(listener));
		if (query.Fetch() != B_OK) {
			listener->Lock();
			listener->Quit();
			r.is_error = true;
			r.output = "query fetch failed";
			return r;
		}

		// Drain initial matches synchronously.
		std::string out = "initial matches:\n";
		BEntry entry;
		int initial = 0;
		while (query.GetNextEntry(&entry) == B_OK) {
			BPath p;
			if (entry.GetPath(&p) == B_OK && p.Path()) {
				out += "  ";
				out += p.Path();
				out += "\n";
				++initial;
			}
		}
		if (initial == 0) out += "  (none)\n";

		// Watch for live events.
		snooze(static_cast<bigtime_t>(duration_ms) * 1000LL);

		// Stop the query before draining.
		query.Clear();

		auto events = listener->drain();
		out += "\nlive events (";
		out += std::to_string(duration_ms);
		out += " ms window): ";
		out += std::to_string(events.size());
		out += "\n";
		int shown = 0;
		for (const auto& ev : events) {
			if (shown++ >= 100) {
				out += "  … (truncated)\n";
				break;
			}
			const char* op = ev.opcode == B_ENTRY_CREATED ? "+" :
			                 ev.opcode == B_ENTRY_REMOVED ? "-" : "?";
			out += "  ";
			out += op;
			out += " ";
			// Try to resolve to full path
			entry_ref ref;
			ref.device = ev.device;
			ref.directory = ev.directory;
			ref.set_name(ev.name.c_str());
			BEntry e(&ref);
			BPath p;
			if (e.InitCheck() == B_OK && e.GetPath(&p) == B_OK && p.Path())
				out += p.Path();
			else
				out += ev.name;
			out += "\n";
		}

		listener->Lock();
		listener->Quit();

		r.output = std::move(out);
		return r;
	}
};

std::unique_ptr<Tool> MakeReadAttrTool()    { return std::make_unique<ReadAttrTool>(); }
std::unique_ptr<Tool> MakeWriteAttrTool()   { return std::make_unique<WriteAttrTool>(); }
std::unique_ptr<Tool> MakeListAttrsTool()   { return std::make_unique<ListAttrsTool>(); }
std::unique_ptr<Tool> MakeRemoveAttrTool()  { return std::make_unique<RemoveAttrTool>(); }
std::unique_ptr<Tool> MakeQueryFsTool()     { return std::make_unique<QueryFsTool>(); }
std::unique_ptr<Tool> MakeLiveQueryTool()   { return std::make_unique<LiveQueryTool>(); }

#else  // __HAIKU__

// Stubs for non-Haiku builds.
std::unique_ptr<Tool> MakeReadAttrTool()    { return nullptr; }
std::unique_ptr<Tool> MakeWriteAttrTool()   { return nullptr; }
std::unique_ptr<Tool> MakeListAttrsTool()   { return nullptr; }
std::unique_ptr<Tool> MakeRemoveAttrTool()  { return nullptr; }
std::unique_ptr<Tool> MakeQueryFsTool()     { return nullptr; }
std::unique_ptr<Tool> MakeLiveQueryTool()   { return nullptr; }

#endif  // __HAIKU__

}  // namespace pluto
