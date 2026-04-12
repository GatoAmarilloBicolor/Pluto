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

class ReadFileTool : public Tool {
public:
	std::string name() const override { return "read_file"; }
	std::string description() const override {
		return "Read a file and return its contents as text. Use this before "
		       "editing a file so you know its exact current content.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{"path":{"type":"string","description":"Absolute or relative path"}},)"
		       R"("required":["path"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "read_file " + v.str("path");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = std::string("args: ") + e.what(); return r;
		}
		if (path.empty()) { r.is_error = true; r.output = "path required"; return r; }
		std::ifstream f(path, std::ios::binary);
		if (!f) {
			r.is_error = true;
			r.output = "cannot open " + path + ": " + std::strerror(errno);
			return r;
		}
		std::ostringstream os;
		os << f.rdbuf();
		r.output = os.str();
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> make_read_file_tool() {
	return std::make_unique<ReadFileTool>();
}

}  // namespace pluto
