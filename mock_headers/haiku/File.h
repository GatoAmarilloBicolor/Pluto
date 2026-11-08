#ifndef MOCK_FILE_H
#define MOCK_FILE_H

#include "Types.h"

class BFile {
public:
    BFile() = default;
    BFile(const entry_ref* ref, uint32_t mode) { (void)ref; (void)mode; }
    BFile(const char* path, uint32_t mode) { (void)path; (void)mode; }
    BFile(const BDirectory* dir, const char* name, uint32_t mode) { (void)dir; (void)name; (void)mode; }
    ~BFile() = default;
    
    bool InitCheck() const { return true; }
    status_t Read(void* buf, size_t len) { (void)buf; (void)len; return 0; }
    status_t Write(const void* buf, size_t len) { (void)buf; (void)len; return 0; }
    status_t ReadAt(off_t pos, void* buf, size_t len) { (void)pos; (void)buf; (void)len; return 0; }
    status_t WriteAt(off_t pos, const void* buf, size_t len) { (void)pos; (void)buf; (void)len; return 0; }
    off_t Seek(off_t pos, uint32_t whence) { (void)pos; (void)whence; return 0; }
    off_t Position() const { return 0; }
    status_t SetSize(off_t size) { (void)size; return B_OK; }
    off_t Size() const { return 0; }
    status_t GetSize(off_t* size) const { if (size) *size = 0; return B_OK; }
    status_t Lock() { return B_OK; }
    status_t Unlock() { return B_OK; }

    // Attribute operations (inherited from BNode in real API)
    status_t ReadAttr(const char* name, uint32_t type, off_t pos, void* buf, size_t len) { (void)name; (void)type; (void)pos; (void)buf; (void)len; return B_OK; }
    status_t WriteAttr(const char* name, uint32_t type, off_t pos, const void* buf, size_t len) { (void)name; (void)type; (void)pos; (void)buf; (void)len; return B_OK; }
    status_t RemoveAttr(const char* name) { (void)name; return B_OK; }
    status_t GetAttrInfo(const char* name, attr_info* info) { (void)name; (void)info; return B_OK; }
    status_t ListAttrs(char* buf, size_t len) { (void)buf; (void)len; return B_OK; }
    status_t RewindAttrs() { return B_OK; }
    status_t GetNextAttrName(char* name) { (void)name; return B_ENTRY_NOT_FOUND; }
    status_t Sync() { return B_OK; }
    status_t GetStat(struct stat* st) { (void)st; return B_OK; }
    status_t SetPermissions(mode_t mode) { (void)mode; return B_OK; }
};

#endif