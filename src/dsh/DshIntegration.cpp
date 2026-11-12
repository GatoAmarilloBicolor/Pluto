// DSH Integration — Main entry point for DSH functionality in Pluto
// 100% Haiku API via Pluto's HaikuAbstraction layer

#include "DshIntegration.h"
#include "Context.h"
#include "Bundle.h"
#include "../haiku/HaikuAbstraction.h"
#include "../json.h"
#include "../config.h"
#include "../tools/Tool.h"
#include "../tools/Builtin.h"
#include "../tools/WebFetch.h"
#include "../tools/WebSearch.h"
#include <fstream>
#include <filesystem>
#include <mutex>

namespace pluto::dsh {

namespace fs = std::filesystem;

DshIntegration::DshIntegration() : bundle_mgr_(ctx_) {
    // Initialize Haiku services
    gStorage; // Force initialization
}

DshIntegration::~DshIntegration() {
    shutdown();
}

bool DshIntegration::initialize() {
    if (initialized_) return true;

    // Ensure bundles directory exists
    gStorage->create_directory(BundleLoader::bundles_dir());

    // Ensure skills directory exists
    std::string skills_dir = pluto::SettingsDir() + "/skills";
    gStorage->create_directory(skills_dir);

    // Register built-in tools
    register_builtin_tools();

    // Register built-in skills
    register_builtin_skills();

    // Load installed bundles
    load_installed_bundles();

    // Load skills from skills directory
    load_skills_from_dir(skills_dir);

    initialized_ = true;
    return true;
}

void DshIntegration::shutdown() {
    if (!initialized_) return;

    // Unload all bundles
    for (const auto& name : active_bundles()) {
        disable_bundle(name);
    }

    // Clear all registrations
    tools_.clear();
    skills_.clear();
    slots_.clear();
    client_modules_.clear();
    subagent_contexts_.clear();

    initialized_ = false;
}

bool DshIntegration::install_bundle(const std::string& source_dir, const std::string& name) {
    return BundleLoader::install_bundle(source_dir, name);
}

bool DshIntegration::uninstall_bundle(const std::string& name) {
    disable_bundle(name);
    return BundleLoader::uninstall_bundle(name);
}

bool DshIntegration::enable_bundle(const std::string& name) {
    return bundle_mgr_.activate_bundle(name);
}

bool DshIntegration::disable_bundle(const std::string& name) {
    return bundle_mgr_.deactivate_bundle(name);
}

std::vector<std::string> DshIntegration::list_bundles() const {
    return BundleLoader::list_installed();
}

std::vector<std::string> DshIntegration::active_bundles() const {
    return bundle_mgr_.active_bundles();
}

void DshIntegration::register_tool(const DshTool& tool) {
    tools_[tool.name] = tool;
}

void DshIntegration::unregister_tool(const std::string& name) {
    tools_.erase(name);
}

std::optional<DshTool> DshIntegration::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) return it->second;
    return std::nullopt;
}

std::vector<DshTool> DshIntegration::list_tools() const {
    std::vector<DshTool> result;
    result.reserve(tools_.size());
    for (const auto& [_, tool] : tools_) result.push_back(tool);
    return result;
}

bool DshIntegration::load_skill(const std::string& skill_name) {
    std::string skills_dir = pluto::SettingsDir() + "/skills/" + skill_name;
    std::string skill_file = skills_dir + "/SKILL.md";

    if (!gStorage->exists(skill_file)) {
        skill_file = skills_dir + "/index.js";
        if (!gStorage->exists(skill_file)) return false;
    }

    // For .md skills, parse and apply
    if (skill_file.ends_with(".md")) {
        auto content = gStorage->read_file(skill_file);
        if (!content) return false;

        // Parse skill frontmatter and instructions
        // For now, just register the skill as loaded
        DshSkill skill;
        skill.name = skill_name;
        skill.entry_file = "SKILL.md";
        skills_[skill_name] = std::move(skill);
        return true;
    }

    // For .js skills, would need JS engine
    return false;
}

bool DshIntegration::unload_skill(const std::string& skill_name) {
    skills_.erase(skill_name);
    return true;
}

std::vector<std::string> DshIntegration::list_skills() const {
    std::vector<std::string> result;
    std::string skills_dir = pluto::SettingsDir() + "/skills";
    if (gStorage->exists(skills_dir)) {
        auto entries = gStorage->list(skills_dir);
        for (const auto& e : entries) {
            if (e.is_directory) result.push_back(e.name);
        }
    }
    return result;
}

std::vector<std::string> DshIntegration::loaded_skills() const {
    std::vector<std::string> result;
    result.reserve(skills_.size());
    for (const auto& [name, _] : skills_) result.push_back(name);
    return result;
}

void DshIntegration::register_slot(const DshSlot& slot) {
    slots_[slot.id] = slot;
}

void DshIntegration::unregister_slot(const std::string& id) {
    slots_.erase(id);
}

std::vector<DshSlot> DshIntegration::list_slots() const {
    std::vector<DshSlot> result;
    result.reserve(slots_.size());
    for (const auto& [_, slot] : slots_) result.push_back(slot);
    return result;
}

void DshIntegration::register_client_module(const DshClientModule& module) {
    client_modules_[module.name] = module;
}

std::vector<DshClientModule> DshIntegration::list_client_modules() const {
    std::vector<DshClientModule> result;
    result.reserve(client_modules_.size());
    for (const auto& [_, module] : client_modules_) result.push_back(module);
    return result;
}

bool DshIntegration::install_mcp_bundle(const std::string& name, const json::Value& config) {
    // Create a minimal bundle for MCP server config
    std::string bundle_dir = BundleLoader::bundles_dir() + "/" + name;
    if (!gStorage->create_directory(bundle_dir)) return false;

    // Write package.json
    json::Value pkg;
    pkg.set("name", "@local/" + name);
    pkg.set("version", "1.0.0");
    pkg.set("private", true);
    pkg.set("type", "module");
    json::Value dsh;
    json::Value bundle;
    bundle.set("patch", "cordis.patch.yml");
    dsh.set("bundle", bundle);
    pkg.set("dsh", dsh);

    // Write cordis.patch.yml
    std::string patch = R"(
- insert:
  - id: mcp_)" + name + R"(
    name: @local/)" + name + R"(
    config: )" + config.serialize() + R"(
)";

    if (!gStorage->write_file(bundle_dir + "/package.json", pkg.serialize())) return false;
    if (!gStorage->write_file(bundle_dir + "/cordis.patch.yml", patch)) return false;

    return enable_bundle(name);
}

bool DshIntegration::remove_mcp_bundle(const std::string& name) {
    return uninstall_bundle(name);
}

bool DshIntegration::save_plugin_config(const std::string& bundle, const std::string& plugin, const json::Value& config) {
    return ConfigStorage::save_plugin_config(bundle, plugin, config);
}

std::optional<json::Value> DshIntegration::load_plugin_config(const std::string& bundle, const std::string& plugin) {
    return ConfigStorage::load_plugin_config(bundle, plugin);
}

std::shared_ptr<Context> DshIntegration::create_subagent_context(
    const std::string& name,
    const std::vector<std::string>& allowed_tools,
    const std::vector<std::string>& denied_tools) {

    auto subagent_ctx = ctx_.isolate();
    
    // Restrict tools if specified
    if (!allowed_tools.empty() || !denied_tools.empty()) {
        auto parent_tools = ctx_.try_get<ToolRegistry>(ServiceNames::TOOLS);
        if (parent_tools) {
            auto restricted = std::make_shared<RestrictedToolRegistry>(parent_tools, allowed_tools, denied_tools);
            subagent_ctx->provide_service<ToolRegistry>(ServiceNames::TOOLS, restricted);
        }
    }

    subagent_contexts_[name] = subagent_ctx;
    return subagent_ctx;
}

void DshIntegration::register_builtin_tools() {
    // Register core DSH-compatible tools
    for (const auto& tool : builtin_tools()) {
        register_tool(tool);
    }
}

void DshIntegration::register_builtin_skills() {
    for (const auto& skill : builtin_skills()) {
        skills_[skill.name] = skill;
    }
}

void DshIntegration::load_installed_bundles() {
    for (const auto& name : BundleLoader::list_installed()) {
        // Auto-enable bundles marked as auto-load
        auto bundle = BundleLoader::load_installed(name);
        if (bundle && bundle->manifest.files.size() > 0) {
            // Check if bundle has auto-load config
            // For now, don't auto-load - require explicit enable
        }
    }
}

void DshIntegration::load_skills_from_dir(const std::string& skills_dir) {
    if (!gStorage->exists(skills_dir)) return;

    auto entries = gStorage->list(skills_dir);
    for (const auto& e : entries) {
        if (e.is_directory) {
            load_skill(e.name);
        }
    }
}

// Built-in DSH tools
std::vector<DshTool> DshIntegration::builtin_tools() {
    std::vector<DshTool> tools;

    // dsh.bundle.install
    tools.push_back(DshTool{
        .name = "dsh.bundle.install",
        .description = "Install a DSH bundle from a local directory",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "source": {"type": "string", "description": "Source directory path"},
                "name": {"type": "string", "description": "Bundle name"}
            },
            "required": ["source", "name"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string source = input.find("source") ? input.find("source")->as_str() : "";
            std::string name = input.find("name") ? input.find("name")->as_str() : "";
            bool ok = dsh.install_bundle(source, name);
            json::Value result;
            result.set("success", ok);
            result.set("message", ok ? "Bundle installed" : "Install failed");
            return result;
        },
        .tags = {"bundle", "management"}
    });

    // dsh.bundle.enable
    tools.push_back(DshTool{
        .name = "dsh.bundle.enable",
        .description = "Enable an installed DSH bundle",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Bundle name"}
            },
            "required": ["name"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string name = input.find("name") ? input.find("name")->as_str() : "";
            bool ok = dsh.enable_bundle(name);
            json::Value result;
            result.set("success", ok);
            result.set("message", ok ? "Bundle enabled" : "Enable failed");
            return result;
        },
        .tags = {"bundle", "management"}
    });

    // dsh.bundle.disable
    tools.push_back(DshTool{
        .name = "dsh.bundle.disable",
        .description = "Disable an active DSH bundle",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Bundle name"}
            },
            "required": ["name"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string name = input.find("name") ? input.find("name")->as_str() : "";
            bool ok = dsh.disable_bundle(name);
            json::Value result;
            result.set("success", ok);
            result.set("message", ok ? "Bundle disabled" : "Disable failed");
            return result;
        },
        .tags = {"bundle", "management"}
    });

    // dsh.bundle.list
    tools.push_back(DshTool{
        .name = "dsh.bundle.list",
        .description = "List all installed DSH bundles",
        .input_schema = json::Value::parse(R"({"type": "object", "properties": {}})"),
        .execute = [](const json::Value&) -> json::Value {
            auto& dsh = dsh_integration();
            auto all = dsh.list_bundles();
            auto active = dsh.active_bundles();
            json::Value result;
            json::Value bundles_arr;
            for (const auto& name : all) {
                json::Value b;
                b.set("name", name);
                b.set("active", std::find(active.begin(), active.end(), name) != active.end());
                bundles_arr.push(std::move(b));
            }
            result.set("bundles", std::move(bundles_arr));
            return result;
        },
        .tags = {"bundle", "management"}
    });

    // dsh.skill.load
    tools.push_back(DshTool{
        .name = "dsh.skill.load",
        .description = "Load a DSH skill",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Skill name"}
            },
            "required": ["name"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string name = input.find("name") ? input.find("name")->as_str() : "";
            bool ok = dsh.load_skill(name);
            json::Value result;
            result.set("success", ok);
            result.set("message", ok ? "Skill loaded" : "Load failed");
            return result;
        },
        .tags = {"skill", "management"}
    });

    // dsh.skill.list
    tools.push_back(DshTool{
        .name = "dsh.skill.list",
        .description = "List all available DSH skills",
        .input_schema = json::Value::parse(R"({"type": "object", "properties": {}})"),
        .execute = [](const json::Value&) -> json::Value {
            auto& dsh = dsh_integration();
            auto all = dsh.list_skills();
            auto loaded = dsh.loaded_skills();
            json::Value result;
            json::Value skills_arr;
            for (const auto& name : all) {
                json::Value s;
                s.set("name", name);
                s.set("loaded", std::find(loaded.begin(), loaded.end(), name) != loaded.end());
                skills_arr.push(std::move(s));
            }
            result.set("skills", std::move(skills_arr));
            return result;
        },
        .tags = {"skill", "management"}
    });

    // dsh.config.save
    tools.push_back(DshTool{
        .name = "dsh.config.save",
        .description = "Save plugin configuration",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "bundle": {"type": "string"},
                "plugin": {"type": "string"},
                "config": {"type": "object"}
            },
            "required": ["bundle", "plugin", "config"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string bundle = input.find("bundle") ? input.find("bundle")->as_str() : "";
            std::string plugin = input.find("plugin") ? input.find("plugin")->as_str() : "";
            json::Value config = input.find("config") ? *input.find("config") : json::Value();
            bool ok = dsh.save_plugin_config(bundle, plugin, config);
            json::Value result;
            result.set("success", ok);
            return result;
        },
        .tags = {"config", "persistence"}
    });

    // dsh.config.load
    tools.push_back(DshTool{
        .name = "dsh.config.load",
        .description = "Load plugin configuration",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "bundle": {"type": "string"},
                "plugin": {"type": "string"}
            },
            "required": ["bundle", "plugin"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string bundle = input.find("bundle") ? input.find("bundle")->as_str() : "";
            std::string plugin = input.find("plugin") ? input.find("plugin")->as_str() : "";
            auto config = dsh.load_plugin_config(bundle, plugin);
            json::Value result;
            if (config) {
                result.set("config", *config);
                result.set("found", true);
            } else {
                result.set("found", false);
            }
            return result;
        },
        .tags = {"config", "persistence"}
    });

    // dsh.subagent.spawn
    tools.push_back(DshTool{
        .name = "dsh.subagent.spawn",
        .description = "Spawn a subagent with restricted tools",
        .input_schema = json::Value::parse(R"({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "task": {"type": "string"},
                "allowed_tools": {"type": "array", "items": {"type": "string"}},
                "denied_tools": {"type": "array", "items": {"type": "string"}}
            },
            "required": ["name", "task"]
        })"),
        .execute = [](const json::Value& input) -> json::Value {
            auto& dsh = dsh_integration();
            std::string name = input.find("name") ? input.find("name")->as_str() : "";
            std::string task = input.find("task") ? input.find("task")->as_str() : "";
            std::vector<std::string> allowed, denied;
            if (auto* a = input.find("allowed_tools"); a && a->is_arr()) {
                for (const auto& item : a->as_arr()) allowed.push_back(item.as_str());
            }
            if (auto* d = input.find("denied_tools"); d && d->is_arr()) {
                for (const auto& item : d->as_arr()) denied.push_back(item.as_str());
            }
            auto ctx = dsh.create_subagent_context(name, allowed, denied);
            json::Value result;
            result.set("success", ctx != nullptr);
            result.set("message", ctx ? "Subagent context created" : "Failed to create subagent");
            return result;
        },
        .tags = {"subagent", "isolation"}
    });

    return tools;
}

// Built-in DSH skills
std::vector<DshSkill> DshIntegration::builtin_skills() {
    return {
        DshSkill{
            .name = "dsh-core",
            .description = "Core DSH functionality (bundles, skills, config, subagents)",
            .version = "1.0",
            .entry_file = "builtin",
            .apply = [](Context& ctx) {
                // Core skill auto-applies
            }
        }
    };
}

// Global instance
static std::unique_ptr<DshIntegration> g_dsh_integration;

DshIntegration& dsh_integration() {
    if (!g_dsh_integration) {
        g_dsh_integration = std::make_unique<DshIntegration>();
    }
    return *g_dsh_integration;
}

// CLI commands
namespace Cli {
int run_dsh_cli(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pluto dsh <command> [args...]\n"
                        "Commands:\n"
                        "  bundle install <source> <name>\n"
                        "  bundle enable <name>\n"
                        "  bundle disable <name>\n"
                        "  bundle list\n"
                        "  bundle remove <name>\n"
                        "  skill load <name>\n"
                        "  skill list\n"
                        "  config save <bundle> <plugin> <json>\n"
                        "  config load <bundle> <plugin>\n");
        return 1;
    }

    auto& dsh = dsh_integration();
    dsh.initialize();

    std::string cmd = argv[1];

    if (cmd == "bundle") {
        if (argc < 3) return 1;
        std::string sub = argv[2];
        if (sub == "install" && argc >= 5) {
            bool ok = dsh.install_bundle(argv[3], argv[4]);
            printf("%s\n", ok ? "Installed" : "Failed");
            return ok ? 0 : 1;
        } else if (sub == "enable" && argc >= 4) {
            bool ok = dsh.enable_bundle(argv[3]);
            printf("%s\n", ok ? "Enabled" : "Failed");
            return ok ? 0 : 1;
        } else if (sub == "disable" && argc >= 4) {
            bool ok = dsh.disable_bundle(argv[3]);
            printf("%s\n", ok ? "Disabled" : "Failed");
            return ok ? 0 : 1;
        } else if (sub == "list") {
            auto all = dsh.list_bundles();
            auto active = dsh.active_bundles();
            for (const auto& name : all) {
                bool is_active = std::find(active.begin(), active.end(), name) != active.end();
                printf("%s%s\n", name.c_str(), is_active ? " (active)" : "");
            }
            return 0;
        } else if (sub == "remove" && argc >= 4) {
            bool ok = dsh.uninstall_bundle(argv[3]);
            printf("%s\n", ok ? "Removed" : "Failed");
            return ok ? 0 : 1;
        }
    } else if (cmd == "skill") {
        if (argc < 3) return 1;
        std::string sub = argv[2];
        if (sub == "load" && argc >= 4) {
            bool ok = dsh.load_skill(argv[3]);
            printf("%s\n", ok ? "Loaded" : "Failed");
            return ok ? 0 : 1;
        } else if (sub == "list") {
            auto all = dsh.list_skills();
            auto loaded = dsh.loaded_skills();
            for (const auto& name : all) {
                bool is_loaded = std::find(loaded.begin(), loaded.end(), name) != loaded.end();
                printf("%s%s\n", name.c_str(), is_loaded ? " (loaded)" : "");
            }
            return 0;
        }
    } else if (cmd == "config") {
        if (argc < 3) return 1;
        std::string sub = argv[2];
        if (sub == "save" && argc >= 6) {
            // argv[3]=bundle, argv[4]=plugin, argv[5]=json
            auto config = json::Value::parse(argv[5]);
            bool ok = dsh.save_plugin_config(argv[3], argv[4], config);
            printf("%s\n", ok ? "Saved" : "Failed");
            return ok ? 0 : 1;
        } else if (sub == "load" && argc >= 5) {
            auto config = dsh.load_plugin_config(argv[3], argv[4]);
            if (config) {
                printf("%s\n", config->serialize().c_str());
            } else {
                printf("(not found)\n");
            }
            return 0;
        }
    }

    fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    return 1;
}
} // namespace Cli

} // namespace pluto::dsh