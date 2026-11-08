#ifndef MOCK_HANDLER_H
#define MOCK_HANDLER_H

#include "Types.h"
#include "Message.h"

class BLooper;
class BView;

class BHandler : public BArchivable {
public:
    BHandler(const char* name = nullptr) { (void)name; }
    virtual ~BHandler() = default;
    
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    BLooper* Looper() const { return nullptr; }
    const char* Name() const { return "BHandler"; }
    virtual status_t SetName(const char* name) { (void)name; return B_OK; }
    
    // SetTarget removed to avoid diamond inheritance ambiguity with BView/BInvoker
    // virtual void SetTarget(BHandler* target) { (void)target; }
    // virtual void SetTarget(BView* view, BHandler* target) { (void)view; (void)target; }
    // virtual BHandler* Target() const { return nullptr; }
    
    virtual status_t Archive(BMessage* archive, bool deep = true) const override { (void)archive; (void)deep; return B_OK; }
};

#endif