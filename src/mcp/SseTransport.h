#pragma once

#include "Transport.h"

#include <string>

namespace pluto {

// The SSE transport is deprecated in the MCP spec in favor of streamable
// HTTP. pluto carries this stub so config files referencing type=sse
// produce a clear error instead of silently falling back.
class SseTransport : public Transport {
public:
	bool start(std::string& err) override {
		err = "sse transport is deprecated and not implemented in pluto; use type=http";
		return false;
	}
	void stop() override {}
	std::string request(const std::string&, std::string& err) override {
		err = "sse not started";
		return {};
	}
	void notify(const std::string&) override {}
};

}  // namespace pluto
