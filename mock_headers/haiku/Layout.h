#ifndef MOCK_LAYOUT_H
#define MOCK_LAYOUT_H

#include "Types.h"
#include "View.h"
#include "Size.h"
#include "Rect.h"

class BLayoutItem {
public:
    BLayoutItem() = default;
    virtual ~BLayoutItem() = default;
    virtual BSize MinSize() { return BSize(0, 0); }
    virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED); }
    virtual BSize PreferredSize() { return BSize(0, 0); }
    virtual void SetExplicitMinSize(BSize size) { (void)size; }
    virtual void SetExplicitMaxSize(BSize size) { (void)size; }
    virtual void SetExplicitPreferredSize(BSize size) { (void)size; }
};

class BLayout : public BLayoutItem {
public:
    BLayout() = default;
    virtual ~BLayout() = default;
    virtual void AddView(BView* view) { (void)view; }
    virtual void AddItem(BLayoutItem* item) { (void)item; }
    virtual void RemoveItem(BLayoutItem* item) { (void)item; }
    virtual int32 CountItems() const { return 0; }
    virtual BLayoutItem* ItemAt(int32 index) const { (void)index; return nullptr; }
};

class BGroupLayout : public BLayout {
public:
    enum orientation {
        B_HORIZONTAL = 0,
        B_VERTICAL = 1
    };
    
    BGroupLayout(orientation orient = B_HORIZONTAL) : BLayout() { (void)orient; }
    virtual ~BGroupLayout() = default;
    
    virtual void AddView(BView* view) { (void)view; }
    virtual void AddItem(BLayoutItem* item) { (void)item; }
    virtual void SetInsets(float left, float top, float right, float bottom) { (void)left; (void)top; (void)right; (void)bottom; }
    virtual void GetInsets(float* left, float* top, float* right, float* bottom) const { (void)left; (void)top; (void)right; (void)bottom; }
    virtual float Spacing() const { return 0; }
    virtual void SetSpacing(float spacing) { (void)spacing; }
    virtual orientation Orientation() const { return B_HORIZONTAL; }
    virtual void SetOrientation(orientation orient) { (void)orient; }
};

#endif