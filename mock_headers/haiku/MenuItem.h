#ifndef MOCK_MENU_ITEM_H
#define MOCK_MENU_ITEM_H

#include "Invoker.h"

class BMenuItem : public BInvoker {
public:
    BMenuItem(const char* label, BMessage* message, char shortcut = 0,
              uint32 modifiers = 0)
        : BInvoker(message, nullptr) {
        (void)label; (void)shortcut; (void)modifiers;
    }
    virtual ~BMenuItem() = default;
    
    virtual void SetLabel(const char* label) { (void)label; }
    virtual const char* Label() const { return ""; }
    virtual void SetEnabled(bool enabled) { (void)enabled; }
    virtual bool IsEnabled() const { return true; }
    virtual void SetMarked(bool marked) { (void)marked; }
    virtual bool IsMarked() const { return false; }
    virtual BMenu* Menu() const { return nullptr; }
};

#endif