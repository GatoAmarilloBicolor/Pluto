#include "Transport.h"

#include "Config.h"
#include "HttpTransport.h"
#include "SseTransport.h"
#include "StdioTransport.h"

namespace pluto {

std::unique_ptr<Transport> make_transport(const McpServerSpec& spec, std::string& err) {
	if (spec.type == "http") {
		return std::make_unique<HttpTransport>(spec.url, spec.headers);
	}
	if (spec.type == "sse") {
		return std::make_unique<SseTransport>();
	}
	if (spec.type == "stdio" || spec.type.empty()) {
		return std::make_unique<StdioTransport>(spec.command, spec.args, spec.env);
	}
	err = "unknown transport type: " + spec.type;
	return nullptr;
}

}  // namespace pluto
