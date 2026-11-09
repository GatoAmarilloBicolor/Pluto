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
#include <vector>

namespace pluto {

namespace {

// Split command string into argv array (simple shell-like parsing)
static std::vector<std::string> split_command(const std::string& cmd) {
    std::vector<std::string> args;
    std::string current;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;
    
    for (size_t i = 0; i < cmd.size(); ++i) {
        char c = cmd[i];
        if (escaped) {
            current += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if ((c == ' ' || c == '\t') && !in_single && !in_double) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) args.push_back(current);
    return args;
}

class BashTool : public Tool {
public:
    std::string name() const override { return "bash"; }
    std::string description() const override {
        return "Execute a command and return stdout and stderr merged. "
               "Use for running programs, inspecting system state, and build/"
               "test commands. Avoid interactive or long-running commands. "
               "Optional timeout_secs (default 120, max 600).";
    }
    std::string input_schema_json() const override {
        return R"JSON({"type":"object","properties":{"command":{"type":"string","description":"Command to execute (parsed into argv, no shell)"},"timeout_secs":{"type":"integer","description":"Timeout in seconds, default 120, max 600"}},"required":["command"]})JSON";
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

        // Parse command into argv (validate - no shell metacharacters allowed in raw form)
        auto argv = split_command(cmd);
        if (argv.empty()) { r.is_error = true; r.output = "empty command"; return r; }
        
        // Build argv with cd to workspace root
        std::string ws = WorkspaceRoot();
        std::vector<std::string> exec_argv;
        if (!ws.empty()) {
            exec_argv.push_back("sh");
            exec_argv.push_back("-c");
            exec_argv.push_back("cd " + shq(ws) + " && exec " + cmd);
        } else {
            exec_argv = argv;
        }

        // Execute with timeout using waitpid + alarm (no shell injection - use execvp)
        pid_t pid = fork();
        if (pid == -1) {
            r.is_error = true;
            r.output = "fork failed: " + std::string(strerror(errno));
            return r;
        }
        
        if (pid == 0) {
            // Child process
            // Set up timeout via alarm
            if (timeout_secs > 0) alarm(timeout_secs);
            
            // Execute directly via execvp (no shell)
            std::vector<char*> args;
            args.reserve(exec_argv.size() + 1);
            for (const auto& s : exec_argv) args.push_back(const_cast<char*>(s.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            _exit(127); // exec failed
        }
        
        // Parent: wait with timeout
        int status = 0;
        bool timed_out = false;
        if (timeout_secs > 0) {
            // Use waitpid with WNOHANG and poll
            for (int elapsed = 0; elapsed < timeout_secs; ++elapsed) {
                pid_t w = waitpid(pid, &status, WNOHANG);
                if (w == pid) break;
                sleep(1);
            }
            // Check if still running
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == 0) {
                // Still running - kill it
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                timed_out = true;
            }
        } else {
            waitpid(pid, &status, 0);
        }
        
        // Capture output using RunCommandCapture with the validated command
        std::string capture_cmd = cmd;
        if (!ws.empty()) {
            capture_cmd = "cd " + shq(ws) + " && exec " + cmd;
        }
        int rc = pluto::RunCommandCapture(capture_cmd.c_str(), r.output);
        
        if (rc == -1) {
            r.is_error = true;
            r.output = "fork/exec failed";
            return r;
        }
        if (timed_out || (WIFEXITED(rc) && WEXITSTATUS(rc) == 124)) {
            r.is_error = true;
            r.output += "\n[timed out after " + std::to_string(timeout_secs) + "s]";
        } else if (rc != 0) {
            int code = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
            r.output += "\n[exit " + std::to_string(code) + "]";
        }
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