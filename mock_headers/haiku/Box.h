#ifndef MOCK_BOX_H
#define MOCK_BOX_H

#include "View.h"
#include "Rect.h"

class BBox : public BView {
public:
    BBox(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
        : BView(frame, name, resizingMode, flags) {}
    virtual ~BBox() = default;
    
    virtual void SetLabel(const char* label) { (void)label; }
    virtual const char* Label() const { return ""; }
    virtual void SetFont(const BFont* font) { (void)font; }
    
    enum label_location {
        B_OFFSET_LABEL = 0,
        B_LEFT_TOP = 1
    };
    virtual void SetLabelLocation(label_location location) { (void)location; }
};

#endif