#pragma once

#include "../tools/Tool.h"
#include "../config.h"
#include "../rag.h"
#include "../wiki.h"
#include <memory>

namespace pluto {

class ToolRegistryBuilder {
public:
    static ToolRegistry build(const Config& cfg);
    
private:
    static void add_builtin_tools(ToolRegistry& tools);
    static void add_haiku_tools(ToolRegistry& tools);
    static void add_rag_tools(ToolRegistry& tools);
    static void add_wiki_tools(ToolRegistry& tools);
    static void add_web_tools(ToolRegistry& tools);
    static void add_misc_tools(ToolRegistry& tools);
};

} // namespace pluto