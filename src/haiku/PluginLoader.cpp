// PluginLoader.cpp — Haiku-native dynamic add-on loading.

#include "PluginLoader.h"

#include <cstdio>
#include <cstring>

#ifdef __HAIKU__
#include <Path.h>
#include <Directory.h>
#include <Entry.h>
#include <Image.h>
#include <OS.h>
#include <kernel/image.h>
#endif

namespace pluto {

#ifdef __HAIKU__

std::vector<AddonHandle> LoadAddonsFromDir(const std::string& dir) {
	std::vector<AddonHandle> handles;

	BDirectory bdir(dir.c_str());
	if (bdir.InitCheck() != B_OK) return handles;

	BEntry entry;
	while (bdir.GetNextEntry(&entry) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		if (entry.GetName(name) != B_OK) continue;
		std::string fname = name;
		if (fname.size() < 7 || fname.substr(fname.size() - 6) != ".addon")
			continue;

		BPath path(&entry);
		if (path.InitCheck() != B_OK) continue;

		// Load the add-on image.
		image_id img = load_image(path.Path(), B_NORMAL_IMAGE);
		if (img < 0) {
			std::fprintf(stderr, "[plugin] failed to load %s: %s\n",
				path.Path(), strerror(-img));
			continue;
		}

		// Resolve the factory symbol.
		Tool* (*create_tool)() = nullptr;
		if (get_image_symbol(img, "CreatePlutoTool",
		                     B_SYMBOL_TYPE_TEXT,
		                     (void**)&create_tool) != B_OK) {
			std::fprintf(stderr, "[plugin] %s: missing CreatePlutoTool symbol\n",
				path.Path());
			unload_image(img);
			continue;
		}

		AddonHandle h;
		h.image = img;
		h.create_tool = create_tool;
		h.path = path.Path();
		handles.push_back(std::move(h));

		std::fprintf(stderr, "[plugin] loaded %s\n", path.Path());
	}

	return handles;
}

void UnloadAddons(std::vector<AddonHandle>& handles) {
	for (auto& h : handles) {
		if (h.image >= 0) {
			unload_image(h.image);
			h.image = -1;
		}
	}
	handles.clear();
}

#else  // __HAIKU__

std::vector<AddonHandle> LoadAddonsFromDir(const std::string&) {
	return {};
}

void UnloadAddons(std::vector<AddonHandle>& handles) {
	handles.clear();
}

#endif  // __HAIKU__

void RegisterAddonTools(const std::vector<AddonHandle>& addons,
                        ToolRegistry& registry) {
	for (const auto& h : addons) {
		if (!h.create_tool) continue;
		Tool* tool = h.create_tool();
		if (tool) {
			registry.add(std::unique_ptr<Tool>(tool));
			std::fprintf(stderr, "[plugin] registered tool: %s\n",
				tool->name().c_str());
		}
	}
}

void LoadAndRegisterPlugins(const std::string& plugins_dir,
                            ToolRegistry& registry) {
#ifdef __HAIKU__
	auto addons = LoadAddonsFromDir(plugins_dir);
	RegisterAddonTools(addons, registry);
	// Keep addons loaded for process duration.
	(void)addons;
#else
	(void)plugins_dir;
	(void)registry;
#endif
}

}  // namespace pluto
