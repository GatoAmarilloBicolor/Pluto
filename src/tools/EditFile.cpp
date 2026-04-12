#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

class EditFileTool : public Tool {
public:
	std::string name() const override { return "edit_file"; }
	std::string description() const override {
		return "Replace a unique occurrence of old_string with new_string in a "
		       "file. old_string must match exactly and appear exactly once; "
		       "otherwise the edit fails and nothing is written. Include enough "
		       "surrounding context in old_string to make the match unique.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("path":{"type":"string"},)"
		       R"("old_string":{"type":"string"},)"
		       R"("new_string":{"type":"string"}},)"
		       R"("required":["path","old_string","new_string"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string out = "edit_file " + v.str("path");
			std::string old_s = v.str("old_string");
			std::string new_s = v.str("new_string");
			auto snippet = [](std::string s) {
				size_t nl = s.find('\n');
				if (nl != std::string::npos) s.resize(nl);
				if (s.size() > 80) { s.resize(77); s += "..."; }
				return s;
			};
			out += "\n  - " + snippet(old_s);
			out += "\n  + " + snippet(new_s);
			return out;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path, old_s, new_s;
		try {
			auto v = json::Value::parse(input_json);
			path  = v.str("path");
			old_s = v.str("old_string");
			new_s = v.str("new_string");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty() || old_s.empty()) {
			r.is_error = true; r.output = "path and old_string required"; return r;
		}

		std::ifstream in(path, std::ios::binary);
		if (!in) {
			r.is_error = true;
			r.output = "open read: " + std::string(std::strerror(errno));
			return r;
		}
		std::ostringstream os;
		os << in.rdbuf();
		std::string content = os.str();
		in.close();

		size_t first = content.find(old_s);
		if (first == std::string::npos) {
			r.is_error = true;
			r.output = "old_string not found in " + path;
			return r;
		}
		if (content.find(old_s, first + 1) != std::string::npos) {
			r.is_error = true;
			r.output = "old_string is not unique in " + path +
				" (at least two occurrences); widen the context";
			return r;
		}
		content.replace(first, old_s.size(), new_s);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) {
			r.is_error = true;
			r.output = "open write: " + std::string(std::strerror(errno));
			return r;
		}
		out.write(content.data(), content.size());
		if (!out) { r.is_error = true; r.output = "write failed"; return r; }
		r.output = "edited " + path;
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> make_edit_file_tool() {
	return std::make_unique<EditFileTool>();
}

}  // namespace pluto
