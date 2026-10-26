#include "PromptTemplate.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace pluto {
namespace langchain {

PromptTemplate::PromptTemplate(std::string template_str,
                                std::vector<std::string> input_variables)
	: fTemplateStr(std::move(template_str)),
	  fInputVariables(std::move(input_variables)) {}

std::string PromptTemplate::format(
	const std::map<std::string, std::string>& values) const {
	std::string result = fTemplateStr;

	// Replace {variable} with the corresponding value.
	for (const auto& [key, val] : values) {
		std::string placeholder = "{" + key + "}";
		size_t pos = 0;
		while ((pos = result.find(placeholder, pos)) != std::string::npos) {
			result.replace(pos, placeholder.size(), val);
			pos += val.size();
		}
	}

	return result;
}

PromptTemplate make_prompt(const std::string& template_str) {
	std::vector<std::string> variables;

	// Extract {variable_name} placeholders.
	std::regex var_regex("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
	std::smatch match;
	std::string s = template_str;
	while (std::regex_search(s, match, var_regex)) {
		variables.push_back(match[1].str());
		s = match.suffix().str();
	}

	return PromptTemplate(template_str, variables);
}

}  // namespace langchain
}  // namespace pluto
