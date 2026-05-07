#pragma once

#include "Tool.h"

namespace pluto {

std::unique_ptr<Tool> MakeReadFileTool();
std::unique_ptr<Tool> MakeWriteFileTool();
std::unique_ptr<Tool> MakeEditFileTool();
std::unique_ptr<Tool> MakeBashTool();
std::unique_ptr<Tool> MakeGrepTool();
std::unique_ptr<Tool> MakeGlobTool();
std::unique_ptr<Tool> MakeTodoWriteTool();
std::unique_ptr<Tool> MakeApplyPatchTool();
std::unique_ptr<Tool> MakeLspTool();

}  // namespace pluto
