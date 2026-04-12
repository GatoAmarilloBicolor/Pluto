#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <array>
#include <cstdio>
#include <string>

namespace pluto {

namespace {

class BashTool : public Tool {
public:
	std::string name() const override { return "bash"; }
	std::string description() const override {
		return "Execute a shell command and return stdout and stderr merged. "
		       "Use for running programs, inspecting system state, and build/"
		       "test commands. Avoid interactive or long-running commands.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{"command":{"type":"string","description":"Shell command"}},)"
		       R"("required":["command"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			std::string cmd = v.str("command");
			if (cmd.size() > 200) { cmd.resize(197); cmd += "..."; }
			return "bash: " + cmd;
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string cmd;
		try {
			auto v = json::Value::parse(input_json);
			cmd = v.str("command");
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (cmd.empty()) { r.is_error = true; r.output = "command required"; return r; }

		std::string full = cmd + " 2>&1";
		FILE* p = popen(full.c_str(), "r");
		if (!p) { r.is_error = true; r.output = "popen failed"; return r; }
		std::array<char, 4096> buf;
		size_t n;
		while ((n = std::fread(buf.data(), 1, buf.size(), p)) > 0) {
			r.output.append(buf.data(), n);
		}
		int rc = pclose(p);
		if (rc != 0) {
			r.output += "\n[exit " + std::to_string(rc) + "]";
		}
		const size_t kMax = 64 * 1024;
		if (r.output.size() > kMax) {
			r.output.resize(kMax);
			r.output += "\n[truncated]";
		}
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> make_bash_tool() {
	return std::make_unique<BashTool>();
}

}  // namespace pluto
