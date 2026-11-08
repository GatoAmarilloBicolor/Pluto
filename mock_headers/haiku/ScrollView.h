#ifndef MOCK_SCROLL_VIEW_H
#define MOCK_SCROLL_VIEW_H

#include "View.h"
#include "Rect.h"
#include "Types.h"

#include "ScrollBar.h"

class BScrollView : public BView {
public:
    BScrollView(const char* name, BView* target, uint32 resizingMode,
                uint32 flags, bool horizontal, bool vertical,
                border_style border = B_FANCY_BORDER)
        : BView(BRect(), name, resizingMode, flags) {
        (void)name; (void)target; (void)resizingMode; (void)flags;
        (void)horizontal; (void)vertical; (void)border;
    }
    virtual ~BScrollView() = default;
    
    virtual BView* Target() const { return nullptr; }
    virtual void SetTarget(BView* target) { (void)target; }
    
    virtual BScrollBar* ScrollBar(int orientation) const { (void)orientation; return nullptr; }
};

#endif