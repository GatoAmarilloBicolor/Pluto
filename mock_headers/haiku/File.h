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
    status_t Lock() { return B_OK; }
    status_t Unlock() { return B_OK; }
};

#endif