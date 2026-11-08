#ifndef MOCK_TAB_VIEW_H
#define MOCK_TAB_VIEW_H

#include "Types.h"
#include "View.h"
#include "Rect.h"

class BTab;

class BTabView : public BView {
public:
    BTabView(BRect frame, const char* name, uint32 resizingMode = B_FOLLOW_ALL_SIDES, uint32 flags = B_WILL_DRAW)
        : BView(frame, name, resizingMode, flags) {}
    BTabView(const char* name, uint32 flags = B_WILL_DRAW)
        : BView(BRect(), name, B_FOLLOW_ALL_SIDES, flags) {}
    virtual ~BTabView() = default;
    
    virtual void AddTab(BView* tabView, BTab* tab = nullptr) { (void)tabView; (void)tab; }
    virtual void AddTab(BView* tabView, const char* tabLabel) { (void)tabView; (void)tabLabel; }
    virtual BTab* RemoveTab(BTab* tab) { (void)tab; return nullptr; }
    virtual BTab* RemoveTab(int32 index) { (void)index; return nullptr; }
    virtual int32 CountTabs() const { return 0; }
    virtual BTab* TabAt(int32 index) const { (void)index; return nullptr; }
    virtual void Select(int32 index) { (void)index; }
    virtual int32 Selection() const { return -1; }
    virtual BRect TabFrame(int32 index) const { (void)index; return BRect(); }
    virtual void SetBorder(uint32 border) { (void)border; }
    virtual BView* ViewForTab(int32 index) const { (void)index; return nullptr; }
    virtual BView* ViewForTab(BTab* tab) const { (void)tab; return nullptr; }
};

class BTab {
public:
    BTab(BView* view, const char* label) { (void)view; (void)label; }
    BTab(int32 index) { (void)index; }
    virtual ~BTab() = default;
    virtual void SetLabel(const char* label) { (void)label; }
    virtual const char* Label() const { return ""; }
};

#endif