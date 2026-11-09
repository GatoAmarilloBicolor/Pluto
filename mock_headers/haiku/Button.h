#ifndef MOCK_BUTTON_H
#define MOCK_BUTTON_H

#include "Control.h"

class BButton : public BControl {
public:
    BButton(BRect frame, const char* name, const char* label,
            BMessage* message, uint32 resizingMode, uint32 flags)
        : BControl(frame, name, label, message, resizingMode, flags) {}
    BButton(BRect frame, const char* label, BMessage* message)
        : BControl(frame, "", label, message, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {}
    BButton(const char* name, const char* label, BMessage* message)
        : BControl(BRect(), name, label, message, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {}
    BButton(const char* label, BMessage* message)
        : BControl(BRect(), "", label, message, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {}
    virtual ~BButton() = default;
    
    virtual void SetExplicitSize(BSize size) { (void)size; }
    virtual void MakeDefault(bool isDefault = true) { (void)isDefault; }
    virtual void SetMessage(BMessage* message) { (void)message; }
    virtual void SetExplicitAlignment(BAlignment alignment) { (void)alignment; }
};

#endif