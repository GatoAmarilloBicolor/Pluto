#pragma once

#include "../config.h"
#include "../provider/Provider.h"
#include <memory>
#include <string>

namespace pluto {

class ProviderFactory {
public:
    static std::unique_ptr<Provider> create(const Config& cfg);
    static std::unique_ptr<Provider> create_anthropic(const Config& cfg);
    static std::unique_ptr<Provider> create_openai_compat(const Config& cfg);
};

} // namespace pluto