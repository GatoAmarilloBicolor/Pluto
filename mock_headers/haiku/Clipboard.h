#ifndef MOCK_CLIPBOARD_H
#define MOCK_CLIPBOARD_H

#include "Types.h"
#include "Message.h"

class BClipboard {
public:
    BClipboard() = default;
    BClipboard(const char* name) { (void)name; }
    ~BClipboard() = default;
    
    bool Lock() { return true; }
    void Unlock() {}
    
    status_t Clear() { return B_OK; }
    status_t Commit() { return B_OK; }
    status_t Revert() { return B_OK; }
    
    BMessage* Data() { return nullptr; }
    const BMessage* Data() const { return nullptr; }
    
    bool IsEmpty() const { return true; }
    int32 CountTypes() const { return 0; }
    
    status_t AddData(const char* type, const void* data, size_t length) {
        (void)type; (void)data; (void)length;
        return B_ERROR;
    }
    
    status_t AddString(const char* type, const char* string) {
        (void)type; (void)string;
        return B_ERROR;
    }
};

#endif