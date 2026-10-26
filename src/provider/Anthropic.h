#pragma once

#include "Provider.h"

namespace pluto {

enum class AuthType { API_KEY, OAUTH };

// Common HTTP streaming client for provider implementations.
// Handles: curl setup, retry logic, cancellation, SSE parsing.
class HttpStreamClient {
public:
	explicit HttpStreamClient(std::string user_agent)
		: fUserAgent(std::move(user_agent)) {}

	struct RequestOptions {
		std::string url;
		std::string body;
		std::vector<std::string> headers;
		std::atomic<bool>* cancel = nullptr;
	};

	// Perform a streaming POST request with automatic retries.
	// Calls cb for each parsed SSE event. Returns false on unrecoverable error.
	virtual bool stream_post(const RequestOptions& opts, StreamCallback cb) = 0;

protected:
	// Build headers list including auth.
	std::vector<std::string> build_headers(const std::string& token,
	                                       const std::string& auth_type,
	                                       const std::vector<std::string>& extra = {}) const;

	// Default retry policy: exponential backoff with cap.
	// Override in derived classes for provider-specific behavior.
	virtual int max_attempts() const { return 3; }
	virtual long initial_backoff_secs() const { return 1; }
	virtual long max_backoff_secs() const { return 60; }

	// Parse SSE stream. Call cb for each event. Return true if stream ended.
	virtual bool parse_sse_stream(const std::string& data, StreamState& st,
	                              StreamCallback cb) = 0;

	const std::string& user_agent() const { return fUserAgent; }

private:
	std::string fUserAgent;
};

class AnthropicProvider : public Provider {
public:
	AnthropicProvider(std::string token,
	                  AuthType auth_type = AuthType::API_KEY,
	                  std::string base_url = "https://api.anthropic.com");
	void stream(const CompletionRequest& req, StreamCallback cb) override;
	std::string name() const override { return "anthropic"; }

private:
	std::string fToken;
	AuthType    fAuthType;
	std::string fBaseUrl;
};

}  // namespace pluto
