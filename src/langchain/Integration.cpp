#include "Integration.h"

#include <sstream>

namespace pluto {
namespace langchain {

std::unique_ptr<LangChainIntegration> gLangChain;

LangChainIntegration::LangChainIntegration(Provider* provider,
                                            const Config& cfg,
                                            ToolRegistry* tools)
	: fProvider(provider), fCfg(cfg), fTools(tools) {
	fLLM = std::make_shared<ProviderLLM>(provider, cfg, tools);
}

std::shared_ptr<LLMChain> LangChainIntegration::create_llm_chain(
	const std::string& template_str,
	const std::vector<std::string>& input_variables,
	const std::string& output_key) {
	PromptTemplate prompt(template_str, input_variables);
	return std::make_shared<LLMChain>(fLLM, prompt, output_key);
}

std::shared_ptr<SequentialChain> LangChainIntegration::create_sequential_chain(
	const std::vector<std::shared_ptr<Chain>>& chains,
	const std::vector<std::string>& input_variables,
	const std::vector<std::string>& output_variables) {
	return std::make_shared<SequentialChain>(
		chains, input_variables, output_variables);
}

std::shared_ptr<ReActAgent> LangChainIntegration::create_react_agent(
	const std::vector<std::string>& tools_to_use,
	int max_iterations) {
	return std::make_shared<ReActAgent>(fLLM, fTools, tools_to_use, max_iterations);
}

std::shared_ptr<ConversationBufferMemory>
LangChainIntegration::create_buffer_memory(size_t max_messages) {
	return std::make_shared<ConversationBufferMemory>(max_messages);
}

std::shared_ptr<VectorStore> LangChainIntegration::create_vector_store(
	const std::string& collection_name) {
	return std::make_shared<VectorStore>(collection_name);
}

std::string LangChainIntegration::run_chain(
	std::shared_ptr<Chain> chain,
	const std::map<std::string, std::string>& inputs) {
	return chain->run(inputs);
}

std::string LangChainIntegration::run_agent(std::shared_ptr<Agent> agent,
                                             const std::string& input) {
	return agent->run(input);
}

void LangChainIntegration::register_agent_as_tool(
	std::shared_ptr<Agent> agent,
	const std::string& name,
	const std::string& description) {
	auto tool = std::make_unique<AgentTool>(agent, name, description);
	fTools->add(std::move(tool));
}

void init_langchain(Provider* provider,
                    const Config& cfg,
                    ToolRegistry* tools) {
	gLangChain = std::make_unique<LangChainIntegration>(provider, cfg, tools);
}

LangChainIntegration* langchain() {
	return gLangChain.get();
}

}  // namespace langchain
}  // namespace pluto
