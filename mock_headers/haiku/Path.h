#ifndef MOCK_PATH_H
#define MOCK_PATH_H

#include "Types.h"
#include <string>

class BPath {
public:
    BPath() = default;
    BPath(const char* path) : fPath(path ? path : "") {}
    BPath(const entry_ref* ref) { (void)ref; }
    BPath(const BDirectory* dir, const char* name) { (void)dir; (void)name; }
    ~BPath() = default;
    
    status_t SetTo(const char* path) { fPath = path ? path : ""; return B_OK; }
    status_t SetTo(const char* path, const char* leaf, bool trailingSlash) { 
        fPath = path ? path : ""; 
        (void)leaf; 
        (void)trailingSlash; 
        return B_OK; 
    }
    status_t SetTo(const entry_ref* ref) { (void)ref; return B_OK; }
    status_t SetTo(const BDirectory* dir, const char* name) { (void)dir; (void)name; return B_OK; }
    status_t Append(const char* path) { fPath += "/"; fPath += path; return B_OK; }
    const char* Path() const { return fPath.c_str(); }
    const char* Leaf() const { 
        size_t pos = fPath.find_last_of('/');
        return pos != std::string::npos ? fPath.c_str() + pos + 1 : fPath.c_str();
    }
    status_t GetParent(BPath* parent) const { (void)parent; return B_OK; }
    
private:
    std::string fPath;
};

#endif
