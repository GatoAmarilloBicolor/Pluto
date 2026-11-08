#ifndef MOCK_ALERT_H
#define MOCK_ALERT_H

#include "Types.h"
#include "Window.h"

enum button_width {
    B_WIDTH_AS_USUAL = 0,
    B_WIDTH_FROM_WIDEST = 1,
    B_WIDTH_FROM_LABEL = 2
};

enum alert_type {
    B_EMPTY_ALERT = 0,
    B_INFO_ALERT = 1,
    B_IDEA_ALERT = 2,
    B_WARNING_ALERT = 3,
    B_STOP_ALERT = 4
};

enum button_spacing {
    B_OFFSET_SPACING = 0,
    B_EVEN_SPACING = 1
};

class BAlert : public BWindow {
public:
    // Unified constructor: handles both common calling conventions
    BAlert(const char* title, const char* text, const char* button1,
           const char* button2 = nullptr, const char* button3 = nullptr,
           button_width width = B_WIDTH_AS_USUAL,
           int arg7 = B_INFO_ALERT,
           int arg8 = B_OFFSET_SPACING)
        : BWindow(BRect(0,0,1,1), title, B_TITLED_WINDOW, 0) {
        (void)title; (void)text; (void)button1; (void)button2; (void)button3;
        (void)width; (void)arg7; (void)arg8;
    }
    virtual ~BAlert() = default;
    
    virtual int32 Go(void* = nullptr) { return 0; }
    virtual void SetShortcut(int32 index, int32 key) { (void)index; (void)key; }
};

#endif