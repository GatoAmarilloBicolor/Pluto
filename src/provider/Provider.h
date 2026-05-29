#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace pluto {

struct ContentBlock {
	enum Type { TEXT, TOOL_USE, TOOL_RESULT, IMAGE, AUDIO, DOCUMENT };
	Type type = TEXT;
	std::string text;         // TEXT, or TOOL_RESULT content payload
	std::string tool_id;      // TOOL_USE (call id) or TOOL_RESULT (refers to tool_use id)
	std::string tool_name;    // TOOL_USE
	std::string tool_input;   // TOOL_USE: raw JSON object string
	bool is_error = false;    // TOOL_RESULT
	std::string image_data;       // IMAGE: base64-encoded image data
	std::string image_media_type; // IMAGE: e.g. "image/png", "image/jpeg"
	// AUDIO: base64-encoded audio data + format ("mp3" or "wav").
	// Supported by OpenAI gpt-4o-audio models. Claude does not support audio.
	std::string audio_data;
	std::string audio_format;     // "mp3" or "wav"
	// DOCUMENT: base64-encoded PDF data. Claude converts pages to images.
	std::string doc_data;
	std::string doc_media_type;   // "application/pdf"
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
	bool use_cache = true;   // emit cache_control on system+tools when true
	// Per-request cancellation flag (optional).  When non-null the provider
	// polls this flag during the HTTP transfer and aborts if it becomes true.
	// nullptr means "not cancellable" — providers must check for nullptr
	// before dereferencing and simply ignore it.
	std::atomic<bool>* cancel = nullptr;
};

struct StreamEvent {
	enum Type {
		TEXT_DELTA,
		TOOL_USE_START,
		TOOL_USE_INPUT_DELTA,
		TOOL_USE_END,
		IMAGE_BLOCK,      // complete image block from model response
		MESSAGE_END,
		ERROR,
		RETRY_INFO,       // non-fatal: provider is retrying (text = description)
	};
	Type type = TEXT_DELTA;
	std::string text;         // TEXT_DELTA
	std::string tool_id;      // TOOL_USE_*
	std::string tool_name;    // TOOL_USE_START
	std::string tool_input;   // TOOL_USE_END (full accumulated JSON) or _INPUT_DELTA (partial)
	std::string image_data;       // IMAGE_BLOCK: base64 data
	std::string image_media_type; // IMAGE_BLOCK: mime type
	std::string stop_reason;  // MESSAGE_END
	std::string error;        // ERROR
	int input_tokens = 0;     // MESSAGE_END
	int output_tokens = 0;    // MESSAGE_END
	int cache_read_tokens = 0;      // MESSAGE_END (prompt caching)
	int cache_creation_tokens = 0;  // MESSAGE_END (prompt caching)
};

using StreamCallback = std::function<void(const StreamEvent&)>;

class Provider {
public:
	virtual ~Provider() = default;
	virtual void stream(const CompletionRequest& req, StreamCallback cb) = 0;
	virtual std::string name() const = 0;
};

}  // namespace pluto
