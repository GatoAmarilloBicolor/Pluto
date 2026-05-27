#pragma once

#include "Tool.h"
#include <memory>

namespace pluto {

std::unique_ptr<Tool> MakeGraphQueryTool();

}  // namespace pluto
