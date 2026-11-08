#ifndef MOCK_SEPARATOR_VIEW_H
#define MOCK_SEPARATOR_VIEW_H

#include "View.h"
#include "Rect.h"

class BSeparatorView : public BView {
public:
    BSeparatorView(BRect frame, const char* name, uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW)
        : BView(frame, name, resizingMode, flags) {}
    virtual ~BSeparatorView() = default;
    
    virtual void Draw(BRect updateRect) { (void)updateRect; }
};

#endif