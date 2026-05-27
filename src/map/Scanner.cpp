#include "Scanner.h"

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace pluto {
namespace map {

namespace {

bool EndsWith(const std::string& s, const std::string& suffix)
{
	if (suffix.size() > s.size()) return false;
	return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool IsCppFile(const std::string& name)
{
	return EndsWith(name, ".cpp") || EndsWith(name, ".cc")
	    || EndsWith(name, ".cxx") || EndsWith(name, ".c");
}

bool IsCppHeader(const std::string& name)
{
	return EndsWith(name, ".h") || EndsWith(name, ".hpp")
	    || EndsWith(name, ".hxx");
}

bool IsPythonFile(const std::string& name)
{
	return EndsWith(name, ".py");
}

bool IsJsFile(const std::string& name)
{
	return EndsWith(name, ".js") || EndsWith(name, ".ts")
	    || EndsWith(name, ".jsx") || EndsWith(name, ".tsx");
}

// Recursively list source files, skipping hidden dirs and build artifacts.
void ListFiles(const std::string& dir, const std::string& rel,
               std::vector<std::string>& out)
{
	DIR* d = opendir(dir.c_str());
	if (!d) return;

	std::vector<std::string> names;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name[0] == '.') continue;
		names.push_back(name);
	}
	closedir(d);
	std::sort(names.begin(), names.end());

	for (const auto& name : names) {
		std::string full = dir + "/" + name;
		std::string rp = rel.empty() ? name : rel + "/" + name;

		struct stat st;
		if (::stat(full.c_str(), &st) != 0) continue;

		if (S_ISDIR(st.st_mode)) {
			// Skip common non-source directories.
			if (name == "build" || name == "node_modules"
			    || name == "__pycache__" || name == ".git"
			    || name == "pkgroot")
				continue;
			ListFiles(full, rp, out);
		} else if (S_ISREG(st.st_mode)) {
			if (IsCppFile(name) || IsCppHeader(name)
			    || IsPythonFile(name) || IsJsFile(name)
			    || name == "Makefile" || name == "CMakeLists.txt"
			    || EndsWith(name, ".md") || EndsWith(name, ".sh"))
				out.push_back(rp);
		}
	}
}

void ScanCppFile(const std::string& root, const std::string& rel_path,
                 Graph& graph)
{
	std::string file_id = "file:" + rel_path;
	std::string abs_path = root + "/" + rel_path;

	// File node.
	GraphNode fnode;
	fnode.id = file_id;
	fnode.kind = IsCppHeader(rel_path) ? "header" : "file";
	fnode.label = rel_path;
	fnode.file_path = rel_path;
	graph.AddNode(std::move(fnode));

	std::ifstream f(abs_path);
	if (!f) return;

	// Regex patterns.
	std::regex re_include(R"(^\s*#include\s*["<]([^">]+)[">])");
	std::regex re_class(R"(^\s*class\s+(\w+)\s*(?::\s*(?:public|protected|private)\s+(\w+))?)");
	std::regex re_struct(R"(^\s*struct\s+(\w+)\s*(?::\s*(?:public|protected|private)\s+(\w+))?)");
	std::regex re_func(R"(^(?:\w[\w:*&<> ]*\s+)?(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?\{?)");
	std::regex re_namespace(R"(^\s*namespace\s+(\w+))");

	std::string line;
	int lineno = 0;
	std::string current_namespace;

	while (std::getline(f, line)) {
		++lineno;
		std::smatch m;

		// #include
		if (std::regex_search(line, m, re_include)) {
			std::string inc = m[1].str();
			std::string target_id = "file:" + inc;
			graph.AddEdge({file_id, target_id, "includes", ""});
			// Ensure target node exists.
			GraphNode hnode;
			hnode.id = target_id;
			hnode.kind = "header";
			hnode.label = inc;
			hnode.file_path = inc;
			graph.AddNode(std::move(hnode));
			continue;
		}

		// namespace
		if (std::regex_search(line, m, re_namespace)) {
			current_namespace = m[1].str();
			continue;
		}

		// class / struct
		if (std::regex_search(line, m, re_class)
		    || std::regex_search(line, m, re_struct)) {
			std::string cls = m[1].str();
			std::string qualified = current_namespace.empty()
				? cls : current_namespace + "::" + cls;
			std::string cls_id = "class:" + qualified;

			GraphNode cnode;
			cnode.id = cls_id;
			cnode.kind = "class";
			cnode.label = qualified;
			cnode.file_path = rel_path;
			cnode.line = lineno;
			graph.AddNode(std::move(cnode));

			// File defines class.
			graph.AddEdge({file_id, cls_id, "defines", ""});

			// Inheritance.
			if (m.size() > 2 && m[2].matched) {
				std::string base = m[2].str();
				std::string base_id = "class:" + base;
				graph.AddEdge({cls_id, base_id, "inherits", ""});
				GraphNode bnode;
				bnode.id = base_id;
				bnode.kind = "class";
				bnode.label = base;
				graph.AddNode(std::move(bnode));
			}
			continue;
		}
	}
}

void ScanPythonFile(const std::string& root, const std::string& rel_path,
                    Graph& graph)
{
	std::string file_id = "file:" + rel_path;
	std::string abs_path = root + "/" + rel_path;

	GraphNode fnode;
	fnode.id = file_id;
	fnode.kind = "file";
	fnode.label = rel_path;
	fnode.file_path = rel_path;
	graph.AddNode(std::move(fnode));

	std::ifstream f(abs_path);
	if (!f) return;

	std::regex re_import(R"(^\s*(?:from\s+(\S+)\s+)?import\s+(\S+))");
	std::regex re_class(R"(^\s*class\s+(\w+)(?:\((\w+)\))?)");
	std::regex re_def(R"(^\s*def\s+(\w+)\s*\()");

	std::string line;
	int lineno = 0;

	while (std::getline(f, line)) {
		++lineno;
		std::smatch m;

		if (std::regex_search(line, m, re_import)) {
			std::string mod = m[1].matched ? m[1].str() : m[2].str();
			std::string target_id = "module:" + mod;
			graph.AddEdge({file_id, target_id, "imports", ""});
			GraphNode mnode;
			mnode.id = target_id;
			mnode.kind = "module";
			mnode.label = mod;
			graph.AddNode(std::move(mnode));
			continue;
		}

		if (std::regex_search(line, m, re_class)) {
			std::string cls_id = "class:" + m[1].str();
			GraphNode cnode;
			cnode.id = cls_id;
			cnode.kind = "class";
			cnode.label = m[1].str();
			cnode.file_path = rel_path;
			cnode.line = lineno;
			graph.AddNode(std::move(cnode));
			graph.AddEdge({file_id, cls_id, "defines", ""});

			if (m[2].matched) {
				std::string base_id = "class:" + m[2].str();
				graph.AddEdge({cls_id, base_id, "inherits", ""});
			}
			continue;
		}

		if (std::regex_search(line, m, re_def)) {
			std::string fn_id = "func:" + rel_path + ":" + m[1].str();
			GraphNode fn;
			fn.id = fn_id;
			fn.kind = "function";
			fn.label = m[1].str();
			fn.file_path = rel_path;
			fn.line = lineno;
			graph.AddNode(std::move(fn));
			graph.AddEdge({file_id, fn_id, "defines", ""});
		}
	}
}

}  // namespace

Graph ScanWorkspace(const std::string& root_dir, ScanProgress progress)
{
	Graph graph;
	graph.root_dir = root_dir;

	// Collect source files.
	std::vector<std::string> files;
	ListFiles(root_dir, "", files);

	int total = (int)files.size();
	std::fprintf(stderr, "[map] scanning %d files in %s\n",
		total, root_dir.c_str());

	for (int i = 0; i < total; ++i) {
		const auto& rel = files[i];
		if (progress)
			progress(i + 1, total, rel);

		if (IsCppFile(rel) || IsCppHeader(rel))
			ScanCppFile(root_dir, rel, graph);
		else if (IsPythonFile(rel))
			ScanPythonFile(root_dir, rel, graph);
		else {
			// Generic file node (Makefile, .md, .sh, etc.)
			GraphNode fnode;
			fnode.id = "file:" + rel;
			fnode.kind = "file";
			fnode.label = rel;
			fnode.file_path = rel;
			graph.AddNode(std::move(fnode));
		}
	}

	std::fprintf(stderr, "[map] %zu nodes, %zu edges\n",
		graph.Nodes().size(), graph.Edges().size());

	if (progress)
		progress(total, total, "computing layout...");

	// Compute layout.
	float size = std::max(800.0f, (float)graph.Nodes().size() * 8.0f);
	graph.ForceDirectedLayout(120, size, size);

	return graph;
}

}  // namespace map
}  // namespace pluto
