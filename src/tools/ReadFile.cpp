#include "Builtin.h"
#include "FileToolBase.h"
#include "../json.h"
#include "../workspace.h"

#ifdef __HAIKU__
#include <File.h>
#include <NodeInfo.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

class ReadFileTool : public FileToolBase {
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

protected:
    std::string tool_name_for_preview() const override { return "read_file"; }
    std::string extract_path_for_preview(const json::Value& v) const override { return v.str("path"); }

public:
    ToolResult execute(const std::string& input_json) override {
        ToolResult r;
        auto path_opt = parse_string_field(input_json, "path");
        if (!path_opt) { set_json_error(r, std::runtime_error("invalid JSON")); return r; }

        std::string path = validate_path(r, *path_opt, "read_file");
        if (path.empty()) return r;

        std::ifstream f(path, std::ios::binary);
        if (!f) {
            r.is_error = true;
            r.output = "cannot open " + path + ": " + std::strerror(errno);
            return r;
        }

        // Get file size to reject very large files early (before loading).
        f.seekg(0, std::ios::end);
        auto file_size = f.tellg();
        f.seekg(0, std::ios::beg);
        constexpr long kMaxReadSize = 64L * 1024 * 1024;  // 64 MB
        if (file_size > kMaxReadSize) {
            r.is_error = true;
            r.output = "file too large (" + std::to_string(file_size)
                     + " bytes, limit " + std::to_string(kMaxReadSize)
                     + "): " + path;
            return r;
        }

        // Detect binary files with a small sentinel read before loading
        // the full file. This avoids loading multi-GB files into memory.
        {
            char probe[8192];
            f.read(probe, sizeof(probe));
            auto got = f.gcount();
            bool is_binary = false;
            for (std::streamsize i = 0; i < got; ++i) {
                if ((unsigned char)probe[i] == 0) { is_binary = true; break; }
            }
            if (is_binary) {
#ifdef __HAIKU__
                std::string desc = "binary file";
                BNode node(path.c_str());
                BNodeInfo info(&node);
                char mime[256] = {};
                if (info.GetType(mime) == B_OK && mime[0])
                    desc = std::string(mime) + " file";
#else
                std::string desc = "binary file";
#endif
                r.output = "(" + desc + ", "
                         + std::to_string(file_size) + " bytes"
                         + ", path: " + path
                         + ", cannot display as text)";
                return r;
            }
            f.clear();  // clear EOF/failbit before seeking back
            f.seekg(0, std::ios::beg);
        }

        std::ostringstream os;
        os << f.rdbuf();
        r.output = os.str();
        return r;
    }
};

}  // namespace

std::unique_ptr<Tool> MakeReadFileTool() {
    return std::make_unique<ReadFileTool>();
}

}  // namespace pluto
