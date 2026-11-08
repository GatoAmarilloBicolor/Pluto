#ifndef MOCK_GROUP_LAYOUT_H
#define MOCK_GROUP_LAYOUT_H

#include "Types.h"
#include "View.h"
#include "Rect.h"
#include "Size.h"

class BLayoutItem {
public:
    BLayoutItem() = default;
    virtual ~BLayoutItem() = default;
    virtual BSize MinSize() { return BSize(0, 0); }
    virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED); }
    virtual BSize PreferredSize() { return BSize(0, 0); }
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
    virtual void SetExplicitMinSize(BSize size) { (void)size; }
    virtual void SetExplicitMaxSize(BSize size) { (void)size; }
    virtual void SetExplicitPreferredSize(BSize size) { (void)size; }
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

class BLayoutBuilder {
public:
    class Group {
    public:
        Group() = default;
        Group& Add(BView* view) { (void)view; return *this; }
        Group& AddGroup(BGroupLayout::orientation orient) { (void)orient; return *this; }
        Group& AddStrut(float size) { (void)size; return *this; }
        Group& AddGlue() { return *this; }
        Group& SetInsets(float left, float top, float right, float bottom) { (void)left; (void)top; (void)right; (void)bottom; return *this; }
        Group& SetSpacing(float spacing) { (void)spacing; return *this; }
    };
    
    static Group Group_(BView* parent, BGroupLayout::orientation orient = BGroupLayout::B_HORIZONTAL) { (void)parent; (void)orient; return Group(); }
    static Group Group_(BView* parent, const char* name, BGroupLayout::orientation orient = BGroupLayout::B_HORIZONTAL) { (void)parent; (void)name; (void)orient; return Group(); }
};

#endif