#ifndef MOCK_ENTRY_H
#define MOCK_ENTRY_H

#include "Types.h"

// Forward declarations
class BDirectory;
class BPath;

class BEntry {
public:
    BEntry() = default;
    BEntry(const entry_ref* ref) { (void)ref; }
    BEntry(const char* path) { (void)path; }
    BEntry(const char* path, bool traverse) { (void)path; (void)traverse; }
    BEntry(const BDirectory* dir, const char* name) { (void)dir; (void)name; }
    ~BEntry() = default;
    
    bool InitCheck() const { return true; }
    bool Exists() const { return false; }
    bool IsDirectory() const { return false; }
    bool IsFile() const { return true; }
    status_t GetRef(entry_ref* ref) const { (void)ref; return B_OK; }
    status_t GetParent(BEntry* parent) const { (void)parent; return B_OK; }
    status_t GetName(char* name) const { (void)name; return B_OK; }
    status_t GetPath(BPath* path) const { (void)path; return B_OK; }
    status_t GetStat(struct stat* st) const { (void)st; return B_OK; }
    status_t SetTo(const entry_ref* ref) { (void)ref; return B_OK; }
    status_t SetTo(const char* path) { (void)path; return B_OK; }
    status_t SetTo(const BDirectory* dir, const char* name) { (void)dir; (void)name; return B_OK; }
    status_t Remove() { return B_OK; }
};

#endif
