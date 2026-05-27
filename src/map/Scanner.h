#pragma once

#include "Graph.h"
#include <string>

namespace pluto {
namespace map {

// Scan a workspace directory and build a knowledge graph.
// Extracts files, includes, classes, functions, and their relationships.
Graph ScanWorkspace(const std::string& root_dir);

}  // namespace map
}  // namespace pluto
