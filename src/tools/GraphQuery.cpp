// Tool: graph_query — query the project knowledge graph for structure,
// dependencies, and relationships between source code entities.

#include "GraphQuery.h"
#include "../json.h"
#include "../map/Graph.h"

#include <cstdio>
#include <string>

namespace pluto {

namespace {

class GraphQueryTool : public Tool {
public:
	std::string name() const override { return "graph_query"; }

	std::string description() const override {
		return "Query the project knowledge graph built by /map. The graph "
		       "contains nodes (files, headers, classes, functions, namespaces) "
		       "and edges (includes, defines, inherits, calls, depends_on). "
		       "Use this to understand project structure, find dependencies, "
		       "trace relationships, and identify critical files. "
		       "The graph must have been built first (via /map or the GUI).";
	}

	std::string input_schema_json() const override {
		return R"JSON({
  "type": "object",
  "properties": {
    "action": {
      "type": "string",
      "enum": ["summary", "find", "edges", "path", "top"],
      "description": "Action to perform:\n- summary: overview of the entire graph (node/edge counts, top files, orphans)\n- find: search nodes by name pattern\n- edges: list all edges for a specific node\n- path: find shortest path between two nodes\n- top: list most-connected nodes"
    },
    "query": {
      "type": "string",
      "description": "For 'find': search pattern (substring match on node id/label). For 'edges': exact node id. For 'path': not used (use 'from' and 'to' instead)."
    },
    "from": {
      "type": "string",
      "description": "For 'path': starting node id or label substring."
    },
    "to": {
      "type": "string",
      "description": "For 'path': destination node id or label substring."
    },
    "limit": {
      "type": "integer",
      "description": "Max results to return. Default: 20."
    }
  },
  "required": ["action"]
})JSON";
	}

	std::string preview(const std::string& input_json) const override {
		try {
			json::Value v = json::Value::parse(input_json);
			std::string action = v.str("action");
			std::string query = v.str("query");
			if (!query.empty())
				return "graph_query(" + action + ", " + query + ")";
			return "graph_query(" + action + ")";
		} catch (...) {
			return "graph_query(?)";
		}
	}

	ToolResult execute(const std::string& input_json) override {
		ToolResult tr;
		try {
			json::Value v = json::Value::parse(input_json);
			std::string action = v.str("action");
			int limit = 20;
			{
				const json::Value* lv = v.find("limit");
				if (lv && lv->is_num())
					limit = (int)lv->as_num();
			}
			if (limit < 1) limit = 1;
			if (limit > 100) limit = 100;

			// Load graph from workspace.
			map::Graph graph = map::Graph::LoadFromFile("graph.json");
			if (graph.Nodes().empty()) {
				tr.is_error = true;
				tr.output = "No graph found. Run /map first to scan the workspace.";
				return tr;
			}

			if (action == "summary") {
				tr.output = graph.Summary();
			}
			else if (action == "find") {
				std::string query = v.str("query");
				if (query.empty()) {
					tr.is_error = true;
					tr.output = "Parameter 'query' is required for action 'find'.";
					return tr;
				}
				auto found = graph.FindNodes(query);
				if (found.empty()) {
					tr.output = "No nodes matching '" + query + "'.";
					return tr;
				}
				std::string out = "Found " + std::to_string(found.size())
				                + " nodes matching '" + query + "':\n\n";
				int count = 0;
				for (const auto* n : found) {
					if (++count > limit) {
						out += "... (" + std::to_string(found.size() - limit)
						     + " more)\n";
						break;
					}
					int degree = (int)graph.EdgesFor(n->id).size();
					out += "  " + n->id + "  (" + n->kind + ", "
					     + std::to_string(degree) + " edges)";
					if (!n->file_path.empty()) {
						out += "  " + n->file_path;
						if (n->line > 0)
							out += ":" + std::to_string(n->line);
					}
					out += "\n";
				}
				tr.output = out;
			}
			else if (action == "edges") {
				std::string query = v.str("query");
				if (query.empty()) {
					tr.is_error = true;
					tr.output = "Parameter 'query' is required for action 'edges'.";
					return tr;
				}
				// Try exact match first, then fuzzy.
				auto edges = graph.EdgesFor(query);
				std::string node_id = query;
				if (edges.empty()) {
					auto found = graph.FindNodes(query);
					if (!found.empty()) {
						node_id = found[0]->id;
						edges = graph.EdgesFor(node_id);
					}
				}
				if (edges.empty()) {
					tr.output = "No edges found for '" + query + "'.";
					return tr;
				}
				std::string out = "Edges for " + node_id + " ("
				                + std::to_string(edges.size()) + "):\n\n";
				int count = 0;
				for (const auto* e : edges) {
					if (++count > limit) {
						out += "... (" + std::to_string(edges.size() - limit)
						     + " more)\n";
						break;
					}
					std::string other = (e->source == node_id)
						? e->target : e->source;
					std::string dir = (e->source == node_id) ? " -> " : " <- ";
					out += "  " + e->kind + dir + other + "\n";
				}
				tr.output = out;
			}
			else if (action == "path") {
				std::string from = v.str("from");
				std::string to = v.str("to");
				if (from.empty() || to.empty()) {
					tr.is_error = true;
					tr.output = "Parameters 'from' and 'to' are required for action 'path'.";
					return tr;
				}
				// Resolve fuzzy names to node ids.
				auto from_nodes = graph.FindNodes(from);
				auto to_nodes = graph.FindNodes(to);
				if (from_nodes.empty()) {
					tr.output = "No node matching '" + from + "'.";
					return tr;
				}
				if (to_nodes.empty()) {
					tr.output = "No node matching '" + to + "'.";
					return tr;
				}
				auto path = graph.PathBetween(
					from_nodes[0]->id, to_nodes[0]->id);
				if (path.empty()) {
					tr.output = "No path found between '"
					          + from_nodes[0]->id + "' and '"
					          + to_nodes[0]->id + "'.";
					return tr;
				}
				std::string out = "Path (" + std::to_string(path.size())
				                + " steps):\n\n";
				for (size_t i = 0; i < path.size(); ++i) {
					out += "  " + std::to_string(i + 1) + ". " + path[i];
					if (i + 1 < path.size()) {
						// Find the edge between this and next.
						for (const auto& e : graph.Edges()) {
							if ((e.source == path[i] && e.target == path[i + 1])
							 || (e.source == path[i + 1] && e.target == path[i])) {
								out += "  --[" + e.kind + "]--> ";
								break;
							}
						}
					}
					out += "\n";
				}
				tr.output = out;
			}
			else if (action == "top") {
				auto top = graph.TopNodes(limit);
				if (top.empty()) {
					tr.output = "Graph is empty.";
					return tr;
				}
				std::string out = "Top " + std::to_string(top.size())
				                + " most-connected nodes:\n\n";
				for (size_t i = 0; i < top.size(); ++i) {
					const auto* n = top[i].first;
					int degree = top[i].second;
					out += "  " + std::to_string(i + 1) + ". "
					     + n->id + "  (" + n->kind + ", "
					     + std::to_string(degree) + " edges)\n";
				}
				tr.output = out;
			}
			else {
				tr.is_error = true;
				tr.output = "Unknown action '" + action
				          + "'. Use: summary, find, edges, path, top.";
			}
		} catch (const std::exception& e) {
			tr.is_error = true;
			tr.output = std::string("graph_query error: ") + e.what();
		}
		return tr;
	}
};

}  // namespace

std::unique_ptr<Tool> MakeGraphQueryTool() {
	return std::make_unique<GraphQueryTool>();
}

}  // namespace pluto
