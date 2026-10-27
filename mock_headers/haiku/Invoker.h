#ifndef MOCK_INVOKER_H
#define MOCK_INVOKER_H

#include "Handler.h"
#include "Message.h"

class BInvoker : public BHandler {
public:
    BInvoker(BMessage* message, BHandler* target) : BHandler("") {
        (void)message; (void)target;
    }
    virtual ~BInvoker() = default;
    
    virtual void SetMessage(BMessage* message) { (void)message; }
    virtual BMessage* Message() const { return nullptr; }
    virtual void SetTarget(BHandler* target) { (void)target; }
    virtual BHandler* Target() const { return nullptr; }
    virtual status_t Invoke(BMessage* message = nullptr) {
        (void)message;
        return B_OK;
    }
};

#endif