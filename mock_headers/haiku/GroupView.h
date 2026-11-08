#ifndef MOCK_GROUP_VIEW_H
#define MOCK_GROUP_VIEW_H

#include "View.h"
#include "GroupLayout.h"

class BGroupView : public BView {
public:
    BGroupView(BRect frame, const char* name, uint32 resizingMode, uint32 flags,
               BGroupLayout::orientation orient = BGroupLayout::B_HORIZONTAL)
        : BView(frame, name, resizingMode, flags) {
        (void)orient;
    }
    BGroupView(const char* name, BGroupLayout::orientation orient = BGroupLayout::B_HORIZONTAL)
        : BView(BRect(), name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient;
    }
    virtual ~BGroupView() = default;
    
    virtual BGroupLayout* GroupLayout() const { return nullptr; }
    virtual void SetLayout(BLayout* layout) { (void)layout; }
};

#endif