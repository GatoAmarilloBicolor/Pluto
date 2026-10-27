// Serena MCP tools — codebase exploration (symbol search, file read/edit, LSP).
// Wraps the Serena MCP server (stdio) as Pluto tools.

#include "../mcp/Client.h"
#include "../mcp/builtin/mcp_tool_wrapper.h"
#include "Tool.h"
#include "../json.h"

#include <memory>
#include <string>

namespace pluto {

// ---- serena_read_file ----
class SerenaReadFileTool : public Tool {
public:
	std::string name() const override { return "serena_read_file"; }
	std::string description() const override {
		return "Read a file from the Serena codebase exploration server. "
		       "Returns the file contents as text.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object","properties":{"path":{"type":"string","description":"Absolute or relative path to read"}},"required":["path"]})";
	}
	std::string preview(const std::string& input_json) const override {
		return "serena_read_file " + input_json.substr(0, 80);
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		try {
			auto v = json::Value::parse(input_json);
			std::string path = v.str("path");
			// Build MCP call: serena__serena_fs__read_file
			std::string args = "{\"relative_path\":\"" + path + "\"}";
			// This will be called via the MCP client wrapper; here we delegate
			// to the registered MCP tool through a registry lookup.
			// For standalone use, return a hint.
			r.output = "Use via MCP client: serena__serena_fs__read_file with path=" + path;
		} catch (const std::exception& e) {
			r.is_error = true;
			r.output = e.what();
		}
		return r;
	}
};

// ---- serena_search_pattern ----
class SerenaSearchPatternTool : public Tool {
public:
	std::string name() const override { return "serena_search_pattern"; }
	std::string description() const override {
		return "Search for a pattern in files via Serena. "
		       "Returns matching lines with context.";
	}
	std::string input_schema_json() const override {
		return R"json({"type":"object","properties":{"pattern":{"type":"string","description":"Regex pattern to search"},"path":{"type":"string","description":"Directory to search (default '.')"}},"required":["pattern"]})json";
	}
	std::string preview(const std::string& input_json) const override {
		return "serena_search_pattern " + input_json.substr(0, 80);
	}
	ToolResult execute([[maybe_unused]] const std::string& input_json) override {
		ToolResult r;
		r.output = "Use via MCP client: serena__serena_fs__search_for_pattern";
		return r;
	}
};

// Register Serena tools into the registry.
void register_serena_tools(ToolRegistry& registry,
                           std::shared_ptr<McpClient> client) {
	int count = ::pluto::mcp::register_mcp_client_tools(client, registry, "serena__");
	(void)count;
}

}  // namespace pluto
