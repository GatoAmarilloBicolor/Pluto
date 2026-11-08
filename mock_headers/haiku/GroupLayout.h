#ifndef MOCK_GROUP_LAYOUT_H
#define MOCK_GROUP_LAYOUT_H

#include "Layout.h"
#include "View.h"

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
    BGroupView(int orient, float spacing)
        : BView(BRect(), "group", B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient; (void)spacing;
    }
    BGroupView(BGroupLayout::orientation orient, float spacing)
        : BView(BRect(), "group", B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient; (void)spacing;
    }
    virtual ~BGroupView() = default;
    
    virtual BGroupLayout* GroupLayout() const { return nullptr; }
    virtual void SetLayout(BLayout* layout) { (void)layout; }
    virtual void Hide() {}
    virtual void Show() {}
};

#endif