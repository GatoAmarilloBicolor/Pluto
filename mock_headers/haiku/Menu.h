#ifndef MOCK_MENU_H
#define MOCK_MENU_H

#include "View.h"
#include "Rect.h"
#include "MenuItem.h"

enum layout_type {
    B_ITEMS_IN_ROW = 0,
    B_ITEMS_IN_COLUMN = 1,
    B_ITEMS_IN_MATRIX = 2
};

class BMenu : public BView {
public:
    BMenu(const char* title, layout_type layout = B_ITEMS_IN_COLUMN)
        : BView(BRect(), title, 0, 0) {
        (void)title; (void)layout;
    }
    virtual ~BMenu() = default;
    
    virtual void AddItem(BMenuItem* item) { (void)item; }
    virtual void AddItem(BMenu* submenu) { (void)submenu; }
    virtual void RemoveItem(BMenuItem* item) { (void)item; }
    virtual BMenuItem* RemoveItem(int32 index) { (void)index; return nullptr; }
    virtual BMenuItem* FindItem(const char* label) const { (void)label; return nullptr; }
    virtual BMenuItem* FindMarked() const { return nullptr; }
    virtual BMenuItem* ItemAt(int32 index) const { (void)index; return nullptr; }
    virtual int32 CountItems() const { return 0; }
    virtual void AddSeparatorItem() { AddItem(static_cast<BMenuItem*>(nullptr)); }
};

class BPopUpMenu : public BMenu {
public:
    BPopUpMenu(const char* name, bool radioMode = false, bool labelFromMarked = true)
        : BMenu(name, B_ITEMS_IN_COLUMN) {
        (void)radioMode; (void)labelFromMarked;
    }
    BPopUpMenu(const char* title, bool radioMode, bool labelFromMarked, layout_type layout)
        : BMenu(title, layout) {
        (void)title; (void)radioMode; (void)labelFromMarked; (void)layout;
    }
    virtual ~BPopUpMenu() = default;
    
    virtual void SetTargetForItems(BHandler* target) { (void)target; }
    virtual void Go(BPoint screenWhere, bool async = false, bool autoClose = true, bool centered = false) {
        (void)screenWhere; (void)async; (void)autoClose; (void)centered;
    }
    virtual void SetBorder(uint32 border) { (void)border; }
    virtual void SetFont(const BFont* font) { (void)font; }
    virtual void SetFontSize(float size) { (void)size; }
};

class BSeparatorItem : public BMenuItem {
public:
    BSeparatorItem() : BMenuItem("", nullptr) {}
    virtual ~BSeparatorItem() = default;
};

#endif