#pragma once

#include "Transport.h"

#include <map>
#include <string>

namespace pluto {

// MCP SSE transport (2024-11-05 spec).
//
// Protocol:
//   1. start(): GET the SSE URL, read the first "endpoint" event to learn
//      the message URL for JSON-RPC POSTs.
//   2. request(): POST JSON-RPC body to the message URL, return response.
//   3. Server-initiated events arrive on the SSE stream (not yet used).
//
// Used by Pippo and other SSE-based MCP servers.
class SseTransport : public Transport {
public:
	SseTransport(std::string sse_url, std::map<std::string, std::string> headers);

	bool start(std::string& err) override;
	void stop() override {}
	std::string request(const std::string& body, std::string& err) override;
	void notify(const std::string& body) override;

private:
	std::string fSseUrl;
	std::string fMessageUrl;
	std::map<std::string, std::string> fHeaders;

	std::string post(const std::string& body, std::string& err);
};

}  // namespace pluto
