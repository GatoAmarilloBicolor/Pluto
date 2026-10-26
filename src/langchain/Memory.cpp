#include "Memory.h"
#include "Chain.h"

#include <sstream>
#include <algorithm>

namespace pluto {
namespace langchain {

std::map<std::string, std::string>
ConversationBufferMemory::load_memory_variables(
	const std::map<std::string, std::string>& /*inputs*/) {
	std::map<std::string, std::string> result;
	result["history"] = buffer();
	return result;
}

void ConversationBufferMemory::save_context(
	const std::map<std::string, std::string>& inputs,
	const std::map<std::string, std::string>& outputs) {
	auto input_it = inputs.find("input");
	auto output_it = outputs.find("output");

	if (input_it != inputs.end()) {
		fMessages.push_back({"human", input_it->second, std::time(nullptr)});
	}
	if (output_it != outputs.end()) {
		fMessages.push_back({"ai", output_it->second, std::time(nullptr)});
	}

	// Trim to max messages.
	while (fMessages.size() > fMaxMessages * 2) {
		fMessages.erase(fMessages.begin());
	}
}

void ConversationBufferMemory::clear() {
	fMessages.clear();
}

std::string ConversationBufferMemory::buffer() const {
	std::ostringstream ss;
	for (const auto& msg : fMessages) {
		ss << msg.role << ": " << msg.content << "\n";
	}
	return ss.str();
}

std::map<std::string, std::string>
ConversationSummaryMemory::load_memory_variables(
	const std::map<std::string, std::string>& /*inputs*/) {
	std::map<std::string, std::string> result;
	std::ostringstream ss;
	if (!fSummary.empty()) {
		ss << "Summary: " << fSummary << "\n";
	}
	for (const auto& [role, content] : fRecentMessages) {
		ss << role << ": " << content << "\n";
	}
	result["history"] = ss.str();
	return result;
}

void ConversationSummaryMemory::save_context(
	const std::map<std::string, std::string>& inputs,
	const std::map<std::string, std::string>& outputs) {
	auto input_it = inputs.find("input");
	auto output_it = outputs.find("output");

	if (input_it != inputs.end()) {
		fRecentMessages.push_back({"human", input_it->second});
	}
	if (output_it != outputs.end()) {
		fRecentMessages.push_back({"ai", output_it->second});
	}

	// If we exceed max messages, summarize older ones.
	if (fRecentMessages.size() > fMaxMessages * 2) {
		// Summarize all but the last 2 messages.
		size_t summarize_count = fRecentMessages.size() - 2;
		std::ostringstream ss;
		for (size_t i = 0; i < summarize_count; i++) {
			ss << fRecentMessages[i].first << ": "
			   << fRecentMessages[i].second << "\n";
		}

		if (fLLM) {
			std::string summary_prompt =
				"Summarize the following conversation concisely:\n" + ss.str() +
				"\nSummary:";
			fSummary = fLLM->predict(summary_prompt);
		}

		fRecentMessages.erase(fRecentMessages.begin(),
		                      fRecentMessages.begin() + summarize_count);
	}
}

void ConversationSummaryMemory::clear() {
	fSummary.clear();
	fRecentMessages.clear();
}

}  // namespace langchain
}  // namespace pluto
