#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

#include "../agent/Loop.h"
#include "../config.h"
#include "../tools/Tool.h"
#include "../tools/ToolRegistry.h"
#include "Chain.h"
#include "Memory.h"
#include "Agent.h"
#include "VectorStore.h"

namespace pluto {
namespace langchain {

// LangChain integration for Pluto.
// This class bridges LangChain concepts with Pluto's existing agent loop,
// tools, and provider system.
class LangChainIntegration {
public:
	LangChainIntegration(Provider* provider,
	                     const Config& cfg,
	                     ToolRegistry* tools);

	// Create an LLM chain from a prompt template.
	std::shared_ptr<LLMChain> create_llm_chain(
		const std::string& template_str,
		const std::vector<std::string>& input_variables,
		const std::string& output_key = "text");

	// Create a sequential chain from multiple chains.
	std::shared_ptr<SequentialChain> create_sequential_chain(
		const std::vector<std::shared_ptr<Chain>>& chains,
		const std::vector<std::string>& input_variables,
		const std::vector<std::string>& output_variables);

	// Create a ReAct agent with Pluto's tools.
	std::shared_ptr<ReActAgent> create_react_agent(
		const std::vector<std::string>& tools_to_use = {},
		int max_iterations = 10);

	// Create a conversation memory.
	std::shared_ptr<ConversationBufferMemory> create_buffer_memory(
		size_t max_messages = 10);

	// Create a vector store for document retrieval.
	std::shared_ptr<VectorStore> create_vector_store(
		const std::string& collection_name = "pluto_langchain");

	// Run a chain with the given inputs.
	std::string run_chain(std::shared_ptr<Chain> chain,
	                      const std::map<std::string, std::string>& inputs);

	// Run an agent with the given input.
	std::string run_agent(std::shared_ptr<Agent> agent,
	                      const std::string& input);

	// Register an agent as a Pluto tool (callable from the main agent loop).
	void register_agent_as_tool(std::shared_ptr<Agent> agent,
	                            const std::string& name,
	                            const std::string& description);

	// Get the underlying LLM.
	std::shared_ptr<LLM> get_llm() const { return fLLM; }

private:
	std::shared_ptr<ProviderLLM> fLLM;
	Provider* fProvider;
	Config fCfg;
	ToolRegistry* fTools;
};

// Global instance for easy access.
extern std::unique_ptr<LangChainIntegration> gLangChain;

// Initialize LangChain integration (call once at startup).
void init_langchain(Provider* provider,
                    const Config& cfg,
                    ToolRegistry* tools);

// Get the global LangChain integration instance.
LangChainIntegration* langchain();

}  // namespace langchain
}  // namespace pluto
