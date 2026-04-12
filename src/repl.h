#pragma once

#include "config.h"
#include "provider/Provider.h"
#include "tools/Tool.h"

namespace pluto {

int run_repl(Provider& provider, Config& cfg, ToolRegistry& tools, bool yolo);

}  // namespace pluto
