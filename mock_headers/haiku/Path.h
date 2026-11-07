#ifndef MOCK_PATH_H
#define MOCK_PATH_H

#include "Types.h"
#include <string>
#include <vector>

class BPath {
public:
    BPath() = default;
    BPath(const char* path) : fPath(normalize(path ? path : "")) {}
    BPath(const entry_ref* ref) { (void)ref; }
    BPath(const BEntry* entry) { (void)entry; }
    BPath(const BDirectory* dir, const char* name) { (void)dir; (void)name; }
    ~BPath() = default;

    bool InitCheck() const { return !fPath.empty(); }
    
    status_t SetTo(const char* path) { fPath = normalize(path ? path : ""); return B_OK; }
    status_t SetTo(const char* path, const char* leaf, bool trailingSlash) { 
        if (leaf) {
            fPath = normalize(std::string(path ? path : "") + "/" + leaf);
        } else {
            fPath = normalize(path ? path : "");
        }
        if (trailingSlash && !fPath.empty() && fPath.back() != '/') fPath += '/';
        return B_OK; 
    }
    status_t SetTo(const entry_ref* ref) { (void)ref; return B_OK; }
    status_t SetTo(const BDirectory* dir, const char* name) { (void)dir; (void)name; return B_OK; }
    status_t Append(const char* path) { fPath = normalize(fPath + "/" + (path ? path : "")); return B_OK; }
    const char* Path() const { return fPath.c_str(); }
    const char* Leaf() const { 
        size_t pos = fPath.find_last_of('/');
        return pos != std::string::npos ? fPath.c_str() + pos + 1 : fPath.c_str(); 
    }
    status_t GetParent(BPath* parent) const { (void)parent; return B_OK; }
    
private:
    std::string fPath;
    
    static std::string normalize(const std::string& path) {
        std::vector<std::string> parts;
        bool absolute = !path.empty() && path[0] == '/';
        size_t i = 0;
        while (i < path.size()) {
            size_t j = path.find('/', i);
            if (j == std::string::npos) j = path.size();
            std::string comp = path.substr(i, j - i);
            i = j + 1;
            if (comp.empty() || comp == ".") continue;
            if (comp == "..") {
                if (!parts.empty() && parts.back() != "..")
                    parts.pop_back();
                else if (!absolute)
                    parts.push_back("..");
            } else {
                parts.push_back(comp);
            }
        }
        std::string result;
        if (absolute) result = "/";
        for (size_t k = 0; k < parts.size(); ++k) {
            if (k > 0) result += "/";
            result += parts[k];
        }
        if (result.empty()) result = ".";
        return result;
    }
};

#endif
