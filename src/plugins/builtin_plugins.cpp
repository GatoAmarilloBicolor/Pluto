#include "plugins/builtin_plugins.h"
#include "tools/Builtin.h"
#include "tools/WebFetch.h"
#include "tools/WebSearch.h"
#include "tools/WikiLookup.h"
#include "tools/BeapiLookup.h"
#include "tools/GraphQuery.h"

namespace pluto {

void BuiltinToolsProvider::populate(ToolRegistry& registry) const {
    // Core file tools (via factory functions from Builtin.h)
    registry.add(MakeReadFileTool());
    registry.add(MakeWriteFileTool());
    registry.add(MakeEditFileTool());
    registry.add(MakeBashTool());
    registry.add(MakeGrepTool());
    registry.add(MakeGlobTool());
    registry.add(MakeTodoWriteTool());
    registry.add(MakeApplyPatchTool());
    registry.add(MakeLspTool());
    
    // Web tools
    registry.add(MakeWebFetchTool());
    registry.add(MakeWebSearchTool());
    
    // Lookup tools
    registry.add(MakeWikiLookupTool());
    registry.add(MakeBeapiLookupTool());
    
    // Graph query
    registry.add(MakeGraphQueryTool());
    
    // MCP tools (conditionally added based on config)
    // These are added by the caller based on MCP config
}

} // namespace pluto