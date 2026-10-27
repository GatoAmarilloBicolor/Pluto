#ifndef MOCK_TEXT_CONTROL_H
#define MOCK_TEXT_CONTROL_H

#include "Control.h"
#include "TextView.h"

class BTextControl : public BControl {
public:
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
};

#endif