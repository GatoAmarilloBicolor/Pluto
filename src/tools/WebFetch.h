#pragma once

#include "Tool.h"

namespace pluto {

std::unique_ptr<Tool> MakeWebFetchTool();

}  // namespace pluto
