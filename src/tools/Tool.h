#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pluto {

struct ToolResult {
	std::string output;
	bool is_error = false;
};

class Tool {
public:
	virtual ~Tool() = default;
	virtual std::string name() const = 0;
	virtual std::string description() const = 0;
	virtual std::string input_schema_json() const = 0;

	// One-line human preview for permission prompts. Default: name(args).
	virtual std::string preview(const std::string& input_json) const;

	virtual ToolResult execute(const std::string& input_json) = 0;
};

class ToolRegistry {
public:
	void add(std::unique_ptr<Tool> t);
	Tool* find(const std::string& name);
	std::vector<const Tool*> list() const;
	
	ToolRegistry() = default;
	ToolRegistry(ToolRegistry&& other) noexcept 
		: fTools(std::move(other.fTools)) {}
	ToolRegistry& operator=(ToolRegistry&& other) noexcept {
		if (this != &other) {
			std::lock_guard<std::mutex> lock(fMutex);
			std::lock_guard<std::mutex> lock_other(other.fMutex);
			fTools = std::move(other.fTools);
		}
		return *this;
	}
	ToolRegistry(const ToolRegistry&) = delete;
	ToolRegistry& operator=(const ToolRegistry&) = delete;

private:
	mutable std::mutex fMutex;
	std::map<std::string, std::unique_ptr<Tool>> fTools;
};

}  // namespace pluto
