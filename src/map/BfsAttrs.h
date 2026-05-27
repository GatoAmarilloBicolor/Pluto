#pragma once

#include "Graph.h"
#include <string>

namespace pluto {
namespace map {

// Stamp graph metadata as BFS extended attributes on source files.
// Attributes: pluto:graph:kind, pluto:graph:includes, pluto:graph:defines,
//             pluto:graph:edges_out, pluto:graph:edges_in
int StampBfsAttributes(const Graph& graph);

}  // namespace map
}  // namespace pluto
