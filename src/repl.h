#pragma once

#include <memory>

#include "config.h"
#include "provider/Provider.h"
#include "tools/Tool.h"

namespace pluto {

int RunRepl(std::unique_ptr<Provider>& provider, Config& cfg,
             ToolRegistry& tools, bool yolo);

}  // namespace pluto
