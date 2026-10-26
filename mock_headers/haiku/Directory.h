#ifndef MOCK_DIRECTORY_H
#define MOCK_DIRECTORY_H

#include "Types.h"
#include "File.h"

// Forward declarations
class BEntry;

class BDirectory {
public:
    BDirectory() = default;
    BDirectory(const entry_ref* ref) { (void)ref; }
    BDirectory(const char* path) { (void)path; }
    BDirectory(const BDirectory* dir, const char* name) { (void)dir; (void)name; }
    BDirectory(const BEntry* entry) { (void)entry; }
    ~BDirectory() = default;
    
    bool InitCheck() const { return true; }
    status_t GetEntry(BEntry* entry) { (void)entry; return B_ENTRY_NOT_FOUND; }
    status_t GetNextEntry(BEntry* entry) { (void)entry; return B_ENTRY_NOT_FOUND; }
    status_t GetNextRef(entry_ref* ref) { (void)ref; return B_ENTRY_NOT_FOUND; }
    void Rewind() {}
    status_t CreateDirectory(const char* path, BDirectory* dir) { (void)path; (void)dir; return B_OK; }
    status_t CreateFile(const char* path, BFile* file) { (void)path; (void)file; return B_OK; }
    status_t GetNextDirents(void* buf, size_t len, int32* count) { (void)buf; (void)len; (void)count; return B_OK; }
};

#endif
