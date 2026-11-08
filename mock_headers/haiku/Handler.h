#ifndef MOCK_HANDLER_H
#define MOCK_HANDLER_H

#include "Types.h"
#include "Message.h"

class BLooper;

class BHandler {
public:
    BHandler(const char* name = nullptr) { (void)name; }
    virtual ~BHandler() = default;
    
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    BLooper* Looper() const { return nullptr; }
    const char* Name() const { return "BHandler"; }
    virtual status_t SetName(const char* name) { (void)name; return B_OK; }
};

#endif