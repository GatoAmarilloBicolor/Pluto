// DSH Bundle — Cordis-compatible bundle system for Haiku
// Parses cordis.patch.yml, loads locale/icons, manages plugin lifecycle

#pragma once

#include "Context.h"
#include "../plugin.h"
#include "../json.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <filesystem>

namespace pluto::dsh {

// Bundle manifest entry (from package.json dsh.bundle.patch)
struct BundleManifest {
    std::string name;
    std::string version;
    std::string patch_file;      // e.g. "cordis.patch.yml"
    std::string detail_file;     // e.g. "docs/DETAIL.md"
    std::string icon_file;       // e.g. "icon.svg"
    std::map<std::string, std::string> locale_files;  // locale -> path
    std::vector<std::string> files;    // all files in bundle
    bool is_host_only = true;    // no native deps, no build scripts
};

// Parsed cordis.patch.yml entry
struct PatchEntry {
    std::string id;              // plugin id
    std::string name;            // package name (e.g. "@local/my-plugin")
    json::Value config;          // plugin config object
    bool enabled = true;
};

// Bundle metadata from locale files
struct BundleMeta {
    std::string title;
    std::string description;
    std::string detail;          // path to detail document
    std::map<std::string, std::string> locale_titles;
    std::map<std::string, std::string> locale_descriptions;
};

// Loaded bundle instance
struct LoadedBundle {
    BundleManifest manifest;
    BundleMeta meta;
    std::string install_path;    // absolute path to bundle root
    std::vector<PatchEntry> patches;
    std::vector<std::string> loaded_plugin_ids;
    bool active = false;
};

// Bundle loader — reads bundle from disk, parses manifest and patches
class BundleLoader {
public:
    // Load bundle from directory (for local development)
    static std::optional<LoadedBundle> load_from_dir(const std::string& bundle_dir);

    // Load bundle from installed location (~/config/settings/pluto/bundles/<name>)
    static std::optional<LoadedBundle> load_installed(const std::string& bundle_name);

    // Get all installed bundles
    static std::vector<std::string> list_installed();

    // Install bundle from source directory
    static bool install_bundle(const std::string& source_dir, const std::string& bundle_name);

    // Uninstall bundle
    static bool uninstall_bundle(const std::string& bundle_name);

    // Enable/disable bundle (loads/unloads plugins)
    static bool set_bundle_active(const std::string& bundle_name, bool active, Context& ctx);

private:
    static std::string bundles_dir();
    static std::optional<BundleManifest> read_manifest(const std::string& bundle_dir);
    static std::vector<PatchEntry> parse_patch_file(const std::string& patch_path);
    static std::optional<BundleMeta> read_meta(const std::string& bundle_dir);
    static std::string read_file_safe(const std::string& path);
};

// Bundle manager — manages active bundles and their plugins
class BundleManager {
public:
    explicit BundleManager(Context& ctx) : ctx_(ctx) {}

    // Load and activate a bundle
    bool activate_bundle(const std::string& bundle_name);

    // Deactivate a bundle (unload its plugins)
    bool deactivate_bundle(const std::string& bundle_name);

    // Get active bundle names
    std::vector<std::string> active_bundles() const;

    // Check if bundle is active
    bool is_active(const std::string& bundle_name) const;

    // Reload all active bundles (e.g. after config change)
    void reload_all();

    // Get loaded bundle info
    std::optional<LoadedBundle> get_bundle(const std::string& bundle_name) const;

private:
    Context& ctx_;
    std::map<std::string, LoadedBundle> loaded_bundles_;
    std::mutex mutex_;

    bool load_and_apply_patches(LoadedBundle& bundle);
    void unload_bundle_plugins(LoadedBundle& bundle);
};

// Plugin entry point types (matching DSH host plugin exports)
namespace PluginEntry {

// Form 1: apply function with optional inject and Config
struct ApplyPlugin {
    std::function<void(Context&, const json::Value&)> apply;
    std::vector<std::string> inject;  // e.g. ["tools"]
    std::optional<json::Value> Config; // JSON schema for config validation
};

// Form 2: Service class as default export
struct ServicePlugin {
    std::function<std::shared_ptr<void>(Context&, const json::Value&)> create_service;
    std::string service_name;
    std::string service_type;  // type identifier
};

} // namespace PluginEntry

// Load plugin from bundle (reads index.js, determines export form)
std::optional<PluginEntry::ApplyPlugin> load_apply_plugin(const std::string& bundle_path, const std::string& plugin_id);
std::optional<PluginEntry::ServicePlugin> load_service_plugin(const std::string& bundle_path, const std::string& plugin_id);

// Configuration persistence for plugins (uses BFS attributes)
namespace ConfigStorage {

// Save plugin config to BFS attribute on bundle directory
bool save_plugin_config(const std::string& bundle_name, const std::string& plugin_id, const json::Value& config);

// Load plugin config from BFS attribute
std::optional<json::Value> load_plugin_config(const std::string& bundle_name, const std::string& plugin_id);

// Delete plugin config
bool delete_plugin_config(const std::string& bundle_name, const std::string& plugin_id);

} // namespace ConfigStorage

} // namespace pluto::dsh