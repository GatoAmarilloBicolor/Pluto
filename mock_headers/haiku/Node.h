#ifndef MOCK_NODE_H
#define MOCK_NODE_H

#include "Types.h"
#include "BString.h"

class BNode {
public:
    BNode() = default;
    BNode(const entry_ref* ref) { (void)ref; }
    BNode(const char* path) { (void)path; }
    BNode(const BEntry* entry) { (void)entry; }
    ~BNode() = default;
    
    bool InitCheck() const { return true; }
    status_t ReadAttr(const char* name, uint32_t type, off_t pos, void* buf, size_t len) { (void)name; (void)type; (void)pos; (void)buf; (void)len; return B_OK; }
    status_t WriteAttr(const char* name, uint32_t type, off_t pos, const void* buf, size_t len) { (void)name; (void)type; (void)pos; (void)buf; (void)len; return B_OK; }
    status_t WriteAttrString(const char* name, const BString* str) { (void)name; (void)str; return B_OK; }
    status_t RemoveAttr(const char* name) { (void)name; return B_OK; }
    status_t GetAttrInfo(const char* name, attr_info* info) { (void)name; (void)info; return B_OK; }
    status_t ListAttrs(char* buf, size_t len) { (void)buf; (void)len; return B_OK; }
    status_t RewindAttrs() { return B_OK; }
    status_t GetNextAttrName(char* name) { (void)name; return B_ENTRY_NOT_FOUND; }
    dev_t Device() const { return 0; }
    ino_t Node() const { return 0; }
    status_t GetStat(struct stat* st) { (void)st; return B_OK; }
    
    // Read/Write methods (for file I/O)
    ssize_t Read(off_t pos, void* buffer, size_t size) { (void)pos; (void)buffer; (void)size; return 0; }
    ssize_t Write(off_t pos, const void* buffer, size_t size) { (void)pos; (void)buffer; (void)size; return size; }
    // Overload for writing at current position
    ssize_t Write(const void* buffer, size_t size) { (void)buffer; (void)size; return size; }
    ssize_t Read(void* buffer, size_t size) { (void)buffer; (void)size; return 0; }
};

#endif
