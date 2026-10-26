#ifndef MOCK_MESSAGE_H
#define MOCK_MESSAGE_H

#include "Types.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

class BMessage {
public:
    BMessage() = default;
    BMessage(uint32_t what) : fWhat(what) {}
    virtual ~BMessage() = default;
    
    uint32_t what() const { return fWhat; }
    void SetWhat(uint32_t what) { fWhat = what; }
    
    status_t AddData(const char* name, uint32_t type, const void* data, size_t len) { (void)name; (void)type; (void)data; (void)len; return B_OK; }
    status_t AddString(const char* name, const char* str) { (void)name; (void)str; return B_OK; }
    status_t AddInt32(const char* name, int32_t val) { (void)name; (void)val; return B_OK; }
    status_t AddInt64(const char* name, int64_t val) { (void)name; (void)val; return B_OK; }
    status_t AddBool(const char* name, bool val) { (void)name; (void)val; return B_OK; }
    status_t AddPointer(const char* name, void* ptr) { (void)name; (void)ptr; return B_OK; }
    status_t AddMessage(const char* name, const BMessage* msg) { (void)name; (void)msg; return B_OK; }
    
    status_t FindData(const char* name, uint32_t type, const void** data, ssize_t* len) const { (void)name; (void)type; (void)data; (void)len; return B_ERROR; }
    status_t FindString(const char* name, const char** str) const { (void)name; (void)str; return B_ERROR; }
    status_t FindInt32(const char* name, int32_t* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindInt64(const char* name, int64_t* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindBool(const char* name, bool* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindPointer(const char* name, void** ptr) const { (void)name; (void)ptr; return B_ERROR; }
    status_t FindMessage(const char* name, BMessage* msg) const { (void)name; (void)msg; return B_ERROR; }
    
    ssize_t FlattenedSize() const { return 0; }
    status_t Flatten(void* buf, ssize_t size) const { (void)buf; (void)size; return B_OK; }
    status_t Unflatten(const void* buf, ssize_t size) { (void)buf; (void)size; return B_OK; }
    
    bool HasData(const char* name, uint32_t type) const { (void)name; (void)type; return false; }
    bool HasString(const char* name) const { (void)name; return false; }
    bool HasInt32(const char* name) const { (void)name; return false; }
    bool HasBool(const char* name) const { (void)name; return false; }
    
    static const int32_t B_OK = 0;
    static const int32_t B_ERROR = -1;
    static const int32_t B_ENTRY_NOT_FOUND = -2;
    static const int32_t B_BAD_VALUE = -3;
    static const int32_t B_NO_MEMORY = -4;
    static const int32_t B_TIMED_OUT = -5;

private:
    uint32_t fWhat = 0;
};

#endif
