#ifndef PLUTO_PROVIDER_H
#define PLUTO_PROVIDER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <atomic>
#include <curl/curl.h>

#include "../json.h"
#include "../cancel.h"

namespace pluto {

// Forward declarations
struct CompletionRequest;
struct StreamEvent;

struct ContentBlock {
    enum Type {
        TEXT,
        IMAGE,
        AUDIO,
        DOCUMENT,
        TOOL_USE,
        TOOL_RESULT
    } type = TEXT;

    std::string text;
    std::string tool_id;
    std::string tool_name;
    std::string tool_input;
    std::string base64_data;
    std::string media_type;
    std::string document_id;
    std::string filename;
    std::string image_data;
    std::string image_media_type;
    std::string audio_format;
    std::string audio_data;
    std::string doc_media_type;
    std::string doc_data;
    int tool_index = -1;
    bool is_error = false;
};

struct ToolSpec {
    std::string name;
    std::string description;
    std::string input_schema_json;
};

using ToolDef = ToolSpec;

struct Message {
    std::string role;
    std::vector<ContentBlock> content;
};

// CancelFlag type for cross-platform cancellation
using CancelFlag = std::atomic<bool>;

// =============================================================================
// Provider-agnostic types
// =============================================================================

enum class ProviderType {
    ANTHROPIC,
    OPENAI_COMPAT
};

struct ModelInfo {
    std::string id;
    std::string display_name;
    std::string provider;
    int max_tokens = 0;
    int context_window = 0;
    bool supports_streaming = true;
    bool supports_tools = true;
    bool supports_vision = false;
    bool supports_reasoning = false;
    double input_cost_per_mtoken = 0.0;
    double output_cost_per_mtoken = 0.0;
};

struct CompletionRequest {
    std::string model;
    std::vector<Message> messages;
    std::string system;
    int max_tokens = 4096;
    double temperature = 1.0;
    std::vector<ToolSpec> tools;
    bool use_cache = false;
    bool stream = true;
    std::optional<int> top_k;
    std::optional<double> top_p;
    std::optional<std::string> stop_sequence;
    CancelFlag* cancel = nullptr;
};

struct StreamEvent {
    enum Type {
        TEXT_DELTA,
        IMAGE_BLOCK,
        TOOL_USE_START,
        TOOL_USE_INPUT_DELTA,
        TOOL_USE_END,
        MESSAGE_END,
        USAGE,
        RETRY_INFO,
        ERROR,
        DONE
    } type = TEXT_DELTA;

    std::string text;
    std::string tool_id;
    std::string tool_name;
    std::string tool_input;
    int input_tokens = 0;
    int output_tokens = 0;
    int cache_read_tokens = 0;
    int cache_creation_tokens = 0;
    int tool_index = -1;
    std::string image_data;
    std::string image_media_type;
    std::string stop_reason;
    std::string error;
    json::Value raw;  // Provider-specific raw event for debugging
};

using StreamCallback = std::function<void(const StreamEvent&)>;

struct ProviderCapabilities {
    bool streaming = true;
    bool tools = true;
    bool vision = false;
    bool reasoning = false;
    bool system_prompt = true;
    bool cache_control = false;
    int max_context = 200000;
};

// =============================================================================
// AuthStrategy - pluggable authentication
// =============================================================================

class AuthStrategy {
public:
    virtual ~AuthStrategy() = default;
    virtual void apply_headers(curl_slist** headers) const = 0;
    virtual bool is_valid() const = 0;
    virtual std::string describe() const = 0;
};

class ApiKeyAuth : public AuthStrategy {
public:
    explicit ApiKeyAuth(std::string key, std::string header_name = "x-api-key");
    void apply_headers(curl_slist** headers) const override;
    bool is_valid() const override;
    std::string describe() const override;
private:
    std::string api_key_;
    std::string header_name_;
};

class BearerTokenAuth : public AuthStrategy {
public:
    explicit BearerTokenAuth(std::string token);
    void apply_headers(curl_slist** headers) const override;
    bool is_valid() const override;
    std::string describe() const override;
private:
    std::string token_;
};

class NoAuth : public AuthStrategy {
public:
    void apply_headers(curl_slist** headers) const override;
    bool is_valid() const override { return true; }
    std::string describe() const override { return "none"; }
};

// =============================================================================
// HttpClient - pluggable HTTP transport
// =============================================================================

class HttpClient {
public:
    virtual ~HttpClient() = default;

    struct Response {
        long http_code = 0;
        std::string body;
        std::string error;
        bool success() const { return http_code >= 200 && http_code < 300; }
    };

    struct StreamOptions {
        StreamCallback on_event;
        CancelFlag* cancel = nullptr;
        int max_retries = 3;
        std::chrono::milliseconds base_delay{1000};
    };

    // Streaming POST with SSE parsing
    virtual void stream_post(const std::string& url,
                             const std::string& body,
                             const AuthStrategy& auth,
                             const StreamOptions& opts) = 0;

    // Simple POST (non-streaming)
    virtual Response post(const std::string& url,
                          const std::string& body,
                          const AuthStrategy& auth,
                          int timeout_seconds = 30) = 0;

    // GET request
    virtual Response get(const std::string& url,
                         const AuthStrategy& auth,
                         int timeout_seconds = 30) = 0;
};

// Default libcurl implementation
std::unique_ptr<HttpClient> create_curl_client();

// =============================================================================
// StreamParser - pluggable SSE parsing
// =============================================================================

class StreamParser {
public:
    virtual ~StreamParser() = default;
    virtual void feed(const char* data, size_t len, StreamCallback cb) = 0;
    virtual void flush(StreamCallback cb) = 0;
    virtual void reset() = 0;
};

// =============================================================================
// Provider - unified interface
// =============================================================================

class Provider {
public:
    virtual ~Provider() = default;

    virtual ProviderType type() const = 0;
    virtual std::string name() const = 0;
    virtual ProviderCapabilities capabilities() const = 0;
    virtual std::vector<ModelInfo> list_models() const = 0;
    virtual bool supports_model(const std::string& model) const = 0;

    // Streaming completion (legacy method name for compatibility)
    virtual void stream(const CompletionRequest& req, StreamCallback cb) = 0;
    virtual void complete(const CompletionRequest& req, StreamCallback cb) {
        stream(req, cb);
    }

    // Non-streaming completion (for simple queries)
    virtual std::string complete_sync(const CompletionRequest& req) = 0;

    // Validate configuration
    virtual bool validate_config(std::string& error_out) const = 0;

    // Get/set base URL (for local/self-hosted models)
    virtual std::string base_url() const = 0;
    virtual void set_base_url(const std::string& url) = 0;

    // Token counting (approximate)
    virtual int estimate_tokens(const std::string& text) const = 0;

    // Cost estimation
    virtual double estimate_cost(const std::string& model, int input_tokens, int output_tokens) const = 0;
};

// =============================================================================
// ProviderFactory - creation & configuration
// =============================================================================

struct ProviderConfig {
    ProviderType type = ProviderType::ANTHROPIC;
    std::string api_key;
    std::string base_url;
    std::string model;
    int max_tokens = 4096;
    bool use_cache = false;
    json::Value extra;  // Provider-specific config
    // Anthropic-specific
    enum class AuthType { API_KEY, OAUTH };
    AuthType auth_type = AuthType::API_KEY;
};

class ProviderConfigFactory {
public:
    static std::unique_ptr<Provider> create(const ProviderConfig& config);
    static std::vector<ProviderType> available_types();
    static std::string type_to_string(ProviderType type);
    static ProviderType string_to_type(const std::string& s);
};

}  // namespace pluto

#endif // PLUTO_PROVIDER_H