#pragma once

#include <set>
#include <string>
#include <vector>

namespace pluto {
namespace map {

struct GraphNode {
	std::string id;         // e.g. "file:src/main.cpp", "class:MainWindow"
	std::string kind;       // "file", "class", "function", "header", "namespace"
	std::string label;      // display name
	std::string file_path;  // source file
	int line = 0;
	float x = 0, y = 0;    // layout position
};

struct GraphEdge {
	std::string source;     // node id
	std::string target;     // node id
	std::string kind;       // "includes", "calls", "defines", "inherits", "depends_on"
	std::string label;      // optional
};

class Graph {
public:
	void AddNode(GraphNode node);
	void AddEdge(GraphEdge edge);

	const std::vector<GraphNode>& Nodes() const { return fNodes; }
	const std::vector<GraphEdge>& Edges() const { return fEdges; }

	// Find nodes matching a substring in id or label.
	std::vector<const GraphNode*> FindNodes(const std::string& pattern) const;

	// Edges connected to a node (in or out).
	std::vector<const GraphEdge*> EdgesFor(const std::string& node_id) const;

	// Shortest path (BFS) between two nodes. Returns node IDs.
	std::vector<std::string> PathBetween(const std::string& from,
	                                      const std::string& to) const;

	// Most-connected nodes, sorted descending by total edges.
	std::vector<std::pair<const GraphNode*, int>> TopNodes(int limit = 20) const;

	// Generate a condensed text summary for LLM context.
	std::string Summary() const;

	// Fruchterman-Reingold force-directed layout.
	void ForceDirectedLayout(int iterations = 100, float width = 1000,
	                          float height = 1000);

	// JSON serialization.
	std::string ToJson() const;
	static Graph FromJson(const std::string& json_str);

	// Save/load to file.
	bool SaveToFile(const std::string& path) const;
	static Graph LoadFromFile(const std::string& path);

	std::string root_dir;

private:
	std::vector<GraphNode> fNodes;
	std::vector<GraphEdge> fEdges;
};

}  // namespace map
}  // namespace pluto
