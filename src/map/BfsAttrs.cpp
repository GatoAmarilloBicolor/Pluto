#include "BfsAttrs.h"

#ifdef __HAIKU__
#include <Node.h>
#include <TypeConstants.h>
#endif

#include <cstdio>
#include <ctime>
#include <unordered_map>
#include <unordered_set>

namespace pluto {
namespace map {

#ifdef __HAIKU__

int StampBfsAttributes(const Graph& graph)
{
	// Build per-file stats.
	struct FileStats {
		std::string kind;
		std::string includes;   // comma-separated
		std::string defines;    // comma-separated
		int edges_out = 0;
		int edges_in = 0;
	};

	std::unordered_map<std::string, FileStats> stats;

	// Initialize from file nodes.
	for (const auto& n : graph.Nodes()) {
		if (n.kind == "file" || n.kind == "header") {
			stats[n.file_path].kind = n.kind;
		}
	}

	// Count edges and collect relationship lists.
	for (const auto& e : graph.Edges()) {
		// Extract file paths from node IDs.
		// "file:src/main.cpp" -> "src/main.cpp"
		auto file_from_id = [](const std::string& id) -> std::string {
			if (id.compare(0, 5, "file:") == 0) return id.substr(5);
			return {};
		};

		std::string src_file = file_from_id(e.source);
		std::string tgt_file = file_from_id(e.target);

		if (!src_file.empty() && stats.count(src_file)) {
			stats[src_file].edges_out++;
			if (e.kind == "includes") {
				auto& inc = stats[src_file].includes;
				if (!inc.empty()) inc += ",";
				// Extract just the filename from the target.
				std::string tgt = e.target;
				if (tgt.compare(0, 5, "file:") == 0) tgt = tgt.substr(5);
				inc += tgt;
			}
		}
		if (!tgt_file.empty() && stats.count(tgt_file))
			stats[tgt_file].edges_in++;

		// Track "defines" edges.
		if (e.kind == "defines" && !src_file.empty() && stats.count(src_file)) {
			std::string target_label = e.target;
			// Strip prefix ("class:", "func:").
			auto colon = target_label.find(':');
			if (colon != std::string::npos)
				target_label = target_label.substr(colon + 1);
			auto& def = stats[src_file].defines;
			if (!def.empty()) def += ",";
			def += target_label;
		}
	}

	int64_t now = (int64_t)std::time(nullptr);
	int stamped = 0;

	for (const auto& [file_path, fs] : stats) {
		std::string abs = graph.root_dir + "/" + file_path;
		BNode node(abs.c_str());
		if (node.InitCheck() != B_OK) continue;

		node.WriteAttr("pluto:graph:kind", B_STRING_TYPE, 0,
			fs.kind.c_str(), fs.kind.size() + 1);

		if (!fs.includes.empty())
			node.WriteAttr("pluto:graph:includes", B_STRING_TYPE, 0,
				fs.includes.c_str(), fs.includes.size() + 1);

		if (!fs.defines.empty())
			node.WriteAttr("pluto:graph:defines", B_STRING_TYPE, 0,
				fs.defines.c_str(), fs.defines.size() + 1);

		node.WriteAttr("pluto:graph:edges_out", B_INT32_TYPE, 0,
			&fs.edges_out, sizeof(int32_t));
		node.WriteAttr("pluto:graph:edges_in", B_INT32_TYPE, 0,
			&fs.edges_in, sizeof(int32_t));
		node.WriteAttr("pluto:graph:scan_time", B_INT64_TYPE, 0,
			&now, sizeof(int64_t));

		stamped++;
	}

	std::fprintf(stderr, "[map] stamped BFS attributes on %d files\n",
		stamped);
	return stamped;
}

#else  // __HAIKU__

int StampBfsAttributes(const Graph& /*graph*/)
{
	std::fprintf(stderr, "[map] BFS attributes not available on this platform\n");
	return 0;
}

#endif  // __HAIKU__

}  // namespace map
}  // namespace pluto
