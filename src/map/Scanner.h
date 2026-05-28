#pragma once

#include "Graph.h"
#include <atomic>
#include <functional>
#include <string>

namespace pluto {
namespace map {

// Progress callback: (current_file_index, total_files, current_filename).
using ScanProgress = std::function<void(int current, int total,
                                         const std::string& file)>;

// Scan a workspace directory and build a knowledge graph.
// Extracts files, includes, classes, functions, and their relationships.
// The optional progress callback is invoked for each file processed.
// If a partial graph.json exists, resumes from where it left off.
// The graph is auto-saved every `save_interval` files (0 = only at end).
// If cancel is non-null, the scan stops when *cancel becomes true.
Graph ScanWorkspace(const std::string& root_dir,
                    ScanProgress progress = nullptr,
                    int save_interval = 50,
                    std::atomic<bool>* cancel = nullptr);

}  // namespace map
}  // namespace pluto
