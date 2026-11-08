#ifndef MOCK_LOOPER_H
#define MOCK_LOOPER_H

#include "Types.h"
#include "Message.h"
#include "Locker.h"
#include "Handler.h"

class BLooper : public BHandler {
public:
    BLooper(const char* name = nullptr) : BHandler(name) {}
    virtual ~BLooper() = default;
    
    bool Lock() { return fLocker.Lock(); }
    bool Unlock() { return fLocker.Unlock(); }
    
    virtual void PostMessage(BMessage* msg, BHandler* handler = nullptr) { (void)msg; (void)handler; }
    virtual void PostMessage(uint32 what) { (void)what; }
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    virtual void Quit() {}
    virtual BHandler* PreferredHandler() { return nullptr; }
    virtual void Run() {}
    thread_id Thread() const { return -1; }
    const char* Name() const { return "BLooper"; }
    
    static BLooper* LooperForThread(thread_id thread = -1) { (void)thread; return nullptr; }
    static BLooper* ApplicationLooper() { return nullptr; }

private:
    BLocker fLocker;
};

#endif