#ifndef MOCK_STRING_VIEW_H
#define MOCK_STRING_VIEW_H

#include "View.h"
#include "Rect.h"

class BStringView : public BView {
public:
    // Haiku BStringView convenience constructor (no frame needed)
    BStringView(const char* name, const char* text)
        : BView(BRect(0, 0, 100, 20), name, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {
        (void)text;
    }
    // Full constructor for compatibility
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