/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "Builtin.h"
#include "Tool.h"
#include "../json.h"
#include "../workspace.h"
#include "../lsp/LspClient.h"
#include "../lsp/LspConfig.h"

#include <cstdio>
#include <sstream>
#include <string>


namespace pluto {


namespace {


// Shared LSP client instance, created on first use and kept alive
// for the session. One client per language server.
struct LspSession {
	std::string					serverName;
	std::unique_ptr<LspClient>	client;
};

std::vector<LspSession> gSessions;
std::vector<LspServerConfig> gConfigs;
bool gConfigsLoaded = false;


LspClient*
GetOrCreateClient(const std::string& filePath, std::string& error)
{
	if (!gConfigsLoaded) {
		gConfigs = LoadLspConfigs();
		gConfigsLoaded = true;
	}

	std::string ext = FileExtension(filePath);
	const LspServerConfig* config = FindLspServer(gConfigs, ext);
	if (config == NULL) {
		error = "no LSP server configured for " + ext
			+ " files. Add one to ~/config/settings/pluto/lsp.conf";
		return NULL;
	}

	// Reuse existing session for this server.
	for (auto& session : gSessions) {
		if (session.serverName == config->name && session.client->IsRunning())
			return session.client.get();
	}

	// Create new session.
	auto client = std::make_unique<LspClient>();
	if (!client->Start(config->command, config->args, error))
		return NULL;

	// Initialize with workspace root as the project URI.
	std::string rootUri = "file://" + WorkspaceRoot();
	if (!client->Initialize(rootUri, error))
		return NULL;

	LspClient* raw = client.get();
	gSessions.push_back({config->name, std::move(client)});
	return raw;
}


// Format a file URI back to a readable path.
std::string
UriToPath(const std::string& uri)
{
	if (uri.rfind("file://", 0) == 0)
		return uri.substr(7);
	return uri;
}


class LspTool : public Tool {
public:
	std::string
	name() const override
	{
		return "lsp";
	}

	std::string
	description() const override
	{
		return "Query a Language Server for code intelligence. Provides hover "
			"info (type signatures), go-to-definition, find-references, "
			"diagnostics (errors/warnings), and document symbols. The language "
			"server (e.g. clangd for C++) is started automatically on first use. "
			"Line and character are 0-based.";
	}

	std::string
	input_schema_json() const override
	{
		return R"JSON({
			"type": "object",
			"properties": {
				"action": {
					"type": "string",
					"enum": ["hover", "definition", "references", "diagnostics", "symbols"],
					"description": "The LSP operation to perform"
				},
				"path": {
					"type": "string",
					"description": "File path to query"
				},
				"line": {
					"type": "integer",
					"description": "0-based line number (for hover/definition/references)"
				},
				"character": {
					"type": "integer",
					"description": "0-based character offset (for hover/definition/references)"
				}
			},
			"required": ["action", "path"]
		})JSON";
	}

	std::string
	preview(const std::string& inputJson) const override
	{
		try {
			auto value = json::Value::parse(inputJson);
			std::string action = value.str("action");
			std::string path = value.str("path");
			std::string result = "lsp " + action + " " + path;
			const json::Value* lineVal = value.find("line");
			if (lineVal != NULL && lineVal->is_num())
				result += ":" + std::to_string(static_cast<int>(lineVal->as_num()));
			return result;
		} catch (const std::exception&) {
			return Tool::preview(inputJson);
		}
	}

	ToolResult
	execute(const std::string& inputJson) override
	{
		ToolResult result;
		std::string action, path;
		int32_t line = 0, character = 0;

		try {
			auto value = json::Value::parse(inputJson);
			action = value.str("action");
			path = value.str("path");
			const json::Value* lineVal = value.find("line");
			if (lineVal != NULL && lineVal->is_num())
				line = static_cast<int32_t>(lineVal->as_num());
			const json::Value* charVal = value.find("character");
			if (charVal != NULL && charVal->is_num())
				character = static_cast<int32_t>(charVal->as_num());
		} catch (const std::exception& exception) {
			result.is_error = true;
			result.output = exception.what();
			return result;
		}

		if (action.empty() || path.empty()) {
			result.is_error = true;
			result.output = "action and path required";
			return result;
		}

		// Resolve relative paths against workspace.
		if (!path.empty() && path[0] != '/') {
			std::string root = WorkspaceRoot();
			if (!root.empty())
				path = root + "/" + path;
		}

		std::string error;
		LspClient* client = GetOrCreateClient(path, error);
		if (client == NULL) {
			result.is_error = true;
			result.output = error;
			return result;
		}

		if (action == "hover") {
			LspHoverResult hover = client->Hover(path, line, character, error);
			if (!error.empty()) {
				result.is_error = true;
				result.output = error;
			} else if (hover.empty) {
				result.output = "(no hover info at this position)";
			} else {
				result.output = hover.contents;
			}
		} else if (action == "definition") {
			auto locations = client->GoToDefinition(
				path, line, character, error);
			if (!error.empty()) {
				result.is_error = true;
				result.output = error;
			} else if (locations.empty()) {
				result.output = "(no definition found)";
			} else {
				std::ostringstream output;
				for (const auto& location : locations) {
					output << UriToPath(location.uri) << ":"
						<< (location.line + 1) << ":"
						<< (location.character + 1) << "\n";
				}
				result.output = output.str();
			}
		} else if (action == "references") {
			auto locations = client->FindReferences(
				path, line, character, error);
			if (!error.empty()) {
				result.is_error = true;
				result.output = error;
			} else if (locations.empty()) {
				result.output = "(no references found)";
			} else {
				std::ostringstream output;
				output << locations.size() << " reference(s):\n";
				for (const auto& location : locations) {
					output << "  " << UriToPath(location.uri) << ":"
						<< (location.line + 1) << ":"
						<< (location.character + 1) << "\n";
				}
				result.output = output.str();
			}
		} else if (action == "diagnostics") {
			auto diagnostics = client->GetDiagnostics(path, error);
			if (!error.empty()) {
				result.is_error = true;
				result.output = error;
			} else if (diagnostics.empty()) {
				result.output = "(no diagnostics)";
			} else {
				std::ostringstream output;
				output << diagnostics.size() << " diagnostic(s):\n";
				const char* severityNames[] = {
					"unknown", "error", "warning", "info", "hint"};
				for (const auto& diagnostic : diagnostics) {
					int severity = diagnostic.severity;
					if (severity < 0 || severity > 4)
						severity = 0;
					output << "  " << (diagnostic.line + 1)
						<< ":" << (diagnostic.character + 1)
						<< " [" << severityNames[severity] << "] "
						<< diagnostic.message << "\n";
				}
				result.output = output.str();
			}
		} else if (action == "symbols") {
			auto symbols = client->DocumentSymbols(path, error);
			if (!error.empty()) {
				result.is_error = true;
				result.output = error;
			} else if (symbols.empty()) {
				result.output = "(no symbols found)";
			} else {
				std::ostringstream output;
				output << symbols.size() << " symbol(s):\n";
				for (const auto& symbol : symbols) {
					output << "  " << (symbol.line + 1) << " "
						<< symbol.kind << " " << symbol.name << "\n";
				}
				result.output = output.str();
			}
		} else {
			result.is_error = true;
			result.output = "unknown action: " + action
				+ " (use hover, definition, references, diagnostics, symbols)";
		}

		return result;
	}
};


}	// namespace


std::unique_ptr<Tool>
MakeLspTool()
{
	return std::make_unique<LspTool>();
}


}	// namespace pluto
