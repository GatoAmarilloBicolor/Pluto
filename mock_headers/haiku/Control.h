#ifndef MOCK_CONTROL_H
#define MOCK_CONTROL_H

#include "View.h"
#include "Rect.h"
#include "Message.h"

class BControl : public BView {
public:
    BControl(BRect frame, const char* name, const char* label,
             BMessage* message, uint32 resizingMode, uint32 flags)
        : BView(frame, name, resizingMode, flags) {
        (void)label; (void)message;
    }
    virtual ~BControl() = default;
    
    virtual void SetValue(int32 value) { (void)value; }
    virtual int32 Value() const { return 0; }
    virtual void SetEnabled(bool enabled) { (void)enabled; }
    virtual bool IsEnabled() const { return true; }
};

#endif