#ifndef MOCK_SCROLL_VIEW_H
#define MOCK_SCROLL_VIEW_H

#include "View.h"
#include "Rect.h"

class BScrollView : public BView {
public:
    BScrollView(const char* name, BView* target, uint32 resizingMode,
                uint32 flags, bool horizontal, bool vertical,
                border_style border = B_FANCY_BORDER) {
        (void)name; (void)target; (void)resizingMode; (void)flags;
        (void)horizontal; (void)vertical; (void)border;
    }
    virtual ~BScrollView() = default;
    
    virtual BView* Target() const { return nullptr; }
    virtual void SetTarget(BView* target) { (void)target; }
    
    enum border_style {
        B_NO_BORDER = 0,
        B_PLAIN_BORDER = 1,
        B_FANCY_BORDER = 2
    };
};

#endif