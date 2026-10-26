#include "ProviderFactory.h"
#include "../config.h"
#include "../provider/Anthropic.h"
#include "../provider/OpenAICompat.h"
#include "../oauth.h"

#include <memory>
#include <string>

namespace pluto {

std::unique_ptr<Provider> ProviderFactory::create(const Config& cfg) {
    // Resolve credentials (OAuth -> API key)
    Config mutable_cfg = cfg;
    Credentials creds = ResolveCredentials(mutable_cfg);
    if (creds.empty()) return nullptr;
    
    AuthType auth_type = (creds.type == Credentials::OAUTH) ? AuthType::OAUTH : AuthType::API_KEY;
    
    if (mutable_cfg.provider == "anthropic") {
        return create_anthropic(mutable_cfg);
    } else if (mutable_cfg.provider == "openai-compat") {
        return create_openai_compat(mutable_cfg);
    }
    
    // Auto-detect based on model name
    if (mutable_cfg.model.find("claude") != std::string::npos ||
        mutable_cfg.model.find("haiku") != std::string::npos ||
        mutable_cfg.model.find("sonnet") != std::string::npos ||
        mutable_cfg.model.find("opus") != std::string::npos) {
        return create_anthropic(mutable_cfg);
    }
    
    return create_openai_compat(mutable_cfg);
}

std::unique_ptr<Provider> ProviderFactory::create_anthropic(const Config& cfg) {
    Credentials creds = ResolveCredentials(const_cast<Config&>(cfg));
    if (creds.empty()) return nullptr;
    
    AuthType auth_type = (creds.type == Credentials::OAUTH) ? AuthType::OAUTH : AuthType::API_KEY;
    std::string base_url = cfg.base_url.empty() ? "https://api.anthropic.com" : cfg.base_url;
    
    return std::make_unique<AnthropicProvider>(creds.token, auth_type, base_url);
}

std::unique_ptr<Provider> ProviderFactory::create_openai_compat(const Config& cfg) {
    std::string api_key = ResolveApiKey(cfg);
    std::string base_url = cfg.base_url.empty() ? "https://api.openai.com/v1" : cfg.base_url;
    
    return std::make_unique<OpenAICompatProvider>(api_key, base_url);
}

} // namespace pluto