#pragma once

#include <string>
#include <vector>
#include <memory>

#include "../provider/Provider.h"
#include "../config.h"
#include "../tools/Tool.h"

namespace pluto {
namespace langchain {

class ToolRegistry;
class Provider;
struct Config;

// Base class for Language Models.
class LLM {
public:
	virtual ~LLM() = default;
	virtual std::string predict(const std::string& prompt) = 0;
	virtual std::string predict(const std::string& prompt,
	                            const std::vector<std::string>& stop) = 0;
};

// LLM implementation that wraps Pluto's Provider.
class ProviderLLM : public LLM {
public:
	ProviderLLM(Provider* provider,
	            const Config& cfg,
	            ToolRegistry* tools = nullptr);

	std::string predict(const std::string& prompt) override;

	std::string predict(const std::string& prompt,
	                    const std::vector<std::string>& stop) override;

private:
	Provider* fProvider;
	const Config& fCfg;
	ToolRegistry* fTools;
};

} // namespace pluto::langchain
