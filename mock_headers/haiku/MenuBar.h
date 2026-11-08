#ifndef MOCK_MENU_BAR_H
#define MOCK_MENU_BAR_H

#include "View.h"
#include "Menu.h"

class BMenuBar : public BView {
public:
    BMenuBar(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
        : BView(frame, name, resizingMode, flags) {
        (void)frame; (void)name; (void)resizingMode; (void)flags;
    }
    BMenuBar(const char* name)
        : BView(BRect(), name, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS) {
        (void)name;
    }
    virtual ~BMenuBar() = default;
    
    virtual void AddItem(BMenuItem* item) { (void)item; }
    virtual void AddItem(BMenu* menu) { (void)menu; }
    virtual void RemoveItem(BMenuItem* item) { (void)item; }
    virtual BMenuItem* FindItem(const char* label) const { (void)label; return nullptr; }
    virtual BMenuItem* ItemAt(int32 index) const { (void)index; return nullptr; }
    virtual int32 CountItems() const { return 0; }
};

#endif