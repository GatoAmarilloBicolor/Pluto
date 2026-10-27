#ifndef MOCK_STRING_VIEW_H
#define MOCK_STRING_VIEW_H

#include "View.h"
#include "Rect.h"

class BStringView : public BView {
public:
    BStringView(BRect frame, const char* name, const char* text,
                uint32 resizingMode, uint32 flags)
        : BView(frame, name, resizingMode, flags) {
        (void)text;
    }
    virtual ~BStringView() = default;
    
    virtual void SetText(const char* text) { (void)text; }
    virtual const char* Text() const { return ""; }
    virtual void SetAlignment(alignment align) { (void)align; }
    virtual alignment Alignment() const { return B_ALIGN_LEFT; }
    
    enum alignment {
        B_ALIGN_LEFT = 0,
        B_ALIGN_RIGHT = 1,
        B_ALIGN_CENTER = 2
    };
};

#endif