#pragma once

#include <memory>
#include <string>

namespace pluto {

struct McpServerSpec;

// Low-level JSON-RPC wire for an MCP server. Created once per server;
// reused across initialize/tools/list/tools/call. Not thread-safe —
// the caller serializes requests.
class Transport {
public:
	virtual ~Transport() = default;

	// Establish connection or spawn subprocess. Called once before requests.
	virtual bool start(std::string& err) = 0;

	// Tear down. Safe to call multiple times.
	virtual void stop() = 0;

	// Send a JSON-RPC request body, return the response body.
	// Empty string + populated err on failure.
	virtual std::string request(const std::string& body, std::string& err) = 0;

	// Fire-and-forget notification (no id, no response expected).
	virtual void notify(const std::string& body) = 0;
};

std::unique_ptr<Transport> make_transport(const McpServerSpec& spec, std::string& err);

}  // namespace pluto
