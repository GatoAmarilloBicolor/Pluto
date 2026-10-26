#include "Agent.h"
#include "../json.h"
#include "../util.h"

#include <sstream>

namespace pluto {
namespace langchain {

ReActAgent::ReActAgent(std::shared_ptr<langchain::LLM> llm,
                       ToolRegistry* tools,
                       std::vector<std::string> tools_to_use,
                       int max_iterations)
	: fLLM(std::move(llm)),
	  fTools(tools),
	  fToolsToUse(std::move(tools_to_use)),
	  fMaxIterations(max_iterations) {}

std::string ReActAgent::run(const std::string& input) {
	std::string history;

	for (int i = 0; i < fMaxIterations; i++) {
		// Build the prompt with current input and history.
		std::string prompt = build_prompt(input, history);

		// Get the LLM's response.
		std::string response = fLLM->predict(prompt);

		// Parse the response to see if it's an action or final answer.
		std::string tool_name, tool_input;
		if (parse_action(response, tool_name, tool_input)) {
			// Execute the tool.
			Tool* tool = fTools->find(tool_name);
			if (!tool) {
				history += "Observation: Tool '" + tool_name + "' not found.\n";
				continue;
			}

			ToolResult result = tool->execute(tool_input);
			history += "Observation: " + result.output + "\n";

			if (result.is_error) {
				history += "Observation: Error: " + result.output + "\n";
			}
		} else {
			// It's a final answer.
			return response;
		}
	}

	return "Agent reached max iterations without a final answer.";
}

std::vector<std::string> ReActAgent::input_keys() const {
	return {"input"};
}

std::string ReActAgent::build_prompt(const std::string& input,
                                      const std::string& history) const {
	std::ostringstream ss;

	ss << "You are a helpful AI assistant. You have access to the following tools:\n\n";

	// List available tools.
	auto all_tools = fTools->list();
	for (auto* tool : all_tools) {
		if (!fToolsToUse.empty() &&
		    std::find(fToolsToUse.begin(), fToolsToUse.end(), tool->name())
		    == fToolsToUse.end()) {
			continue;
		}
		ss << "- " << tool->name() << ": " << tool->description() << "\n";
	}

	ss << "\nUse the following format:\n";
	ss << "Question: the input question you must answer\n";
	ss << "Thought: you should always think about what to do\n";
	ss << "Action: the tool to use, should be one of [";

	bool first = true;
	for (auto* tool : all_tools) {
		if (!fToolsToUse.empty() &&
		    std::find(fToolsToUse.begin(), fToolsToUse.end(), tool->name())
		    == fToolsToUse.end()) {
			continue;
		}
		if (!first) ss << ", ";
		ss << tool->name();
		first = false;
	}
	ss << "]\n";
	ss << "Action Input: the input to the tool\n";
	ss << "Observation: the result of the tool\n";
	ss << "... (this Thought/Action/Action Input/Observation can repeat N times)\n";
	ss << "Thought: I now know the final answer\n";
	ss << "Final Answer: the final answer to the original question\n\n";

	ss << "History:\n" << history << "\n";
	ss << "Question: " << input << "\n";
	ss << "Thought:";

	return ss.str();
}

bool ReActAgent::parse_action(const std::string& response,
                               std::string& tool_name,
                               std::string& tool_input) const {
	// Look for "Action: tool_name" and "Action Input: input"
	size_t action_pos = response.find("Action:");
	if (action_pos == std::string::npos) return false;

	size_t input_pos = response.find("Action Input:");
	if (input_pos == std::string::npos) return false;

	// Extract tool name.
	size_t name_start = action_pos + 7;
	size_t name_end = response.find('\n', name_start);
	if (name_end == std::string::npos) name_end = response.size();
	tool_name = response.substr(name_start, name_end - name_start);
	// Trim whitespace.
	while (!tool_name.empty() && std::isspace(tool_name.back()))
		tool_name.pop_back();
	while (!tool_name.empty() && std::isspace(tool_name.front()))
		tool_name.erase(0, 1);

	// Extract tool input.
	size_t input_start = input_pos + 13;
	size_t input_end = response.find('\n', input_start);
	if (input_end == std::string::npos) input_end = response.size();
	tool_input = response.substr(input_start, input_end - input_start);
	// Trim whitespace.
	while (!tool_input.empty() && std::isspace(tool_input.back()))
		tool_input.pop_back();
	while (!tool_input.empty() && std::isspace(tool_input.front()))
		tool_input.erase(0, 1);

	return true;
}

std::shared_ptr<Agent> create_react_agent(
	std::shared_ptr<langchain::LLM> llm,
	ToolRegistry* tools,
	std::vector<std::string> tools_to_use,
	int max_iterations) {
	return std::make_shared<ReActAgent>(llm, tools, tools_to_use, max_iterations);
}

std::shared_ptr<Agent> create_zero_shot_agent(
	std::shared_ptr<langchain::LLM> llm,
	ToolRegistry* tools,
	std::vector<std::string> tools_to_use,
	int max_iterations) {
	return std::make_shared<ZeroShotAgent>(llm, tools, tools_to_use, max_iterations);
}

// ---------- AgentTool ----------

AgentTool::AgentTool(std::shared_ptr<Agent> agent,
                     std::string name,
                     std::string description,
                     std::string input_schema)
	: fName(std::move(name)),
	  fDescription(std::move(description)),
	  fInputSchema(std::move(input_schema)),
	  fAgent(std::move(agent)) {}

std::string AgentTool::preview(const std::string& input_json) const {
	return fName + " " + input_json.substr(0, 80);
}

ToolResult AgentTool::execute(const std::string& input_json) {
	ToolResult r;
	try {
		// Extract the input string from JSON.
		std::string input;
		size_t pos = input_json.find("\"input\"");
		if (pos != std::string::npos) {
			pos = input_json.find(':', pos);
			if (pos != std::string::npos) {
				pos++;
				while (pos < input_json.size() &&
				       (input_json[pos] == ' ' || input_json[pos] == '\t'))
					pos++;
				if (pos < input_json.size() && input_json[pos] == '"') {
					pos++;
					size_t end = input_json.find('"', pos);
					if (end != std::string::npos) {
						input = input_json.substr(pos, end - pos);
					}
				}
			}
		}

		if (input.empty()) input = input_json;

		std::string result = fAgent->run(input);
		r.output = result;
	} catch (const std::exception& e) {
		r.is_error = true;
		r.output = e.what();
	}
	return r;
}

}  // namespace langchain
}  // namespace pluto
