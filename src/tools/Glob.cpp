#include "Builtin.h"
#include "FileToolBase.h"
#include "../json.h"
#include "../workspace.h"
#include "../util.h"

#include <sys/wait.h>

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

class GlobTool : public FileToolBase {
public:
    std::string name() const override { return "glob"; }
    std::string description() const override {
        return "Find files matching a name glob under a path. Returns a list "
               "of matching paths, one per line.";
    }
    std::string input_schema_json() const override {
        return R"JSON({"type":"object","properties":{"pattern":{"type":"string","description":"Filename glob, e.g. *.cpp"},"path":{"type":"string","description":"Root directory, default ."}},"required":["pattern"]})JSON";
    }

protected:
    std::string tool_name_for_preview() const override { return "glob"; }
    std::string extract_path_for_preview(const json::Value& v) const override { 
        std::string p = v.str("path");
        return v.str("pattern") + " in " + (p.empty() ? "." : p); 
    }

public:
    ToolResult execute(const std::string& input_json) override {
        ToolResult r;
        std::string pattern, path = ".";
        try {
            auto v = json::Value::parse(input_json);
            pattern = v.str("pattern");
            std::string p = v.str("path");
            if (!p.empty()) path = p;
        } catch (const std::exception& e) {
            set_json_error(r, e);
            return r;
        }
        if (pattern.empty()) { set_missing_field_error(r, "pattern"); return r; }
        if (pattern.size() > 200) {
            r.is_error = true;
            r.output = "pattern too long (max 200 chars)";
            return r;
        }

        // Resolve and validate path against workspace sandbox.
        char* resolved = realpath(path.c_str(), nullptr);
        if (resolved) { path = resolved; free(resolved); }
        if (!WorkspaceAllows(path)) {
            r.is_error = true;
            r.output = path + " is outside the workspace";
            return r;
        }

        std::string cmd = "find " + shq(path) + " -type f -name " + shq(pattern) + " 2>&1";
        int rc = pluto::RunCommandCapture(cmd.c_str(), r.output);
        if (rc == -1) { r.is_error = true; r.output = "fork/exec failed"; return r; }
        if (r.output.empty()) r.output = "(no matches)";
        else if (rc != 0 && WIFEXITED(rc) && WEXITSTATUS(rc) != 0) {
            r.output += "\n[exit " + std::to_string(WEXITSTATUS(rc)) + "]";
        }
        return r;
    }
};

}  // namespace

std::unique_ptr<Tool> MakeGlobTool() { return std::make_unique<GlobTool>(); }

}  // namespace pluto
