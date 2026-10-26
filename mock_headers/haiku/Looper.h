#ifndef MOCK_LOOPER_H
#define MOCK_LOOPER_H

#include "Types.h"
#include <Handler.h>
#include <Message.h>

class BLooper : public BHandler {
public:
    BLooper(const char* name = nullptr) { (void)name; }
    virtual ~BLooper() = default;
    
    thread_id Run() { return 0; }
    void Quit() {}
    bool Lock() { return true; }
    void Unlock() {}
    bool LockWithTimeout(bigtime_t timeout) { (void)timeout; return true; }
    
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    virtual void DispatchMessage(BMessage* msg, BHandler* handler) { (void)msg; (void)handler; }
    
    static BLooper* LooperForThread(thread_id thread) { (void)thread; return nullptr; }
    thread_id Thread() const { return 0; }
};

#endif
