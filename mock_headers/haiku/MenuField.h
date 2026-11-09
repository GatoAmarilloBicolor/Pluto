#ifndef MOCK_MENU_FIELD_H
#define MOCK_MENU_FIELD_H

#include "Control.h"
#include "Menu.h"

class BMenuField : public BControl {
public:
    BMenuField(const char* label, BMenu* menu)
        : BControl(BRect(), "", label, nullptr, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {
        (void)menu;
    }
    BMenuField(const char* name, const char* label, BMenu* menu)
        : BControl(BRect(), name, label, nullptr, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW) {
        (void)menu;
    }
    BMenuField(BRect frame, const char* name, const char* label, BMenu* menu,
               uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP,
               uint32 flags = B_WILL_DRAW | B_NAVIGABLE)
        : BControl(frame, name, label, nullptr, resizingMode, flags) {
        (void)menu;
    }
    virtual ~BMenuField() = default;

    virtual BMenu* Menu() const { return nullptr; }
    virtual void SetMenu(BMenu* menu) { (void)menu; }
    virtual void SetDivider(float x) { (void)x; }
    virtual float Divider() const { return 0.0f; }
    virtual void SetLabel(const char* label) { (void)label; }
    virtual const char* Label() const { return ""; }

    // Layout items for GroupLayoutBuilder
    class BLayoutItem* CreateLabelLayoutItem() { return nullptr; }
    class BLayoutItem* CreateMenuBarLayoutItem() { return nullptr; }
};

#endif