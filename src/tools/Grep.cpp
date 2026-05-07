#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <array>
#include <cstdio>
#include <string>

namespace pluto {

namespace {

// Shell-quote a value for single-argument safety.
std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

class GrepTool : public Tool {
public:
	std::string name() const override { return "grep"; }
	std::string description() const override {
		return "Search for a regex pattern in files under a path. Returns "
		       "matching lines with file:line prefix. Use this before edit_file "
		       "to locate the exact text you want to change.";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"pattern":{"type":"string","description":"Regex pattern, POSIX ERE"},"path":{"type":"string","description":"Directory or file to search, default ."},"glob":{"type":"string","description":"Filename glob filter, e.g. *.cpp"},"ignore_case":{"type":"boolean","description":"Case-insensitive match"}},"required":["pattern"]})JSON";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string out = "grep '" + v.str("pattern") + "'";
			std::string p = v.str("path");
			if (!p.empty()) out += " in " + p;
			std::string g = v.str("glob");
			if (!g.empty()) out += " [" + g + "]";
			return out;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string pattern, path = ".", glob;
		bool icase = false;
		try {
			auto v = json::Value::parse(input_json);
			pattern = v.str("pattern");
			std::string p = v.str("path");
			if (!p.empty()) path = p;
			glob = v.str("glob");
			const json::Value* ic = v.find("ignore_case");
			if (ic && ic->is_bool()) icase = ic->as_bool();
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (pattern.empty()) { r.is_error = true; r.output = "pattern required"; return r; }
		// Guard against pathologically complex regex patterns.
		if (pattern.size() > 500) {
			r.is_error = true;
			r.output = "pattern too long (max 500 chars)";
			return r;
		}

		std::string cmd = "grep -rnE";
		if (icase) cmd += "i";
		cmd += " ";
		cmd += shq(pattern);
		cmd += " ";
		cmd += shq(path);
		if (!glob.empty()) {
			cmd += " --include=";
			cmd += shq(glob);
		}
		cmd += " 2>&1";

		FILE* p = popen(cmd.c_str(), "r");
		if (!p) { r.is_error = true; r.output = "popen failed"; return r; }
		std::array<char, 4096> buf;
		size_t n;
		while ((n = std::fread(buf.data(), 1, buf.size(), p)) > 0) {
			r.output.append(buf.data(), n);
		}
		int rc = pclose(p);
		if (rc != 0 && r.output.empty()) {
			r.output = "(no matches)";
		}
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> MakeGrepTool() { return std::make_unique<GrepTool>(); }

}  // namespace pluto
