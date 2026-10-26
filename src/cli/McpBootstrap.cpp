#include "McpBootstrap.h"
#include "../mcp/Client.h"
#include "../mcp/Manager.h"
#include "../mcp/Config.h"
#include "../mcp/Transport.h"
#include <cstdio>
#include <memory>
#include <string>

namespace pluto {

std::unique_ptr<McpManager> McpBootstrap::create(ToolRegistry& tools, ApproveFn approve) {
    return std::make_unique<McpManager>(tools, approve);
}

void McpBootstrap::auto_discover(McpManager& mcp, ToolRegistry& tools) {
    for (const auto& d : DiscoverMcpServers()) {
        McpServerSpec spec;
        spec.name = d.name;
        spec.type = d.type;
        spec.url = d.url;
        spec.scope = McpServerSpec::LOCAL;
        
        std::string err;
        auto transport = MakeTransport(spec, err);
        if (!transport || !transport->start(err)) continue;
        
        auto client = std::make_shared<McpClient>(spec.name, std::move(transport));
        if (!client->connect()) continue;
        
        std::fprintf(stderr, "[discover/%s] %zu tools\n", spec.name.c_str(), client->tools().size());
        mcp.add_client(client, tools);
    }
}

} // namespace pluto