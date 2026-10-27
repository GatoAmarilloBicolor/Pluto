#include "ToolRegistryBuilder.h"
#include "tools/Builtin.h"
#include "tools/BeapiLookup.h"
#include "tools/WikiLookup.h"
#include "tools/WebFetch.h"
#include "tools/WebSearch.h"
#include "tools/GraphQuery.h"
#include "haiku/BfsTools.h"
#include "haiku/DesktopTools.h"
#include "config.h"
#include "rag.h"
#include "wiki.h"

namespace pluto {

ToolRegistry ToolRegistryBuilder::build([[maybe_unused]] const Config& cfg) {
    ToolRegistry tools;
    
    add_builtin_tools(tools);
    add_haiku_tools(tools);
    add_rag_tools(tools);
    add_wiki_tools(tools);
    add_web_tools(tools);
    add_misc_tools(tools);
    
    return tools;
}

void ToolRegistryBuilder::add_builtin_tools(ToolRegistry& tools) {
    tools.add(MakeReadFileTool());
    tools.add(MakeWriteFileTool());
    tools.add(MakeEditFileTool());
    tools.add(MakeBashTool());
    tools.add(MakeGrepTool());
    tools.add(MakeGlobTool());
    tools.add(MakeTodoWriteTool());
    tools.add(MakeApplyPatchTool());
    tools.add(MakeLspTool());
    tools.add(MakeGraphQueryTool());
}

void ToolRegistryBuilder::add_haiku_tools(ToolRegistry& tools) {
    tools.add(MakeReadAttrTool());
    tools.add(MakeWriteAttrTool());
    tools.add(MakeListAttrsTool());
    tools.add(MakeRemoveAttrTool());
    tools.add(MakeQueryFsTool());
    tools.add(MakeDesktopScriptTool());
    tools.add(MakeLaunchAppTool());
    tools.add(MakeLiveQueryTool());
    tools.add(MakeClipboardTool());
    tools.add(MakeNodeMonitorTool());
}

void ToolRegistryBuilder::add_rag_tools(ToolRegistry& tools) {
    if (RagEntryCount() > 0) {
        tools.add(MakeBeapiLookupTool());
    }
}

void ToolRegistryBuilder::add_wiki_tools(ToolRegistry& tools) {
    if (WikiEntryCount() > 0) {
        tools.add(MakeWikiLookupTool());
    }
}

void ToolRegistryBuilder::add_web_tools(ToolRegistry& tools) {
    tools.add(MakeWebFetchTool());
    tools.add(MakeWebSearchTool());
}

void ToolRegistryBuilder::add_misc_tools([[maybe_unused]] ToolRegistry& tools) {
    // No additional misc tools for now
}

} // namespace pluto
