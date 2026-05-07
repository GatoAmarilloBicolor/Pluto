#include "Tool.h"

#include <utility>

namespace pluto {

std::string Tool::preview(const std::string& input_json) const {
	std::string line = name() + "(" + input_json + ")";
	if (line.size() > 200) {
		line.resize(197);
		line += "...";
	}
	return line;
}

void ToolRegistry::add(std::unique_ptr<Tool> t) {
	std::lock_guard<std::mutex> lock(fMutex);
	std::string n = t->name();
	fTools[std::move(n)] = std::move(t);
}

Tool* ToolRegistry::find(const std::string& name) {
	std::lock_guard<std::mutex> lock(fMutex);
	auto it = fTools.find(name);
	return it == fTools.end() ? nullptr : it->second.get();
}

std::vector<const Tool*> ToolRegistry::list() const {
	std::lock_guard<std::mutex> lock(fMutex);
	std::vector<const Tool*> out;
	out.reserve(fTools.size());
	for (const auto& [k, v] : fTools) out.push_back(v.get());
	return out;
}

}  // namespace pluto
