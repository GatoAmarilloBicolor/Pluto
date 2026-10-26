#ifndef MOCK_HANDLER_H
#define MOCK_HANDLER_H

#include <Message.h>

class BLooper;

class BHandler {
public:
    BHandler(const char* name = nullptr) { (void)name; }
    virtual ~BHandler() = default;
    
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    virtual void MessageDropped(BMessage* msg, BHandler* handler) { (void)msg; (void)handler; }
    
    BLooper* Looper() const { return nullptr; }
    void SetLooper(BLooper* looper) { (void)looper; }
    const char* Name() const { return ""; }
    void SetName(const char* name) { (void)name; }
};

#endif
