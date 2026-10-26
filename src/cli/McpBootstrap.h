#pragma once

#include "../mcp/Manager.h"
#include "../tools/Tool.h"
#include "../config.h"
#include <functional>
#include <memory>

namespace pluto {

class McpBootstrap {
public:
    using ApproveFn = std::function<bool(const McpServerSpec& spec)>;
    
    static std::unique_ptr<McpManager> create(ToolRegistry& tools, ApproveFn approve);
    static void auto_discover(McpManager& mcp, ToolRegistry& tools);
    
private:
    static std::shared_ptr<McpClient> connect_server(const McpServerSpec& spec);
};

} // namespace pluto