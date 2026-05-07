#include "Builtin.h"
#include "Tool.h"
#include "../json.h"
#include "../workspace.h"

#include <File.h>
#include <NodeInfo.h>

#include <cerrno>
#include <cstdlib>
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
		// Canonicalize to resolve symlinks before workspace check.
		char* resolved = realpath(path.c_str(), nullptr);
		if (resolved) { path = resolved; free(resolved); }
		if (!WorkspaceAllows(path)) {
			LogAccess(path, "read_file", false);
			r.is_error = true;
			r.output = path + " is outside workspace " + WorkspaceRoot()
			         + " — use `pluto trust add <dir>` to allow";
			return r;
		}
		LogAccess(path, "read_file", true);
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
				std::string desc = "binary file";
				BNode node(path.c_str());
				BNodeInfo info(&node);
				char mime[256] = {};
				if (info.GetType(mime) == B_OK && mime[0])
					desc = std::string(mime) + " file";
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
