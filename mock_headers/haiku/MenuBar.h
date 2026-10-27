#ifndef MOCK_MENU_BAR_H
#define MOCK_MENU_BAR_H

#include "Menu.h"

class BMenuBar : public BMenu {
public:
    BMenuBar(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
        : BMenu(name) {
        (void)frame; (void)resizingMode; (void)flags;
    }
    virtual ~BMenuBar() = default;
};

#endif