#ifndef MOCK_MESSENGER_H
#define MOCK_MESSENGER_H

#include "Handler.h"
#include "Looper.h"
#include "View.h"

class BMessenger {
public:
    BMessenger() = default;
    BMessenger(const BHandler* handler, const BLooper* looper = nullptr) { (void)handler; (void)looper; }
    BMessenger(const BHandler* handler) { (void)handler; }
    BMessenger(BHandler* handler) { (void)handler; }
    BMessenger(const BView* view) { (void)view; }
    BMessenger(BView* view) { (void)view; }
    template<typename T>
    BMessenger(T* ptr) { (void)ptr; }
    BMessenger(const char* signature, team_id team = -1) { (void)signature; (void)team; }
    BMessenger(const BMessenger& other) = default;
    ~BMessenger() = default;
    
    BMessenger& operator=(const BMessenger& other) = default;
    
    status_t SendMessage(BMessage* msg) { (void)msg; return B_OK; }
    status_t SendMessage(uint32 what) { (void)what; return B_OK; }
    status_t SendMessage(BMessage* msg, BHandler* handler) { (void)msg; (void)handler; return B_OK; }
    status_t SendMessage(BMessage* msg, const BHandler* handler) { (void)msg; (void)handler; return B_OK; }
    
    status_t SendMessage(BMessage* msg, BHandler* handler, BHandler* replyTo, bigtime_t timeout) { 
        (void)msg; (void)handler; (void)replyTo; (void)timeout; return B_OK; }
    status_t SendMessage(BMessage* msg, const BHandler* handler, BHandler* replyTo, bigtime_t timeout) { 
        (void)msg; (void)handler; (void)replyTo; (void)timeout; return B_OK; }
    
    // 4-arg version with send and receive timeouts (used by PlutoMessenger)
    status_t SendMessage(BMessage* msg, BMessage* reply, bigtime_t sendTimeout, bigtime_t recvTimeout) {
        (void)msg; (void)reply; (void)sendTimeout; (void)recvTimeout; return B_OK;
    }
    status_t SendMessage(BMessage* msg, BMessage* reply, bigtime_t sendTimeout, bigtime_t recvTimeout, BHandler* handler) {
        (void)msg; (void)reply; (void)sendTimeout; (void)recvTimeout; (void)handler; return B_OK;
    }
    
    bool IsValid() const { return false; }
    bool IsTargetLocal() const { return false; }
    BHandler* Target(BLooper** looper = nullptr) const { (void)looper; return nullptr; }
    team_id Team() const { return -1; }
};

#endif
