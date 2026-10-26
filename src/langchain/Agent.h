#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>

#include "../tools/Tool.h"
#include "../agent/Loop.h"

namespace pluto {
namespace langchain {

// An agent that uses a language model to choose a sequence of actions.
// Implements the ReAct (Reason + Act) pattern:
//   Thought: reason about what to do
//   Action: call a tool
//   Observation: observe the result
//   ... repeat until finished
class Agent {
public:
	virtual ~Agent() = default;

	// Run the agent on an input string.
	// Returns the final answer string.
	virtual std::string run(const std::string& input) = 0;

	// Get the input keys this agent expects.
	virtual std::vector<std::string> input_keys() const = 0;
};

// ReAct agent: reason, act, observe loop.
// Uses an LLM to decide which tool to call next based on the input
// and previous observations.
class ReActAgent : public Agent {
public:
	ReActAgent(std::shared_ptr<langchain::LLM> llm,
	           ToolRegistry* tools,
	           std::vector<std::string> tools_to_use = {},
	           int max_iterations = 10);

	std::string run(const std::string& input) override;
	std::vector<std::string> input_keys() const override;

private:
	std::shared_ptr<langchain::LLM> fLLM;
	ToolRegistry* fTools;
	std::vector<std::string> fToolsToUse;
	int fMaxIterations;

	// Build the ReAct prompt from available tools.
	std::string build_prompt(const std::string& input,
	                         const std::string& history) const;

	// Parse the LLM response to extract action and input.
	bool parse_action(const std::string& response,
	                  std::string& tool_name,
	                  std::string& tool_input) const;
};

// Zero-shot agent: decides which tool to use without examples.
// Good for simple tool-use scenarios.
class ZeroShotAgent : public ReActAgent {
public:
	using ReActAgent::ReActAgent;
};

// Structured chat agent: uses a more formal prompt format.
class StructuredChatAgent : public ReActAgent {
public:
	using ReActAgent::ReActAgent;
};

// Create an agent from tools and an LLM.
std::shared_ptr<Agent> create_react_agent(
	std::shared_ptr<langchain::LLM> llm,
	ToolRegistry* tools,
	std::vector<std::string> tools_to_use = {},
	int max_iterations = 10);

std::shared_ptr<Agent> create_zero_shot_agent(
	std::shared_ptr<langchain::LLM> llm,
	ToolRegistry* tools,
	std::vector<std::string> tools_to_use = {},
	int max_iterations = 10);

// Run an agent as a Pluto tool (integrates with the existing tool registry).
// This allows LangChain agents to be called from within Pluto's agent loop.
class AgentTool : public Tool {
public:
	AgentTool(std::shared_ptr<Agent> agent,
	          std::string name,
	          std::string description,
	          std::string input_schema = "{}");

	std::string name() const override { return fName; }
	std::string description() const override { return fDescription; }
	std::string input_schema_json() const override { return fInputSchema; }
	std::string preview(const std::string& input_json) const override;
	ToolResult execute(const std::string& input_json) override;

private:
	std::string fName;
	std::string fDescription;
	std::string fInputSchema;
	std::shared_ptr<Agent> fAgent;
};

}  // namespace langchain
}  // namespace pluto
