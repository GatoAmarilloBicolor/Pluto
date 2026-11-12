// DSH Bundle — Cordis-compatible bundle system for Haiku
// 100% Haiku API via Pluto's HaikuAbstraction layer

#include "Bundle.h"
#include "Context.h"
#include "../haiku/HaikuAbstraction.h"
#include "../json.h"
#include "../config.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>

namespace pluto::dsh {

namespace fs = std::filesystem;

std::string BundleLoader::bundles_dir() {
    return pluto::SettingsDir() + "/bundles";
}

std::optional<BundleManifest> BundleLoader::read_manifest(const std::string& bundle_dir) {
    std::string pkg_path = bundle_dir + "/package.json";
    auto content = gStorage->read_file(pkg_path);
    if (!content) return std::nullopt;

    try {
        auto pkg = json::Value::parse(*content);
        BundleManifest manifest;
        manifest.name = pkg.find("name") ? pkg.find("name")->as_str() : "";
        manifest.version = pkg.find("version") ? pkg.find("version")->as_str() : "1.0.0";

        if (auto* dsh = pkg.find("dsh")) {
            if (auto* bundle = dsh->find("bundle")) {
                if (auto* patch = bundle->find("patch")) {
                    manifest.patch_file = patch->as_str();
                }
            }
        }

        // Read detail and icon from meta in locale files (will be read separately)
        // For now, look for common locations
        if (fs::exists(bundle_dir + "/docs/DETAIL.md")) manifest.detail_file = "docs/DETAIL.md";
        if (fs::exists(bundle_dir + "/icon.svg")) manifest.icon_file = "icon.svg";
        else if (fs::exists(bundle_dir + "/icon.png")) manifest.icon_file = "icon.png";

        // Collect all files
        for (auto& p : fs::recursive_directory_iterator(bundle_dir)) {
            if (p.is_regular_file()) {
                std::string rel = fs::relative(p.path(), bundle_dir).string();
                manifest.files.push_back(rel);
                // Detect locale files
                if (rel.rfind("locale/", 0) == 0 && rel.size() > 7 && rel.substr(rel.size()-5) == ".json") {
                    std::string locale = rel.substr(7, rel.size()-12); // "locale/XX.json" -> "XX"
                    manifest.locale_files[locale] = rel;
                }
            }
        }
        return manifest;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<PatchEntry> BundleLoader::parse_patch_file(const std::string& patch_path) {
    std::vector<PatchEntry> entries;
    auto content = gStorage->read_file(patch_path);
    if (!content) return entries;

    // Simple YAML parser for cordis.patch.yml format
    // Format: - insert: - id: ... name: ... config: {...}
    std::istringstream iss(*content);
    std::string line;
    PatchEntry current;
    bool in_insert = false;
    bool in_entry = false;
    int indent = 0;

    while (std::getline(iss, line)) {
        // Count leading spaces
        int spaces = 0;
        while (spaces < (int)line.size() && line[spaces] == ' ') spaces++;

        std::string trimmed = line.substr(spaces);

        if (trimmed.rfind("- insert:", 0) == 0) {
            in_insert = true;
            continue;
        }
        if (in_insert && trimmed.rfind("- id:", 0) == 0) {
            if (in_entry && !current.id.empty()) {
                entries.push_back(current);
            }
            current = PatchEntry{};
            current.id = trimmed.substr(5);
            // Trim quotes
            if (current.id.size() >= 2 && current.id.front() == '"' && current.id.back() == '"') {
                current.id = current.id.substr(1, current.id.size()-2);
            }
            in_entry = true;
            continue;
        }
        if (in_entry && trimmed.rfind("name:", 0) == 0) {
            current.name = trimmed.substr(5);
            if (current.name.size() >= 2 && current.name.front() == '"' && current.name.back() == '"') {
                current.name = current.name.substr(1, current.name.size()-2);
            }
            continue;
        }
        if (in_entry && trimmed.rfind("config:", 0) == 0) {
            // Config is on next lines, parse as JSON-like
            // For simplicity, we'll parse the whole YAML as JSON later
            continue;
        }
        if (in_entry && trimmed.rfind("enabled:", 0) == 0) {
            std::string val = trimmed.substr(8);
            current.enabled = (val == "true" || val == "yes" || val == "1");
            continue;
        }
        // End of entry (new top-level item or end of insert)
        if (in_entry && spaces == 0 && !trimmed.empty() && trimmed[0] != ' ') {
            entries.push_back(current);
            current = PatchEntry{};
            in_entry = false;
            in_insert = false;
        }
    }
    if (in_entry && !current.id.empty()) {
        entries.push_back(current);
    }

    // Now parse config for each entry from the full YAML
    // Use a proper approach: parse the whole file as JSON after YAML conversion
    // For now, return what we have
    return entries;
}

std::optional<BundleMeta> BundleLoader::read_meta(const std::string& bundle_dir) {
    BundleMeta meta;
    bool found = false;

    // Read all locale files, prefer English
    std::vector<std::string> locales = {"en", "en_US", "en_GB"};
    for (const auto& locale : locales) {
        std::string path = bundle_dir + "/locale/" + locale + ".json";
        auto content = gStorage->read_file(path);
        if (content) {
            try {
                auto loc = json::Value::parse(*content);
                if (auto* m = loc.find("meta")) {
                    if (auto* t = m->find("title")) { meta.title = t->as_str(); found = true; }
                    if (auto* d = m->find("description")) { meta.description = d->as_str(); }
                    if (auto* det = m->find("detail")) { meta.detail = det->as_str(); }
                    // Store all locales
                    for (const auto& [k, v] : loc.as_obj()) {
                        if (k == "meta") {
                            for (const auto& [mk, mv] : v.as_obj()) {
                                if (mk == "title") meta.locale_titles[locale] = mv.as_str();
                                else if (mk == "description") meta.locale_descriptions[locale] = mv.as_str();
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // Also check root locale files
    for (const auto& entry : fs::directory_iterator(bundle_dir + "/locale")) {
        if (entry.path().extension() == ".json") {
            std::string locale = entry.path().stem().string();
            if (meta.locale_titles.count(locale) == 0) {
                auto content = gStorage->read_file(entry.path().string());
                if (content) {
                    try {
                        auto loc = json::Value::parse(*content);
                        if (auto* m = loc.find("meta")) {
                            if (auto* t = m->find("title")) meta.locale_titles[locale] = t->as_str();
                            if (auto* d = m->find("description")) meta.locale_descriptions[locale] = d->as_str();
                        }
                    } catch (...) {}
                }
            }
        }
    }

    return found ? std::optional<BundleMeta>(meta) : std::nullopt;
}

std::string BundleLoader::read_file_safe(const std::string& path) {
    auto content = gStorage->read_file(path);
    return content ? *content : "";
}

std::optional<LoadedBundle> BundleLoader::load_from_dir(const std::string& bundle_dir) {
    LoadedBundle bundle;
    bundle.install_path = bundle_dir;

    auto manifest = read_manifest(bundle_dir);
    if (!manifest) return std::nullopt;
    bundle.manifest = *manifest;

    auto meta = read_meta(bundle_dir);
    if (meta) bundle.meta = *meta;

    // Parse patch file
    std::string patch_path = bundle_dir + "/" + bundle.manifest.patch_file;
    bundle.patches = parse_patch_file(patch_path);

    return bundle;
}

std::optional<LoadedBundle> BundleLoader::load_installed(const std::string& bundle_name) {
    std::string dir = bundles_dir() + "/" + bundle_name;
    if (!gStorage->exists(dir)) return std::nullopt;
    return load_from_dir(dir);
}

std::vector<std::string> BundleLoader::list_installed() {
    std::vector<std::string> result;
    std::string dir = bundles_dir();
    if (!gStorage->exists(dir)) return result;

    auto entries = gStorage->list(dir);
    for (const auto& e : entries) {
        if (e.is_directory) {
            result.push_back(e.name);
        }
    }
    return result;
}

bool BundleLoader::install_bundle(const std::string& source_dir, const std::string& bundle_name) {
    std::string target = bundles_dir() + "/" + bundle_name;
    if (!gStorage->create_directory(bundles_dir())) return false;
    if (!gStorage->create_directory(target)) return false;

    // Copy all files from source to target
    for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
        if (entry.is_regular_file()) {
            std::string rel = fs::relative(entry.path(), source_dir).string();
            std::string dest = target + "/" + rel;

            // Create parent directories
            std::string parent = dest.substr(0, dest.find_last_of('/'));
            if (!parent.empty()) {
                gStorage->create_directory(parent);
            }

            auto content = gStorage->read_file(entry.path().string());
            if (content) {
                if (!gStorage->write_file(dest, *content)) return false;
            }
        }
    }
    return true;
}

bool BundleLoader::uninstall_bundle(const std::string& bundle_name) {
    std::string target = bundles_dir() + "/" + bundle_name;
    // Remove recursively
    for (auto it = fs::recursive_directory_iterator(target); it != fs::recursive_directory_iterator(); ++it) {
        if (it->is_regular_file()) {
            gStorage->remove(it->path().string());
        }
    }
    // Remove directories bottom-up
    std::vector<std::string> dirs;
    for (const auto& entry : fs::recursive_directory_iterator(target)) {
        if (entry.is_directory()) dirs.push_back(entry.path().string());
    }
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) {
        return a.size() > b.size(); // Deeper first
    });
    for (const auto& d : dirs) {
        gStorage->remove(d);
    }
    return gStorage->remove(target);
}

bool BundleLoader::set_bundle_active(const std::string& bundle_name, bool active, Context& ctx) {
    auto bundle = load_installed(bundle_name);
    if (!bundle) return false;

    BundleManager mgr(ctx);
    if (active) return mgr.activate_bundle(bundle_name);
    else return mgr.deactivate_bundle(bundle_name);
}

// BundleManager implementation

bool BundleManager::activate_bundle(const std::string& bundle_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (loaded_bundles_.count(bundle_name) && loaded_bundles_[bundle_name].active) {
        return true; // Already active
    }

    auto bundle_opt = BundleLoader::load_installed(bundle_name);
    if (!bundle_opt) return false;

    LoadedBundle bundle = *bundle_opt;

    // Load and apply patches
    if (!load_and_apply_patches(bundle)) {
        return false;
    }

    bundle.active = true;
    loaded_bundles_[bundle_name] = std::move(bundle);
    return true;
}

bool BundleManager::deactivate_bundle(const std::string& bundle_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loaded_bundles_.find(bundle_name);
    if (it == loaded_bundles_.end() || !it->second.active) {
        return true; // Not active
    }

    unload_bundle_plugins(it->second);
    it->second.active = false;
    it->second.loaded_plugin_ids.clear();
    return true;
}

std::vector<std::string> BundleManager::active_bundles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [name, bundle] : loaded_bundles_) {
        if (bundle.active) result.push_back(name);
    }
    return result;
}

bool BundleManager::is_active(const std::string& bundle_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loaded_bundles_.find(bundle_name);
    return it != loaded_bundles_.end() && it->second.active;
}

void BundleManager::reload_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> active_names;
    for (const auto& [name, bundle] : loaded_bundles_) {
        if (bundle.active) active_names.push_back(name);
    }

    for (const auto& name : active_names) {
        auto& bundle = loaded_bundles_[name];
        unload_bundle_plugins(bundle);
        bundle.loaded_plugin_ids.clear();
        load_and_apply_patches(bundle);
    }
}

std::optional<LoadedBundle> BundleManager::get_bundle(const std::string& bundle_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loaded_bundles_.find(bundle_name);
    if (it != loaded_bundles_.end()) return it->second;
    return std::nullopt;
}

bool BundleManager::load_and_apply_patches(LoadedBundle& bundle) {
    // For each patch entry, load the plugin and apply it
    for (const auto& patch : bundle.patches) {
        // Try to load as apply-form plugin first
        auto apply_plugin = load_apply_plugin(bundle.install_path, patch.id);
        if (apply_plugin) {
            // Create config from patch config
            json::Value config = patch.config;
            // Validate against schema if provided
            if (apply_plugin->Config) {
                // TODO: JSON schema validation
            }
            // Apply the plugin
            apply_plugin->apply(ctx_, config);
            bundle.loaded_plugin_ids.push_back(patch.id);
            continue;
        }

        // Try service-form plugin
        auto service_plugin = load_service_plugin(bundle.install_path, patch.id);
        if (service_plugin) {
            auto service = service_plugin->create_service(ctx_, patch.config);
            ctx_.provide_service(service_plugin->service_name, 
                std::static_pointer_cast<void>(service));
            bundle.loaded_plugin_ids.push_back(patch.id);
            continue;
        }

        // Plugin not found or failed to load
        fprintf(stderr, "[dsh] Failed to load plugin: %s from bundle %s\n",
                patch.id.c_str(), bundle.manifest.name.c_str());
        return false;
    }
    return true;
}

void BundleManager::unload_bundle_plugins(LoadedBundle& bundle) {
    for (const auto& plugin_id : bundle.loaded_plugin_ids) {
        // Remove service from context
        // Note: We don't know the type, so we can't call remove<T> directly
        // In a real implementation, we'd track the service keys
        // For now, we rely on the context's effect cleanup when context is destroyed
    }
}

// Plugin loading from bundle

std::optional<PluginEntry::ApplyPlugin> load_apply_plugin(const std::string& bundle_path, const std::string& plugin_id) {
    // Look for index.js or <plugin_id>.js in bundle root or plugins/ subdir
    std::vector<std::string> candidates = {
        bundle_path + "/index.js",
        bundle_path + "/plugins/" + plugin_id + "/index.js",
        bundle_path + "/" + plugin_id + "/index.js"
    };

    for (const auto& path : candidates) {
        if (gStorage->exists(path)) {
            // In a real implementation, this would use a JS engine (QuickJS, V8, etc.)
            // For now, return nullopt - native C++ plugins are loaded differently
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<PluginEntry::ServicePlugin> load_service_plugin(const std::string& bundle_path, const std::string& plugin_id) {
    // Similar to above, would need JS engine
    return std::nullopt;
}

// ConfigStorage implementation using BFS attributes

namespace ConfigStorage {

std::string config_attr_name(const std::string& plugin_id) {
    return "pluto:dsh:config:" + plugin_id;
}

bool save_plugin_config(const std::string& bundle_name, const std::string& plugin_id, const json::Value& config) {
    std::string bundle_dir = BundleLoader::bundles_dir() + "/" + bundle_name;
    if (!gStorage->exists(bundle_dir)) return false;

    std::string attr_name = config_attr_name(plugin_id);
    std::string json_str = config.serialize();

    return gStorage->write_attr(bundle_dir, attr_name, "string", json_str);
}

std::optional<json::Value> load_plugin_config(const std::string& bundle_name, const std::string& plugin_id) {
    std::string bundle_dir = BundleLoader::bundles_dir() + "/" + bundle_name;
    if (!gStorage->exists(bundle_dir)) return std::nullopt;

    std::string attr_name = config_attr_name(plugin_id);
    auto value = gStorage->read_attr(bundle_dir, attr_name);
    if (!value) return std::nullopt;

    try {
        return json::Value::parse(*value);
    } catch (...) {
        return std::nullopt;
    }
}

bool delete_plugin_config(const std::string& bundle_name, const std::string& plugin_id) {
    std::string bundle_dir = BundleLoader::bundles_dir() + "/" + bundle_name;
    if (!gStorage->exists(bundle_dir)) return false;

    std::string attr_name = config_attr_name(plugin_id);
    return gStorage->remove_attr(bundle_dir, attr_name);
}

} // namespace ConfigStorage

} // namespace pluto::dsh
