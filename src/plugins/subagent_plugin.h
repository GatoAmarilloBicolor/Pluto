#pragma once

#include "plugin.h"
#include "tools/Tool.h"
#include "../json.h"

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

namespace pluto {

// Restricted tool registry - only exposes allowed tools
class RestrictedToolRegistry : public ToolRegistry {
public:
    RestrictedToolRegistry(::std::shared_ptr<ToolRegistry> parent,
                          const ::std::vector<::std::string>& allowed,
                          const ::std::vector<::std::string>& denied)
        : parent_(parent), allowed_tools_(allowed), denied_tools_(denied) {}
    
    Tool* find(const ::std::string& name) override {
        // Check denied list first
        if (::std::find(denied_tools_.begin(), denied_tools_.end(), name) != denied_tools_.end()) {
            return nullptr;
        }
        
        // If allowed list is specified, only allow those tools
        if (!allowed_tools_.empty()) {
            if (::std::find(allowed_tools_.begin(), allowed_tools_.end(), name) == allowed_tools_.end()) {
                return nullptr;
            }
        }
        
        return parent_->find(name);
    }
    
    ::std::vector<const Tool*> list() const override {
        auto all_tools = parent_->list();
        ::std::vector<const Tool*> filtered;
        filtered.reserve(all_tools.size());
        
        for (const auto* tool : all_tools) {
            const ::std::string& name = tool->name();
            
            // Check denied list
            if (::std::find(denied_tools_.begin(), denied_tools_.end(), name) != denied_tools_.end()) {
                continue;
            }
            
            // If allowed list is specified, only include those tools
            if (!allowed_tools_.empty()) {
                if (::std::find(allowed_tools_.begin(), allowed_tools_.end(), name) == allowed_tools_.end()) {
                    continue;
                }
            }
            
            filtered.push_back(tool);
        }
        
        return filtered;
    }
    
private:
    ::std::shared_ptr<ToolRegistry> parent_;
    ::std::vector<::std::string> allowed_tools_;
    ::std::vector<::std::string> denied_tools_;
};

// Subagent configuration
struct SubagentConfig {
    ::std::string name;
    ::std::string description;
    ::std::vector<::std::string> allowed_tools;  // Empty = all tools
    ::std::vector<::std::string> denied_tools;   // Explicitly denied tools
    bool inherit_parent_context = true;      // Whether to inherit parent's services
    ::std::vector<::std::string> isolated_services; // Services to isolate (not inherit)
};

// Factory for creating subagent contexts
class SubagentFactory {
public:
    explicit SubagentFactory(PluginContext& parent_ctx) : parent_ctx_(parent_ctx) {}
    
    // Create a subagent context with restricted tools
    ::std::shared_ptr<PluginContext> create_subagent(const SubagentConfig& config) {
        // Determine which services to isolate
        ::std::vector<::std::string> isolated = config.isolated_services;
        
        // Always isolate tools if we're restricting them
        if (!config.allowed_tools.empty() || !config.denied_tools.empty()) {
            isolated.push_back(ServiceNames::TOOLS);
        }
        
        // Create isolated context
        auto subagent_ctx = parent_ctx_.isolate(isolated);
        
        // If tools are restricted, create a restricted tool registry
        if (!config.allowed_tools.empty() || !config.denied_tools.empty()) {
            auto parent_tools = parent_ctx_.try_get<ToolRegistry>(ServiceNames::TOOLS);
            if (parent_tools) {
                auto restricted_registry = ::std::make_shared<RestrictedToolRegistry>(
                    parent_tools, config.allowed_tools, config.denied_tools);
                subagent_ctx->provide<ToolRegistry>(ServiceNames::TOOLS, restricted_registry);
            }
        }
        
        return subagent_ctx;
    }
    
private:
    PluginContext& parent_ctx_;
};

// Service name for subagent factory
namespace ServiceNames {
    inline constexpr const char* SUBAGENT_FACTORY = "subagent_factory";
}

// Subagent plugin - creates isolated contexts for subagents
class SubagentPlugin : public Plugin {
public:
    ::std::string name() const override { return "subagent"; }
    ::std::string version() const override { return "1.0"; }
    ::std::vector<::std::string> depends_on() const override { return {"tools"}; }
    
    void apply(PluginContext& ctx) override {
        // Register the subagent factory
        ctx.provide_factory<SubagentFactory>(ServiceNames::SUBAGENT_FACTORY, 
            [&ctx]() { return ::std::make_shared<SubagentFactory>(ctx); });
    }
    
    void unapply(PluginContext& ctx) override {
        ctx.remove<SubagentFactory>(ServiceNames::SUBAGENT_FACTORY);
    }
};

// Tool definition for spawning subagents
class SpawnSubagentTool : public Tool {
public:
    SpawnSubagentTool(::std::shared_ptr<PluginContext> ctx) : ctx_(ctx) {}
    
    ::std::string name() const override { return "spawn_subagent"; }
    ::std::string description() const override { 
        return "Spawn a subagent with restricted tool access. Args: {\"name\": string, \"task\": string, \"allowed_tools\": string[], \"denied_tools\": string[]}"; 
    }
    ::std::string input_schema_json() const override { return R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "task": {"type": "string"},
            "allowed_tools": {"type": "array", "items": {"type": "string"}},
            "denied_tools": {"type": "array", "items": {"type": "string"}}
        },
        "required": ["name", "task"]
    })"; }
    
    ToolResult execute(const ::std::string& input_json) override {
        // Parse input
        try {
            auto v = json::Value::parse(input_json);
            
            SubagentConfig config;
            config.name = v.find("name") ? v.find("name")->as_str() : "subagent";
            
            if (auto* task_val = v.find("task")) {
                config.description = task_val->as_str();
            }
            
            if (auto* allowed = v.find("allowed_tools"); allowed && allowed->is_arr()) {
                for (const auto& item : allowed->as_arr()) {
                    config.allowed_tools.push_back(item.as_str());
                }
            }
            
            if (auto* denied = v.find("denied_tools"); denied && denied->is_arr()) {
                for (const auto& item : denied->as_arr()) {
                    config.denied_tools.push_back(item.as_str());
                }
            }
            
            // Get factory
            auto factory = ctx_->try_get<SubagentFactory>(ServiceNames::SUBAGENT_FACTORY);
            if (!factory) {
                return ToolResult{.output = "Subagent factory not available", .is_error = true};
            }
            
            // Create subagent context
            auto subagent_ctx = factory->create_subagent(config);
            
            // Store subagent context for later use
            // In a real implementation, this would run the subagent task
            return ToolResult{.output = "Subagent '" + config.name + "' created with restricted tools. Task: " + config.description};
            
        } catch (const ::std::exception& e) {
            return ToolResult{.output = "Failed to spawn subagent: " + ::std::string(e.what()), .is_error = true};
        }
    }
    
private:
    ::std::shared_ptr<PluginContext> ctx_;
};

} // namespace pluto