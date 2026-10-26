// Haiku-native plugin/add-on loader.
// Uses load_image() / get_image_symbol() to dynamically load Pluto add-ons
// from ~/config/settings/pluto/plugins/*.addon
// Each add-on must export: extern "C" pluto::Tool* CreatePlutoTool();

#pragma once

#include "tools/Tool.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pluto {

// A loaded add-on: image handle + the factory symbol.
struct AddonHandle {
#ifdef __HAIKU__
	image_id    image = -1;
#endif
	Tool*       (*create_tool)() = nullptr;
	std::string path;
};

// Scan `dir` for .addon files, load each via load_image(), and return handles.
std::vector<AddonHandle> LoadAddonsFromDir(const std::string& dir);

// Unload all handles (calls unload_image on each).
void UnloadAddons(std::vector<AddonHandle>& handles);

// Instantiate a Tool from each loaded add-on and add to registry.
void RegisterAddonTools(const std::vector<AddonHandle>& addons,
                        ToolRegistry& registry);

// Convenience: load all add-ons from the standard plugins dir and register.
void LoadAndRegisterPlugins(const std::string& plugins_dir,
                            ToolRegistry& registry);

}  // namespace pluto
