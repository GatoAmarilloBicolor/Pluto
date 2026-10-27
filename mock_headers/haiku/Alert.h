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

class BAlert : public BWindow {
public:
    BAlert(const char* title, const char* text, const char* button1,
           const char* button2 = nullptr, const char* button3 = nullptr,
           button_width width = B_WIDTH_AS_USUAL,
           alert_type type = B_INFO_ALERT)
        : BWindow(BRect(0,0,1,1), title, B_TITLED_WINDOW, 0) {
        (void)title; (void)text; (void)button1; (void)button2; (void)button3;
        (void)width; (void)type;
    }
    virtual ~BAlert() = default;
    
    virtual void Go(void* invoker = nullptr) { (void)invoker; }
    virtual int32 Go() { return 0; }
};

#endif