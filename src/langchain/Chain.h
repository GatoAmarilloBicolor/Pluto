#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "PromptTemplate.h"

namespace pluto {
namespace langchain {

#include "LLM.h"

// Base class for all chains.
// A chain is a sequence of steps that processes input and produces output.
class Chain {
public:
	virtual ~Chain() = default;

	// Run the chain with the given inputs.
	// Returns the output string.
	virtual std::string run(const std::map<std::string, std::string>& inputs) = 0;

	// Run the chain with a single string input.
	// The input is treated as the "input" variable.
	virtual std::string run(const std::string& input) {
		std::map<std::string, std::string> inputs;
		inputs["input"] = input;
		return run(inputs);
	}

	// Get the names of input keys this chain expects.
	virtual std::vector<std::string> input_keys() const = 0;

	// Get the names of output keys this chain produces.
	virtual std::vector<std::string> output_keys() const = 0;
};

// A simple chain that formats a prompt and runs it through an LLM.
// This is the most common chain type.
class LLMChain : public Chain {
public:
	LLMChain(std::shared_ptr<LLM> llm,
	         PromptTemplate prompt,
	         std::string output_key = "text");

	std::string run(const std::map<std::string, std::string>& inputs) override;

	std::vector<std::string> input_keys() const override;
	std::vector<std::string> output_keys() const override;

private:
	std::shared_ptr<LLM> fLLM;
	PromptTemplate fPrompt;
	std::string fOutputKey;
};

// Chains multiple chains together sequentially.
// The output of one chain becomes the input of the next.
class SequentialChain : public Chain {
public:
	SequentialChain(std::vector<std::shared_ptr<Chain>> chains,
	                std::vector<std::string> input_variables,
	                std::vector<std::string> output_variables);

	std::string run(const std::map<std::string, std::string>& inputs) override;

	std::vector<std::string> input_keys() const override;
	std::vector<std::string> output_keys() const override;

private:
	std::vector<std::shared_ptr<Chain>> fChains;
	std::vector<std::string> fInputVariables;
	std::vector<std::string> fOutputVariables;
};

}  // namespace langchain
}  // namespace pluto
