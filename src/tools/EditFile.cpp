#include "Builtin.h"
#include "FileToolBase.h"
#include "../filelock.h"
#include "../json.h"
#include "../workspace.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

class EditFileTool : public FileToolBase {
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

protected:
    std::string tool_name_for_preview() const override { return "edit_file"; }
    std::string extract_path_for_preview(const json::Value& v) const override {
        std::string path = v.str("path");
        auto snippet = [](std::string s) {
            size_t nl = s.find('\n');
            if (nl != std::string::npos) s.resize(nl);
            if (s.size() > 80) { s.resize(77); s += "..."; }
            return s;
        };
        return path + "\n  - " + snippet(v.str("old_string")) + "\n  + " + snippet(v.str("new_string"));
    }

public:
    ToolResult execute(const std::string& input_json) override {
        ToolResult r;
        std::string path, old_s, new_s;
        try {
            auto v = json::Value::parse(input_json);
            path  = v.str("path");
            old_s = v.str("old_string");
            new_s = v.str("new_string");
        } catch (const std::exception& e) {
            set_json_error(r, e);
            return r;
        }
        if (path.empty() || old_s.empty()) {
            set_missing_field_error(r, "path and old_string");
            return r;
        }

        std::string canon = validate_path(r, path, "edit_file");
        if (canon.empty()) return r;

        // Lock the file for the entire read-verify-write cycle to
        // prevent TOCTOU races with concurrent edits.
        ScopedFileLock flock(canon + ".lock");

        std::ifstream in(canon, std::ios::binary);
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
            r.output = "old_string not found in " + canon;
            return r;
        }
        if (content.find(old_s, first + 1) != std::string::npos) {
            r.is_error = true;
            r.output = "old_string is not unique in " + canon +
                " (at least two occurrences); widen the context";
            return r;
        }
        content.replace(first, old_s.size(), new_s);

        std::ofstream out(canon, std::ios::binary | std::ios::trunc);
        if (!out) {
            r.is_error = true;
            r.output = "open write: " + std::string(std::strerror(errno));
            return r;
        }
        out.write(content.data(), content.size());
        if (!out) { r.is_error = true; r.output = "write failed"; return r; }
        r.output = "edited " + canon;
        return r;
    }
};

}  // namespace

std::unique_ptr<Tool> MakeEditFileTool() {
    return std::make_unique<EditFileTool>();
}

}  // namespace pluto
