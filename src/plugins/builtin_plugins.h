#pragma once

#include "plugin.h"
#include "../tools/Tool.h"
#include "../config.h"
#include "../storage/SessionStore.h"

namespace pluto {

// Tools capability definition
class ToolsCapability : public CapabilityDef<ToolRegistry> {
public:
    ::std::string name() const override { return ServiceNames::TOOLS; }
    ::std::string version() const override { return "1.0"; }
};

// Built-in tools provider
class BuiltinToolsProvider : public CapabilityProvider<ToolRegistry> {
public:
    explicit BuiltinToolsProvider(const Config* cfg = nullptr) : config_(cfg) {}
    
    ::std::shared_ptr<ToolRegistry> create() const override {
        auto registry = ::std::make_shared<ToolRegistry>();
        // Tools are added by the caller after creation
        return registry;
    }
    
    ::std::string provider_name() const override { return "builtin_tools"; }
    
    // Allow adding tools after creation
    void populate(ToolRegistry& registry) const;

private:
    const Config* config_;
};

// Tools plugin - mounts the tools capability
class ToolsPlugin : public Plugin {
public:
    ::std::string name() const override { return "tools"; }
    ::std::string version() const override { return "1.0"; }
    ::std::vector<::std::string> depends_on() const override { return {}; }
    
    void apply(PluginContext& ctx) override {
        auto provider = ::std::make_shared<BuiltinToolsProvider>();
        auto registry = provider->create();
        ctx.provide<ToolRegistry>(ServiceNames::TOOLS, registry);
        // Store provider for later population
        providers_[ServiceNames::TOOLS] = provider;
    }
    
    void unapply(PluginContext& ctx) override {
        ctx.remove<ToolRegistry>(ServiceNames::TOOLS);
        providers_.clear();
    }
    
    // Access the registry for adding tools
    ToolRegistry* registry() {
        if (auto ctx = root_ctx_.lock()) {
            return ctx->try_get<ToolRegistry>(ServiceNames::TOOLS).get();
        }
        return nullptr;
    }
    
    void set_root_context(::std::shared_ptr<PluginContext> ctx) { root_ctx_ = ctx; }

private:
    ::std::weak_ptr<PluginContext> root_ctx_;
    ::std::unordered_map<::std::string, ::std::shared_ptr<BuiltinToolsProvider>> providers_;
};

// Provider plugin for LLM providers
class ProviderPlugin : public Plugin {
public:
    ::std::string name() const override { return "llm_provider"; }
    ::std::string version() const override { return "1.0"; }
    ::std::vector<::std::string> depends_on() const override { return {}; }
    
    void apply(PluginContext& ctx) override {
        // Provider is set by the application based on config
        // This plugin just declares the capability
    }
    
    void unapply(PluginContext& ctx) override {
        ctx.remove<Provider>(ServiceNames::LLM_PROVIDER);
    }
    
    void set_provider(PluginContext& ctx, ::std::shared_ptr<Provider> provider) {
        ctx.provide<Provider>(ServiceNames::LLM_PROVIDER, provider);
    }
};

// Session store plugin
class SessionStorePlugin : public Plugin {
public:
    ::std::string name() const override { return "session_store"; }
    ::std::string version() const override { return "1.0"; }
    ::std::vector<::std::string> depends_on() const override { return {}; }
    
    void apply(PluginContext& ctx) override {
        // Session store is created by the application
    }
    
    void unapply(PluginContext& ctx) override {
        ctx.remove<SessionStore>(ServiceNames::SESSION_STORE);
    }
    
    void set_store(PluginContext& ctx, ::std::shared_ptr<SessionStore> store) {
        ctx.provide<SessionStore>(ServiceNames::SESSION_STORE, store);
    }
};

// Config plugin
class ConfigPlugin : public Plugin {
public:
    ::std::string name() const override { return "config"; }
    ::std::string version() const override { return "1.0"; }
    ::std::vector<::std::string> depends_on() const override { return {}; }
    
    void apply(PluginContext& ctx) override {
        // Config is provided by the application
    }
    
    void unapply(PluginContext& ctx) override {
        ctx.remove<Config>(ServiceNames::CONFIG);
    }
    
    void set_config(PluginContext& ctx, ::std::shared_ptr<Config> config) {
        ctx.provide<Config>(ServiceNames::CONFIG, config);
    }
};

} // namespace pluto