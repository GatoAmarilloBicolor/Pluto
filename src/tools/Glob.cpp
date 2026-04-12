#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <array>
#include <cstdio>
#include <string>

namespace pluto {

namespace {

std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

class GlobTool : public Tool {
public:
	std::string name() const override { return "glob"; }
	std::string description() const override {
		return "Find files matching a name glob under a path. Returns a list "
		       "of matching paths, one per line.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"pattern":{"type":"string","description":"Filename glob, e.g. *.cpp"},"path":{"type":"string","description":"Root directory, default ."}},"required":["pattern"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string p = v.str("path");
			return "glob " + v.str("pattern") + " in " + (p.empty() ? "." : p);
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string pattern, path = ".";
		try {
			auto v = json::Value::parse(input_json);
			pattern = v.str("pattern");
			std::string p = v.str("path");
			if (!p.empty()) path = p;
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (pattern.empty()) { r.is_error = true; r.output = "pattern required"; return r; }

		std::string cmd = "find " + shq(path) + " -type f -name " + shq(pattern) + " 2>&1";
		FILE* p = popen(cmd.c_str(), "r");
		if (!p) { r.is_error = true; r.output = "popen failed"; return r; }
		std::array<char, 4096> buf;
		size_t n;
		while ((n = std::fread(buf.data(), 1, buf.size(), p)) > 0) {
			r.output.append(buf.data(), n);
		}
		pclose(p);
		if (r.output.empty()) r.output = "(no matches)";
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> make_glob_tool() { return std::make_unique<GlobTool>(); }

}  // namespace pluto
