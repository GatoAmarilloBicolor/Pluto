#include "Client.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "../json.h"

namespace pluto {

namespace {

class RemoteTool : public Tool {
public:
	RemoteTool(McpClient* client,
	           std::string prefixed_name,
	           std::string remote_name,
	           std::string description,
	           std::string schema)
		: fClient(client),
		  fName(std::move(prefixed_name)),
		  fRemoteName(std::move(remote_name)),
		  fDescription(std::move(description)),
		  fSchema(std::move(schema)) {}

	std::string name() const override { return fName; }
	std::string description() const override { return fDescription; }
	std::string input_schema_json() const override { return fSchema; }
	std::string preview(const std::string& input_json) const override {
		std::string out = fName + " " + input_json;
		if (out.size() > 200) { out.resize(197); out += "..."; }
		return out;
	}
	ToolResult execute(const std::string& input_json) override {
		return fClient->call_tool(fRemoteName, input_json);
	}

private:
	McpClient* fClient;
	std::string fName;
	std::string fRemoteName;
	std::string fDescription;
	std::string fSchema;
};

}  // namespace

McpClient::McpClient(std::string server_name, std::unique_ptr<Transport> transport)
	: fServerName(std::move(server_name)), fTransport(std::move(transport)) {}

McpClient::~McpClient() {
	if (fTransport) fTransport->stop();
}

bool McpClient::connect() {
	using namespace json;

	// initialize
	{
		Value req;
		req.set("jsonrpc", std::string("2.0"));
		req.set("id", Value(fNextId++));
		req.set("method", std::string("initialize"));
		Value params;
		params.set("protocolVersion", std::string("2024-11-05"));
		params.set("capabilities", Value(Object{}));
		Value info;
		info.set("name", std::string("pluto"));
		info.set("version", std::string("0.3.0"));
		params.set("clientInfo", info);
		req.set("params", params);

		std::string err;
		std::string resp = fTransport->request(req.serialize(), err);
		if (resp.empty()) {
			std::fprintf(stderr, "[mcp/%s] initialize failed: %s\n",
				fServerName.c_str(), err.c_str());
			return false;
		}
		try {
			Value rv = Value::parse(resp);
			if (rv.find("error")) {
				std::fprintf(stderr, "[mcp/%s] initialize error: %s\n",
					fServerName.c_str(), resp.c_str());
				return false;
			}
		} catch (const std::exception& e) {
			std::fprintf(stderr, "[mcp/%s] initialize bad JSON: %s\n",
				fServerName.c_str(), e.what());
			return false;
		}
	}

	// notifications/initialized
	{
		Value note;
		note.set("jsonrpc", std::string("2.0"));
		note.set("method", std::string("notifications/initialized"));
		note.set("params", Value(Object{}));
		fTransport->notify(note.serialize());
	}

	// tools/list
	{
		Value req;
		req.set("jsonrpc", std::string("2.0"));
		req.set("id", Value(fNextId++));
		req.set("method", std::string("tools/list"));
		req.set("params", Value(Object{}));

		std::string err;
		std::string resp = fTransport->request(req.serialize(), err);
		if (resp.empty()) {
			std::fprintf(stderr, "[mcp/%s] tools/list failed: %s\n",
				fServerName.c_str(), err.c_str());
			return false;
		}
		try {
			Value rv = Value::parse(resp);
			const Value* result = rv.find("result");
			if (!result || !result->is_obj()) {
				std::fprintf(stderr, "[mcp/%s] tools/list: no result\n",
					fServerName.c_str());
				return false;
			}
			const Value* tools = result->find("tools");
			if (!tools || !tools->is_arr()) {
				std::fprintf(stderr, "[mcp/%s] tools/list: no tools array\n",
					fServerName.c_str());
				return false;
			}
			for (const auto& t : tools->as_arr()) {
				if (!t.is_obj()) continue;
				McpToolInfo info;
				info.name = t.str("name");
				info.description = t.str("description");
				const Value* schema = t.find("inputSchema");
				info.input_schema_json = schema
					? schema->serialize()
					: std::string(R"({"type":"object","properties":{}})");
				if (!info.name.empty()) fTools.push_back(std::move(info));
			}
		} catch (const std::exception& e) {
			std::fprintf(stderr, "[mcp/%s] tools/list bad JSON: %s\n",
				fServerName.c_str(), e.what());
			return false;
		}
	}
	return true;
}

ToolResult McpClient::call_tool(const std::string& name, const std::string& arguments_json) {
	using namespace json;
	ToolResult r;

	Value req;
	req.set("jsonrpc", std::string("2.0"));
	req.set("id", Value(fNextId++));
	req.set("method", std::string("tools/call"));

	Value params;
	params.set("name", name);
	Value args(Object{});
	if (!arguments_json.empty()) {
		try { args = Value::parse(arguments_json); }
		catch (const std::exception&) { args = Value(Object{}); }
	}
	params.set("arguments", args);
	req.set("params", params);

	std::string err;
	std::string resp = fTransport->request(req.serialize(), err);
	if (resp.empty()) {
		r.is_error = true;
		r.output = "mcp transport: " + err;
		return r;
	}
	try {
		Value rv = Value::parse(resp);
		const Value* e = rv.find("error");
		if (e && e->is_obj()) {
			r.is_error = true;
			r.output = "mcp error: " + e->str("message");
			return r;
		}
		const Value* result = rv.find("result");
		if (!result || !result->is_obj()) {
			r.is_error = true;
			r.output = "mcp: malformed result";
			return r;
		}
		const Value* content = result->find("content");
		std::string text;
		if (content && content->is_arr()) {
			bool first = true;
			for (const auto& item : content->as_arr()) {
				if (!item.is_obj()) continue;
				if (item.str("type") == "text") {
					if (!first) text += "\n";
					text += item.str("text");
					first = false;
				}
			}
		}
		r.output = std::move(text);
		const Value* is_err = result->find("isError");
		if (is_err && is_err->is_bool() && is_err->as_bool()) r.is_error = true;
	} catch (const std::exception& ex) {
		r.is_error = true;
		r.output = std::string("mcp bad JSON: ") + ex.what();
	}
	return r;
}

void register_mcp_tools(McpClient& client, ToolRegistry& registry) {
	int skipped = 0;
	for (const auto& info : client.tools()) {
		// Skip per-app generated scripting wrappers (e.g. app_application_x_vnd_*)
		// — they are redundant with the generic get_property/set_property tools
		// and overwhelm the model when there are hundreds of them.
		if (info.name.substr(0, 4) == "app_") {
			++skipped;
			continue;
		}
		std::string prefixed = client.server_name() + "__" + info.name;
		registry.add(std::make_unique<RemoteTool>(
			&client, prefixed, info.name, info.description, info.input_schema_json));
	}
	if (skipped > 0)
		std::fprintf(stderr, "[mcp/%s] skipped %d per-app wrappers\n",
			client.server_name().c_str(), skipped);
}

}  // namespace pluto
