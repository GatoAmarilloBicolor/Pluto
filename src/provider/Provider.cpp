#include "Provider.h"
#include "CurlUtil.h"
#include "Anthropic.h"
#include "OpenAICompat.h"
#include "../log.h"
#include "../util.h"

#include <algorithm>
#include <sstream>

namespace pluto {

// =============================================================================
// AuthStrategy implementations
// =============================================================================

ApiKeyAuth::ApiKeyAuth(std::string key, std::string header_name)
    : api_key_(std::move(key)), header_name_(std::move(header_name)) {}

void ApiKeyAuth::apply_headers(curl_slist** headers) const {
    if (!api_key_.empty()) {
        std::string hdr = header_name_ + ": " + api_key_;
        *headers = curl_slist_append(*headers, hdr.c_str());
    }
}

bool ApiKeyAuth::is_valid() const { return !api_key_.empty(); }
std::string ApiKeyAuth::describe() const { return "api_key(" + header_name_ + ")"; }

BearerTokenAuth::BearerTokenAuth(std::string token) : token_(std::move(token)) {}

void BearerTokenAuth::apply_headers(curl_slist** headers) const {
    if (!token_.empty()) {
        *headers = curl_slist_append(*headers, ("Authorization: Bearer " + token_).c_str());
    }
}

bool BearerTokenAuth::is_valid() const { return !token_.empty(); }
std::string BearerTokenAuth::describe() const { return "bearer"; }

void NoAuth::apply_headers(curl_slist** headers) const { (void)headers; }

// =============================================================================
// ProviderConfigFactory
// =============================================================================

std::unique_ptr<Provider> ProviderConfigFactory::create(const ProviderConfig& config) {
    switch (config.type) {
        case ProviderType::ANTHROPIC: {
            AuthType auth = (config.auth_type == ProviderConfig::AuthType::OAUTH)
                ? AuthType::OAUTH : AuthType::API_KEY;
            return std::make_unique<AnthropicProvider>(config.api_key, auth, config.base_url);
        }
        case ProviderType::OPENAI_COMPAT:
            return std::make_unique<OpenAICompatProvider>(config.api_key, config.base_url);
    }
    return nullptr;
}

std::vector<ProviderType> ProviderConfigFactory::available_types() {
    return {ProviderType::ANTHROPIC, ProviderType::OPENAI_COMPAT};
}

std::string ProviderConfigFactory::type_to_string(ProviderType type) {
    switch (type) {
        case ProviderType::ANTHROPIC: return "anthropic";
        case ProviderType::OPENAI_COMPAT: return "openai-compat";
    }
    return "unknown";
}

ProviderType ProviderConfigFactory::string_to_type(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "anthropic" || lower == "claude") return ProviderType::ANTHROPIC;
    if (lower == "openai" || lower == "openai-compat" || lower == "ollama" || lower == "openrouter") return ProviderType::OPENAI_COMPAT;
    return ProviderType::ANTHROPIC;  // default
}

}  // namespace pluto
