// DSH Integration — Main entry point for DSH functionality in Pluto
// 100% Haiku API via Pluto's HaikuAbstraction layer

#pragma once

#include "Context.h"
#include "Bundle.h"
#include "../plugin.h"
#include "../tools/Tool.h"
#include "../config.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace pluto::dsh {

// DSH Tool definition matching DSH's tool schema
struct DshTool {
    std::string name;
    std::string description;
    json::Value input_schema;  // JSON Schema for input validation
    std::function<json::Value(const json::Value&)> execute;
    bool requires_approval = false;
    std::vector<std::string> tags;  // e.g., "file", "web", "mcp", "system"
};

// DSH Skill definition
struct DshSkill {
    std::string name;
    std::string description;
    std::string version;
    std::string entry_file;  // e.g., "SKILL.md" or "index.js"
    std::vector<std::string> dependencies;  // other skill names
    json::Value config_schema;
    std::function<void(Context&)> apply;  // Called when skill is loaded
};

// DSH UI Slot definition (for panel decorations, sidebar, etc.)
struct DshSlot {
    std::string id;           // Unique slot identifier
    std::string name;         // Human-readable name
    std::string description;  // What this slot displays
    std::string component;    // Component identifier (e.g., "panel", "sidebar", "composer")
    json::Value config_schema;
    std::function<json::Value(Context&)> render;  // Returns JSON for UI
};

// DSH Client-side module (for web UI)
struct DshClientModule {
    std::string name;
    std::string entry_point;  // e.g., "client.js"
    std::vector<std::string> slots;  // Slot IDs this module registers
    std::string css;          // Optional CSS
};

// DSH Bundle Plugin interface (what bundles export)
struct BundlePlugin {
    std::string id;
    std::string name;
    json::Value config_schema;
    std::vector<std::string> provides_services;
    std::vector<std::string> requires_services;
    std::function<void(Context&, const json::Value&)> apply;
    std::function<void(Context&)> unapply;
};

// Main DSH Integration class
class DshIntegration {
public:
    DshIntegration();
    ~DshIntegration();

    // Initialize DSH system (load bundles, skills, register tools)
    bool initialize();

    // Shutdown DSH system
    void shutdown();

    // Get the root DSH context
    Context& context() { return ctx_; }

    // Bundle management
    bool install_bundle(const std::string& source_dir, const std::string& name);
    bool uninstall_bundle(const std::string& name);
    bool enable_bundle(const std::string& name);
    bool disable_bundle(const std::string& name);
    std::vector<std::string> list_bundles() const;
    std::vector<std::string> active_bundles() const;

    // Tool registration (for DSH-compatible tools)
    void register_tool(const DshTool& tool);
    void unregister_tool(const std::string& name);
    std::optional<DshTool> get_tool(const std::string& name) const;
    std::vector<DshTool> list_tools() const;

    // Skill management
    bool load_skill(const std::string& skill_name);
    bool unload_skill(const std::string& skill_name);
    std::vector<std::string> list_skills() const;
    std::vector<std::string> loaded_skills() const;

    // UI Slot registration
    void register_slot(const DshSlot& slot);
    void unregister_slot(const std::string& id);
    std::vector<DshSlot> list_slots() const;

    // Client module registration
    void register_client_module(const DshClientModule& module);
    std::vector<DshClientModule> list_client_modules() const;

    // MCP bundle support (config-only bundles)
    bool install_mcp_bundle(const std::string& name, const json::Value& config);
    bool remove_mcp_bundle(const std::string& name);

    // Configuration persistence
    bool save_plugin_config(const std::string& bundle, const std::string& plugin, const json::Value& config);
    std::optional<json::Value> load_plugin_config(const std::string& bundle, const std::string& plugin);

    // Event emission
    template<typename T>
    void emit(const std::string& event, const T& data) {
        ctx_.emit(event, data);
    }

    // Subagent support (extends existing subagent_plugin)
    std::shared_ptr<Context> create_subagent_context(
        const std::string& name,
        const std::vector<std::string>& allowed_tools = {},
        const std::vector<std::string>& denied_tools = {});

    // Default DSH tools (built-in)
    static std::vector<DshTool> builtin_tools();

    // Default DSH skills
    static std::vector<DshSkill> builtin_skills();

private:
    Context ctx_;
    BundleManager bundle_mgr_;
    std::map<std::string, DshTool> tools_;
    std::map<std::string, DshSkill> skills_;
    std::map<std::string, DshSlot> slots_;
    std::map<std::string, DshClientModule> client_modules_;
    std::map<std::string, std::shared_ptr<Context>> subagent_contexts_;
    bool initialized_ = false;

    void register_builtin_tools();
    void register_builtin_skills();
    void load_installed_bundles();
    void load_skills_from_dir(const std::string& skills_dir);
};

// Global accessor
DshIntegration& dsh_integration();

// CLI commands for DSH management
namespace Cli {
    int run_dsh_cli(int argc, char** argv);
}

} // namespace pluto::dsh