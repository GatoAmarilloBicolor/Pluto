#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <ctime>

namespace pluto {
namespace langchain {

// Base class for memory.
// Memory stores and retrieves information across chain runs.
class Memory {
public:
	virtual ~Memory() = default;

	// Load memory variables given input.
	// Returns a map of variable names to values.
	virtual std::map<std::string, std::string> load_memory_variables(
		const std::map<std::string, std::string>& inputs) = 0;

	// Save context (input and output) to memory.
	virtual void save_context(
		const std::map<std::string, std::string>& inputs,
		const std::map<std::string, std::string>& outputs) = 0;

	// Clear all memory.
	virtual void clear() = 0;
};

// Conversation memory that stores the last N messages.
// Uses a simple sliding window buffer.
class ConversationBufferMemory : public Memory {
public:
	explicit ConversationBufferMemory(size_t max_messages = 10)
		: fMaxMessages(max_messages) {}

	std::map<std::string, std::string> load_memory_variables(
		const std::map<std::string, std::string>& inputs) override;

	void save_context(
		const std::map<std::string, std::string>& inputs,
		const std::map<std::string, std::string>& outputs) override;

	void clear() override;

	// Get the conversation history as a formatted string.
	std::string buffer() const;

	// Add a message to the buffer.
	void add_message(const std::string& role, const std::string& content);

private:
	size_t fMaxMessages;
	struct Message {
		std::string role;
		std::string content;
		std::time_t timestamp;
	};
	std::vector<Message> fMessages;
};

// Conversation memory with a sliding window and summary.
// Summarizes older messages when the buffer exceeds max size.
class ConversationSummaryMemory : public Memory {
public:
	ConversationSummaryMemory(std::shared_ptr<class LLM> llm,
	                          size_t max_messages = 10)
		: fLLM(std::move(llm)), fMaxMessages(max_messages) {}

	std::map<std::string, std::string> load_memory_variables(
		const std::map<std::string, std::string>& inputs) override;

	void save_context(
		const std::map<std::string, std::string>& inputs,
		const std::map<std::string, std::string>& outputs) override;

	void clear() override;

private:
	std::shared_ptr<class LLM> fLLM;
	size_t fMaxMessages;
	std::string fSummary;
	std::vector<std::pair<std::string, std::string>> fRecentMessages;
};

}  // namespace langchain
}  // namespace pluto
