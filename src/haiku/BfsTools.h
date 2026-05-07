#pragma once

#include <memory>

#include "../tools/Tool.h"

namespace pluto {

// BFS extended attributes — native Haiku tools.
std::unique_ptr<Tool> MakeReadAttrTool();
std::unique_ptr<Tool> MakeWriteAttrTool();
std::unique_ptr<Tool> MakeListAttrsTool();
std::unique_ptr<Tool> MakeRemoveAttrTool();

// BQuery wrapper — one-shot attribute queries over a volume.
std::unique_ptr<Tool> MakeQueryFsTool();

// Live BQuery — runs the query for a duration and returns initial matches
// plus add/remove events seen during the watch window.
std::unique_ptr<Tool> MakeLiveQueryTool();

}  // namespace pluto
