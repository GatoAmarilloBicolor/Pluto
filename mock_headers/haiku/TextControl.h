#ifndef MOCK_TEXT_CONTROL_H
#define MOCK_TEXT_CONTROL_H

#include "Control.h"
#include "TextView.h"

class BTextControl : public BControl {
public:
    BTextControl(const char* name, const char* label,
                 const char* initialText, BMessage* message)
        : BControl(BRect(), name, label, message, 0, 0) {
        (void)initialText;
    }
    BTextControl(BRect frame, const char* name, const char* label,
                 const char* initialText, BMessage* message,
                 uint32 resizingMode, uint32 flags)
        : BControl(frame, name, label, message, resizingMode, flags) {
        (void)initialText;
    }
    virtual ~BTextControl() = default;
    
    virtual void SetText(const char* text) { (void)text; }
    virtual const char* Text() const { return ""; }
    virtual BTextView* TextView() const { return nullptr; }
    virtual void SetModificationMessage(BMessage* msg) { (void)msg; }
};

#endif