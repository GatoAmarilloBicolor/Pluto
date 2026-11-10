#ifndef MOCK_LIST_VIEW_H
#define MOCK_LIST_VIEW_H

#include "Types.h"
#include "View.h"
#include "List.h"
#include <string>

class BListItem {
public:
    BListItem() = default;
    virtual ~BListItem() = default;
    virtual void DrawItem(BView* owner, BRect frame, bool complete = false) { (void)owner; (void)frame; (void)complete; }
    virtual void Update(BView* owner, const BFont* font) { (void)owner; (void)font; }
    virtual bool IsSelected() const { return fSelected; }
    virtual void SetHeight(float height) { (void)height; }
    virtual float Height() const { return 20.0f; }
    virtual void SetEnabled(bool enabled) { (void)enabled; }
    virtual bool IsEnabled() const { return true; }
    class BListView* fOwnerView = nullptr;
    int32 fIndex = -1;
    bool fSelected = false;
};

class BListView : public BView {
public:
    enum list_type {
        B_SINGLE_SELECTION_LIST = 0,
        B_MULTIPLE_SELECTION_LIST = 1
    };
    
    BListView(const char* name, list_type type = B_SINGLE_SELECTION_LIST)
        : BView(BRect(), name, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE) {}
    virtual ~BListView() = default;
    
    virtual void AddItem(BListItem* item, int32 index = -1) { (void)item; (void)index; }
    virtual BListItem* RemoveItem(int32 index) { (void)index; return nullptr; }
    virtual BListItem* RemoveItem(BListItem* item) { (void)item; return nullptr; }
    virtual BListItem* ItemAt(int32 index) const { (void)index; return nullptr; }
    virtual int32 CountItems() const { return 0; }
    virtual int32 CurrentSelection(int32 index = -1) const { (void)index; return -1; }
    virtual void Select(int32 index) { (void)index; }
    virtual void Deselect(int32 index) { (void)index; }
    virtual void DeselectAll() {}
    virtual void ScrollToSelection() {}
    virtual void InvalidateItem(int32 index) { (void)index; }
    virtual void SetSelectionMessage(BMessage* msg) { (void)msg; }
    virtual void SetInvocationMessage(BMessage* msg) { (void)msg; }
    virtual BMessage* SelectionMessage() const { return nullptr; }
    virtual BMessage* InvocationMessage() const { return nullptr; }
};

class BOutlineListView : public BListView {
public:
    BOutlineListView(const char* name) : BListView(name) {}
    virtual ~BOutlineListView() = default;
    virtual void AddItem(BListItem* item, BListItem* superItem, bool expanded = true) { (void)item; (void)superItem; (void)expanded; }
    virtual BListItem* RemoveItem(BListItem* item) { (void)item; return nullptr; }
    virtual BListItem* SuperItem(BListItem* item) const { (void)item; return nullptr; }
    virtual void SetExpanded(BListItem* item, bool expanded) { (void)item; (void)expanded; }
};

class BStringItem : public BListItem {
public:
    BStringItem(const char* text, uint32 level = 0, bool expanded = true) { (void)text; (void)level; (void)expanded; }
    virtual ~BStringItem() = default;
    virtual void DrawItem(BView* owner, BRect frame, bool complete = false) { (void)owner; (void)frame; (void)complete; }
};

#endif