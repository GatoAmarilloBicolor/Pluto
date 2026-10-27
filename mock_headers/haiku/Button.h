#ifndef MOCK_BUTTON_H
#define MOCK_BUTTON_H

#include "Control.h"

class BButton : public BControl {
public:
    BButton(BRect frame, const char* name, const char* label,
            BMessage* message, uint32 resizingMode, uint32 flags)
        : BControl(frame, name, label, message, resizingMode, flags) {}
    virtual ~BButton() = default;
};

#endif