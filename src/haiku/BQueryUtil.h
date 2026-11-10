// BQueryUtil - Shared utilities for BQuery volume iteration and query execution.
// Eliminates duplicate BVolumeRoster iteration code across HaikuAbstraction and tools.

#pragma once

#include <string>
#include <vector>
#include <functional>

#ifdef __HAIKU__
#include <Volume.h>
#include <VolumeRoster.h>
#include <Query.h>
#include <Entry.h>
#include <Path.h>
#include <Node.h>
#endif

namespace pluto {
namespace bquery {

// Iterate over all persistent, writable volumes and apply a callback.
// The callback receives a BVolume reference and should return true to continue,
// false to stop iteration.
#ifdef __HAIKU__
inline void for_each_volume(const std::function<bool(const BVolume&)>& callback) {
    BVolumeRoster roster;
    BVolume vol;
    while (roster.GetNextVolume(&vol) == B_OK) {
        if (!vol.IsPersistent() || vol.IsReadOnly()) continue;
        if (!callback(vol)) break;
    }
}

// Iterate over specific volumes by name.
inline void for_each_volume(const std::vector<std::string>& volume_names,
                            const std::function<bool(const BVolume&)>& callback) {
    BVolumeRoster roster;
    BVolume vol;
    for (const auto& name : volume_names) {
        if (roster.GetVolume(name.c_str(), &vol) == B_OK) {
            if (!vol.IsPersistent() || vol.IsReadOnly()) continue;
            if (!callback(vol)) break;
        }
    }
}

// Execute a BQuery on a single volume with a predicate.
// The result_callback is called for each matching entry.
template <typename ResultCallback>
inline void query_volume(const BVolume& vol, const std::string& predicate, ResultCallback result_callback) {
    BQuery query;
    query.SetVolume(&vol);
    query.SetPredicate(predicate.c_str());
    
    if (query.Fetch() != B_OK) return;

    BEntry entry;
    while (query.GetNextEntry(&entry) == B_OK) {
        BPath path(&entry);
        result_callback(path.Path(), entry);
    }
}

// Build a predicate string for common attribute queries.
inline std::string make_attr_predicate(const std::string& attr_name,
                                        const std::string& attr_value,
                                        const std::string& attr_type = "string") {
    if (attr_type == "string") {
        return attr_name + " == \"" + attr_value + "\"";
    }
    return attr_name + " == " + attr_value;
}

// Read string attributes from a node into a map.
inline std::map<std::string, std::string> read_string_attrs(const BNode& node) {
    std::map<std::string, std::string> attrs;
    char attrName[B_ATTR_NAME_LENGTH];
    while (node.GetNextAttrName(attrName) == B_OK) {
        attr_info info;
        if (node.GetAttrInfo(attrName, &info) == B_OK && info.type == B_STRING_TYPE) {
            std::string value(info.size, '\0');
            if (node.ReadAttr(attrName, info.type, 0, &value[0], info.size) == (ssize_t)info.size) {
                attrs[attrName] = value;
            }
        }
    }
    return attrs;
}

#else  // __HAIKU__

// Mock implementations for cross-compilation
inline void for_each_volume(const std::function<bool(const void*)>&) {}
inline void for_each_volume(const std::vector<std::string>&, const std::function<bool(const void*)>&) {}
template <typename ResultCallback>
inline void query_volume(const void*, const std::string&, ResultCallback) {}
inline std::string make_attr_predicate(const std::string&, const std::string&, const std::string&) { return ""; }
inline std::map<std::string, std::string> read_string_attrs(const void*) { return {}; }

#endif  // __HAIKU__

}  // namespace bquery
}  // namespace pluto
