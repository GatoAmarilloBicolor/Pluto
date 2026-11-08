#ifndef MOCK_SPLIT_VIEW_H
#define MOCK_SPLIT_VIEW_H

#include "View.h"
#include "Rect.h"

class BSplitView : public BView {
public:
    enum orientation {
        B_HORIZONTAL = 0,
        B_VERTICAL = 1
    };
    
    BSplitView(orientation orient = B_HORIZONTAL)
        : BView(BRect(), "SplitView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient;
    }
    BSplitView(int orient, float spacing)
        : BView(BRect(), "SplitView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {
        (void)orient; (void)spacing;
    }
    BSplitView(orientation orient, float spacing)
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