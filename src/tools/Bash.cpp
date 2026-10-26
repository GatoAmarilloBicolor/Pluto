#include "Builtin.h"
#include "Tool.h"
#include "../json.h"
#include "../workspace.h"
#include "../util.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

namespace pluto {

namespace {

class BashTool : public Tool {
public:
	std::string name() const override { return "bash"; }
	std::string description() const override {
		return "Execute a shell command and return stdout and stderr merged. "
		       "Use for running programs, inspecting system state, and build/"
		       "test commands. Avoid interactive or long-running commands. "
		       "Optional timeout_secs (default 120, max 600).";
	}
	std::string input_schema_json() const override {
		return R"JSON({"type":"object","properties":{"command":{"type":"string","description":"Shell command"},"timeout_secs":{"type":"integer","description":"Timeout in seconds, default 120, max 600"}},"required":["command"]})JSON";
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
		int timeout_secs = 120;
		try {
			auto v = json::Value::parse(input_json);
			cmd = v.str("command");
			if (const auto* t = v.find("timeout_secs"); t && t->is_num())
				timeout_secs = static_cast<int>(t->as_num());
		} catch (const std::exception& e) {
			r.is_error = true; r.output = e.what(); return r;
		}
		if (cmd.empty()) { r.is_error = true; r.output = "command required"; return r; }
		// Check that cwd is within workspace.
		{
			std::string ws = WorkspaceRoot();
			if (!ws.empty()) {
				char cwd_buf[4096];
				std::string cwd_str;
				if (::getcwd(cwd_buf, sizeof(cwd_buf)))
					cwd_str = cwd_buf;
				else
					cwd_str = ws;
				if (!WorkspaceAllows(cwd_str)) {
					LogAccess(cwd_str, "bash", false);
					r.is_error = true;
					r.output = std::string("cwd ") + cwd_str
					         + " is outside workspace " + ws
					         + " — use `pluto trust add <dir>` to allow";
					return r;
				}
				LogAccess(cwd_str, "bash", true);
			}
		}
		if (timeout_secs < 1)   timeout_secs = 1;
		if (timeout_secs > 600) timeout_secs = 600;

		// Sandbox: always execute from the workspace root so relative
		// paths resolve inside the trusted directory.
		std::string prefix;
		{
			std::string ws = WorkspaceRoot();
			if (!ws.empty())
				prefix = "cd " + shq(ws) + " && ";
		}

		// Use timeout(1) if available, otherwise run without it.
		bool has_timeout = (::access("/bin/timeout", X_OK) == 0);
		std::string full;
		if (has_timeout)
			full = prefix + "timeout " + std::to_string(timeout_secs)
			     + " sh -c " + shq(cmd) + " 2>&1";
		else
			full = prefix + "sh -c " + shq(cmd) + " 2>&1";
		int rc = pluto::RunCommandCapture(full.c_str(), r.output);
		if (rc == -1) {
			r.is_error = true;
			r.output = "fork/exec failed";
			return r;
		}
		if (WIFEXITED(rc) && WEXITSTATUS(rc) == 124) {
			// timeout(1) returns 124 on timeout.
			r.is_error = true;
			r.output += "\n[timed out after " + std::to_string(timeout_secs) + "s]";
		} else if (rc != 0) {
			int code = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
			r.output += "\n[exit " + std::to_string(code) + "]";
		}
		if (!has_timeout && timeout_secs < 120)
			r.output += "\n[note: timeout not enforced â /bin/timeout not found]";
		const size_t kMax = 64 * 1024;
		if (r.output.size() > kMax) {
			r.output.resize(kMax);
			r.output += "\n[truncated]";
		}
		return r;
	}

private:
	static std::string shq(const std::string& s) {
		std::string out = "'";
		for (char c : s) {
			if (c == '\'') out += "'\\''";
			else out += c;
		}
		out += "'";
		return out;
	}
};

}  // namespace

std::unique_ptr<Tool> MakeBashTool() {
	return std::make_unique<BashTool>();
}

}  // namespace pluto
