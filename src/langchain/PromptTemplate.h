#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pluto {
namespace langchain {

// A prompt template with input variables that get substituted at runtime.
// Example: "You are a helpful assistant. Task: {task}"
// Variables are substituted using {variable_name} syntax.
class PromptTemplate {
public:
	PromptTemplate(std::string template_str,
	               std::vector<std::string> input_variables);

	// Format the template with the given values.
	std::string format(const std::map<std::string, std::string>& values) const;

	// Get the list of required input variables.
	const std::vector<std::string>& input_variables() const { return fInputVariables; }

	// Get the raw template string.
	const std::string& template_str() const { return fTemplateStr; }

private:
	std::string fTemplateStr;
	std::vector<std::string> fInputVariables;
};

// Create a prompt template from a string.
// Automatically extracts {variable_name} placeholders as input variables.
PromptTemplate make_prompt(const std::string& template_str);

}  // namespace langchain
}  // namespace pluto
