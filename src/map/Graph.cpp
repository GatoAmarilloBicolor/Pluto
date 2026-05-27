#include "Graph.h"
#include "../json.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace pluto {
namespace map {

void Graph::AddNode(GraphNode node)
{
	// Avoid duplicates.
	for (const auto& n : fNodes)
		if (n.id == node.id) return;
	fNodes.push_back(std::move(node));
}

void Graph::AddEdge(GraphEdge edge)
{
	// Avoid duplicate edges.
	for (const auto& e : fEdges)
		if (e.source == edge.source && e.target == edge.target
		    && e.kind == edge.kind)
			return;
	fEdges.push_back(std::move(edge));
}

std::vector<const GraphNode*>
Graph::FindNodes(const std::string& pattern) const
{
	std::vector<const GraphNode*> result;
	for (const auto& n : fNodes) {
		if (n.id.find(pattern) != std::string::npos
		    || n.label.find(pattern) != std::string::npos)
			result.push_back(&n);
	}
	return result;
}

std::vector<const GraphEdge*>
Graph::EdgesFor(const std::string& node_id) const
{
	std::vector<const GraphEdge*> result;
	for (const auto& e : fEdges) {
		if (e.source == node_id || e.target == node_id)
			result.push_back(&e);
	}
	return result;
}

std::vector<std::string>
Graph::PathBetween(const std::string& from, const std::string& to) const
{
	// BFS on undirected adjacency.
	std::unordered_map<std::string, std::vector<std::string>> adj;
	for (const auto& e : fEdges) {
		adj[e.source].push_back(e.target);
		adj[e.target].push_back(e.source);
	}

	std::unordered_map<std::string, std::string> parent;
	std::queue<std::string> q;
	q.push(from);
	parent[from] = "";

	while (!q.empty()) {
		std::string cur = q.front();
		q.pop();
		if (cur == to) {
			// Reconstruct path.
			std::vector<std::string> path;
			for (std::string n = to; !n.empty(); n = parent[n])
				path.push_back(n);
			std::reverse(path.begin(), path.end());
			return path;
		}
		auto it = adj.find(cur);
		if (it == adj.end()) continue;
		for (const auto& next : it->second) {
			if (parent.count(next)) continue;
			parent[next] = cur;
			q.push(next);
		}
	}
	return {};  // no path
}

std::vector<std::pair<const GraphNode*, int>>
Graph::TopNodes(int limit) const
{
	std::unordered_map<std::string, int> degree;
	for (const auto& e : fEdges) {
		degree[e.source]++;
		degree[e.target]++;
	}

	std::vector<std::pair<const GraphNode*, int>> ranked;
	for (const auto& n : fNodes) {
		auto it = degree.find(n.id);
		ranked.push_back({&n, it != degree.end() ? it->second : 0});
	}
	std::sort(ranked.begin(), ranked.end(),
		[](const auto& a, const auto& b) { return a.second > b.second; });

	if ((int)ranked.size() > limit)
		ranked.resize(limit);
	return ranked;
}

std::string Graph::Summary() const
{
	// Count by kind.
	std::unordered_map<std::string, int> node_kinds, edge_kinds;
	for (const auto& n : fNodes) node_kinds[n.kind]++;
	for (const auto& e : fEdges) edge_kinds[e.kind]++;

	std::ostringstream ss;
	ss << "Graph: " << fNodes.size() << " nodes, "
	   << fEdges.size() << " edges\n";
	ss << "Root: " << root_dir << "\n\n";

	ss << "Node types:\n";
	for (const auto& [kind, count] : node_kinds)
		ss << "  " << kind << ": " << count << "\n";

	ss << "\nEdge types:\n";
	for (const auto& [kind, count] : edge_kinds)
		ss << "  " << kind << ": " << count << "\n";

	auto top = TopNodes(15);
	if (!top.empty()) {
		ss << "\nMost connected nodes:\n";
		for (const auto& [node, degree] : top)
			ss << "  " << node->label << " (" << node->kind
			   << ") — " << degree << " connections\n";
	}

	// Orphans (nodes with 0 edges).
	int orphans = 0;
	std::unordered_set<std::string> connected;
	for (const auto& e : fEdges) {
		connected.insert(e.source);
		connected.insert(e.target);
	}
	for (const auto& n : fNodes)
		if (!connected.count(n.id)) orphans++;
	if (orphans > 0)
		ss << "\nOrphan nodes (no connections): " << orphans << "\n";

	return ss.str();
}

// ---- Force-directed layout (Fruchterman-Reingold) ----

void Graph::ForceDirectedLayout(int iterations, float width, float height)
{
	int n = (int)fNodes.size();
	if (n == 0) return;

	// Scale layout area based on node count for better spacing.
	float scale = std::max(1.0f, std::sqrt((float)n / 30.0f));
	width *= scale;
	height *= scale;

	float area = width * height;
	float k = std::sqrt(area / (float)n) * 1.2f;  // ideal edge length (wider spacing)

	// Initialize with circular layout.
	for (int i = 0; i < n; ++i) {
		float angle = 2.0f * 3.14159f * i / n;
		fNodes[i].x = width / 2 + (width * 0.4f) * std::cos(angle);
		fNodes[i].y = height / 2 + (height * 0.4f) * std::sin(angle);
	}

	// Node index lookup.
	std::unordered_map<std::string, int> idx;
	for (int i = 0; i < n; ++i)
		idx[fNodes[i].id] = i;

	// More iterations for larger graphs.
	int iters = std::max(iterations, (int)(iterations * std::sqrt(scale)));

	std::vector<float> dx(n), dy(n);

	for (int iter = 0; iter < iters; ++iter) {
		float temp = (width / 8.0f) * (1.0f - (float)iter / iters);
		std::fill(dx.begin(), dx.end(), 0.0f);
		std::fill(dy.begin(), dy.end(), 0.0f);

		// Repulsive forces between all pairs.
		for (int i = 0; i < n; ++i) {
			for (int j = i + 1; j < n; ++j) {
				float ex = fNodes[i].x - fNodes[j].x;
				float ey = fNodes[i].y - fNodes[j].y;
				float dist = std::sqrt(ex * ex + ey * ey);
				if (dist < 0.1f) dist = 0.1f;
				float force = k * k / dist;
				float fx = (ex / dist) * force;
				float fy = (ey / dist) * force;
				dx[i] += fx;  dy[i] += fy;
				dx[j] -= fx;  dy[j] -= fy;
			}
		}

		// Attractive forces along edges.
		for (const auto& e : fEdges) {
			auto si = idx.find(e.source);
			auto ti = idx.find(e.target);
			if (si == idx.end() || ti == idx.end()) continue;
			int s = si->second, t = ti->second;
			float ex = fNodes[s].x - fNodes[t].x;
			float ey = fNodes[s].y - fNodes[t].y;
			float dist = std::sqrt(ex * ex + ey * ey);
			if (dist < 0.1f) dist = 0.1f;
			float force = dist * dist / k;
			// Weaken attraction slightly to prevent clumping.
			force *= 0.8f;
			float fx = (ex / dist) * force;
			float fy = (ey / dist) * force;
			dx[s] -= fx;  dy[s] -= fy;
			dx[t] += fx;  dy[t] += fy;
		}

		// Gravity toward center to prevent disconnected components flying away.
		float cx = width / 2, cy = height / 2;
		float gravity = 0.3f;
		for (int i = 0; i < n; ++i) {
			dx[i] += (cx - fNodes[i].x) * gravity;
			dy[i] += (cy - fNodes[i].y) * gravity;
		}

		// Apply with temperature clamping.
		for (int i = 0; i < n; ++i) {
			float disp = std::sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
			if (disp < 0.01f) continue;
			float s = std::min(disp, temp) / disp;
			fNodes[i].x += dx[i] * s;
			fNodes[i].y += dy[i] * s;
			// Keep within bounds with margin.
			fNodes[i].x = std::max(30.0f, std::min(width - 30.0f, fNodes[i].x));
			fNodes[i].y = std::max(30.0f, std::min(height - 30.0f, fNodes[i].y));
		}
	}
}

// ---- JSON serialization ----

std::string Graph::ToJson() const
{
	json::Value root(json::Object{});
	root.set("root_dir", root_dir);

	json::Value nodes_arr(json::Array{});
	for (const auto& n : fNodes) {
		json::Value obj(json::Object{});
		obj.set("id", n.id);
		obj.set("kind", n.kind);
		obj.set("label", n.label);
		obj.set("file_path", n.file_path);
		obj.set("line", (double)n.line);
		obj.set("x", (double)n.x);
		obj.set("y", (double)n.y);
		nodes_arr.push(std::move(obj));
	}
	root.set("nodes", std::move(nodes_arr));

	json::Value edges_arr(json::Array{});
	for (const auto& e : fEdges) {
		json::Value obj(json::Object{});
		obj.set("source", e.source);
		obj.set("target", e.target);
		obj.set("kind", e.kind);
		if (!e.label.empty())
			obj.set("label", e.label);
		edges_arr.push(std::move(obj));
	}
	root.set("edges", std::move(edges_arr));

	return root.serialize();
}

Graph Graph::FromJson(const std::string& json_str)
{
	Graph g;
	json::Value root = json::Value::parse(json_str);
	g.root_dir = root.str("root_dir");

	const json::Value* nodes = root.find("nodes");
	if (nodes && nodes->is_arr()) {
		for (const auto& v : nodes->as_arr()) {
			GraphNode n;
			n.id = v.str("id");
			n.kind = v.str("kind");
			n.label = v.str("label");
			n.file_path = v.str("file_path");
			const json::Value* vl = v.find("line");
			if (vl && vl->is_num()) n.line = (int)vl->as_num();
			const json::Value* vx = v.find("x");
			if (vx && vx->is_num()) n.x = (float)vx->as_num();
			const json::Value* vy = v.find("y");
			if (vy && vy->is_num()) n.y = (float)vy->as_num();
			g.fNodes.push_back(std::move(n));
		}
	}

	const json::Value* edges = root.find("edges");
	if (edges && edges->is_arr()) {
		for (const auto& v : edges->as_arr()) {
			GraphEdge e;
			e.source = v.str("source");
			e.target = v.str("target");
			e.kind = v.str("kind");
			e.label = v.str("label");
			g.fEdges.push_back(std::move(e));
		}
	}

	return g;
}

bool Graph::SaveToFile(const std::string& path) const
{
	std::ofstream f(path);
	if (!f) return false;
	f << ToJson();
	return f.good();
}

Graph Graph::LoadFromFile(const std::string& path)
{
	std::ifstream f(path);
	if (!f) return {};
	std::ostringstream ss;
	ss << f.rdbuf();
	return FromJson(ss.str());
}

}  // namespace map
}  // namespace pluto
