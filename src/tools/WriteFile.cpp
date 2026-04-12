#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

namespace pluto {

namespace {

class WriteFileTool : public Tool {
public:
	std::string name() const override { return "write_file"; }
	std::string description() const override {
		return "Write content to a file, creating or overwriting it. For "
		       "modifying existing files, prefer edit_file.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("path":{"type":"string"},)"
		       R"("content":{"type":"string"}},)"
		       R"("required":["path","content"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "write_file " + v.str("path") + " (" +
				std::to_string(v.str("content").size()) + " bytes)";
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string path, content;
		try {
			auto v = json::Value::parse(input_json);
			path = v.str("path");
			content = v.str("content");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (path.empty()) { r.is_error = true; r.output = "path required"; return r; }
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f) {
			r.is_error = true;
			r.output = "open: " + std::string(std::strerror(errno));
			return r;
		}
		f.write(content.data(), content.size());
		if (!f) { r.is_error = true; r.output = "write failed"; return r; }
		r.output = "wrote " + std::to_string(content.size()) + " bytes to " + path;
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> make_write_file_tool() {
	return std::make_unique<WriteFileTool>();
}

}  // namespace pluto
