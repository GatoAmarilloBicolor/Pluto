#include "Chain.h"
#include "../agent/Loop.h"
#include "../provider/Provider.h"
#include "../json.h"

namespace pluto {
namespace langchain {

// Forward: we need an LLM wrapper that uses Pluto's Provider.
class LLM {
public:
	virtual ~LLM() = default;
	virtual std::string predict(const std::string& prompt) = 0;
	virtual std::string predict(const std::string& prompt,
	                            const std::vector<std::string>& stop) = 0;
};

// LLM implementation that wraps Pluto's Provider.
class ProviderLLM : public LLM {
public:
	ProviderLLM(Provider* provider,
	            const Config& cfg,
	            ToolRegistry* tools = nullptr)
		: fProvider(provider), fCfg(cfg), fTools(tools) {}

	std::string predict(const std::string& prompt) override {
		return predict(prompt, {});
	}

	std::string predict(const std::string& prompt,
	                    const std::vector<std::string>& stop) override {
		// Build a simple message and call the provider.
		std::vector<pluto::Message> messages;
		pluto::Message user_msg;
		user_msg.role = "user";
		pluto::ContentBlock tb;
		tb.type = pluto::ContentBlock::TEXT;
		tb.text = prompt;
		user_msg.content.push_back(std::move(tb));
		messages.push_back(std::move(user_msg));

		CompletionRequest req;
		req.model = fCfg.model;
		req.system = fCfg.system_prompt;
		req.messages = messages;
		req.max_tokens = fCfg.max_tokens;
		req.use_cache = fCfg.cache;

		std::string result;
		bool had_error = false;
		fProvider->stream(req, [&](const StreamEvent& ev) {
			if (ev.type == StreamEvent::TEXT_DELTA) {
				result += ev.text;
			} else if (ev.type == StreamEvent::ERROR) {
				had_error = true;
			}
		});

		if (had_error) return "";
		return result;
	}

private:
	Provider* fProvider;
	Config fCfg;
	ToolRegistry* fTools;
};

LLMChain::LLMChain(std::shared_ptr<LLM> llm,
                   PromptTemplate prompt,
                   std::string output_key)
	: fLLM(std::move(llm)),
	  fPrompt(std::move(prompt)),
	  fOutputKey(std::move(output_key)) {}

std::string LLMChain::run(const std::map<std::string, std::string>& inputs) {
	std::string prompt = fPrompt.format(inputs);
	return fLLM->predict(prompt);
}

std::vector<std::string> LLMChain::input_keys() const {
	return fPrompt.input_variables();
}

std::vector<std::string> LLMChain::output_keys() const {
	return {fOutputKey};
}

SequentialChain::SequentialChain(
	std::vector<std::shared_ptr<Chain>> chains,
	std::vector<std::string> input_variables,
	std::vector<std::string> output_variables)
	: fChains(std::move(chains)),
	  fInputVariables(std::move(input_variables)),
	  fOutputVariables(std::move(output_variables)) {}

std::string SequentialChain::run(
	const std::map<std::string, std::string>& inputs) {
	std::map<std::string, std::string> current_inputs = inputs;

	for (auto& chain : fChains) {
		std::string output = chain->run(current_inputs);

		// Add output to current inputs for the next chain.
		// For simplicity, we store the last output under "output" key.
		current_inputs["output"] = output;
	}

	// Return the last output.
	auto it = current_inputs.find("output");
	if (it != current_inputs.end()) return it->second;
	return "";
}

std::vector<std::string> SequentialChain::input_keys() const {
	return fInputVariables;
}

std::vector<std::string> SequentialChain::output_keys() const {
	return fOutputVariables;
}

}  // namespace langchain
}  // namespace pluto
