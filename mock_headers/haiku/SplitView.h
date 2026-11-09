#ifndef MOCK_SPLIT_VIEW_H
#define MOCK_SPLIT_VIEW_H

#include "View.h"
#include "Rect.h"

class BSplitView : public BView {
public:
    // Accept both int and enum for orientation
    BSplitView(int orient = B_HORIZONTAL, float spacing = B_USE_DEFAULT_SPACING)
        : BView(BRect(), "SplitView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient; (void)spacing;
    }
    virtual ~BSplitView() = default;
    
    virtual void AddChild(BView* child, float weight = 1.0f) { (void)child; (void)weight; }
    virtual void RemoveChild(BView* child) { (void)child; }
    virtual int32 CountChildren() const { return 0; }
    virtual BView* ChildAt(int32 index) const { (void)index; return nullptr; }
    virtual void SetCollapsed(int32 index, bool collapsed) { (void)index; (void)collapsed; }
    virtual bool IsCollapsed(int32 index) const { (void)index; return false; }
    virtual void SetWeight(int32 index, float weight) { (void)index; (void)weight; }
    virtual float Weight(int32 index) const { (void)index; return 1.0f; }
    virtual void SetCollapsible(int32 index, bool collapsible) { (void)index; (void)collapsible; }
};

#endif