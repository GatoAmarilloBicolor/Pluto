#pragma once

#include <functional>
#include <string>
#include <vector>

namespace pluto {

struct ContentBlock {
	enum Type { TEXT, TOOL_USE, TOOL_RESULT };
	Type type = TEXT;
	std::string text;         // TEXT, or TOOL_RESULT content payload
	std::string tool_id;      // TOOL_USE (call id) or TOOL_RESULT (refers to tool_use id)
	std::string tool_name;    // TOOL_USE
	std::string tool_input;   // TOOL_USE: raw JSON object string
	bool is_error = false;    // TOOL_RESULT
};

struct Message {
	std::string role;                  // "user" | "assistant"
	std::vector<ContentBlock> content;
};

struct ToolDef {
	std::string name;
	std::string description;
	std::string input_schema_json;     // raw JSON object string
};

struct CompletionRequest {
	std::string model;
	std::string system;
	std::vector<Message> messages;
	std::vector<ToolDef> tools;
	int max_tokens = 4096;
};

struct StreamEvent {
	enum Type {
		TEXT_DELTA,
		TOOL_USE_START,
		TOOL_USE_INPUT_DELTA,
		TOOL_USE_END,
		MESSAGE_END,
		ERROR,
	};
	Type type = TEXT_DELTA;
	std::string text;         // TEXT_DELTA
	std::string tool_id;      // TOOL_USE_*
	std::string tool_name;    // TOOL_USE_START
	std::string tool_input;   // TOOL_USE_END (full accumulated JSON) or _INPUT_DELTA (partial)
	std::string stop_reason;  // MESSAGE_END
	std::string error;        // ERROR
	int input_tokens = 0;     // MESSAGE_END
	int output_tokens = 0;    // MESSAGE_END
};

using StreamCallback = std::function<void(const StreamEvent&)>;

class Provider {
public:
	virtual ~Provider() = default;
	virtual void stream(const CompletionRequest& req, StreamCallback cb) = 0;
	virtual std::string name() const = 0;
};

}  // namespace pluto
