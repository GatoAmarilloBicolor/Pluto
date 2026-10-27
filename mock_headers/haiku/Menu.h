#ifndef MOCK_MENU_H
#define MOCK_MENU_H

#include "View.h"
#include "Rect.h"
#include "MenuItem.h"

class BMenu : public BView {
public:
    BMenu(const char* title, layout_type layout = B_ITEMS_IN_COLUMN)
        : BView(BRect(), title, 0, 0) {
        (void)title; (void)layout;
    }
    virtual ~BMenu() = default;
    
    virtual void AddItem(BMenuItem* item) { (void)item; }
    virtual void RemoveItem(BMenuItem* item) { (void)item; }
    virtual BMenuItem* FindItem(const char* label) const { (void)label; return nullptr; }
    virtual BMenuItem* ItemAt(int32 index) const { (void)index; return nullptr; }
    virtual int32 CountItems() const { return 0; }
    
    enum layout_type {
        B_ITEMS_IN_ROW = 0,
        B_ITEMS_IN_COLUMN = 1,
        B_ITEMS_IN_MATRIX = 2
    };
};

#endif