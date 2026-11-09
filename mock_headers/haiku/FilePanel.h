#ifndef MOCK_FILE_PANEL_H
#define MOCK_FILE_PANEL_H

#include "Types.h"
#include "Window.h"
#include "Messenger.h"
#include "Message.h"
#include "Entry.h"
#include "Rect.h"

class BRefFilter {
public:
    BRefFilter() = default;
    virtual ~BRefFilter() = default;
    virtual bool Filter(const entry_ref* ref, BNode* node, struct stat_beos* st, const char* filetype) {
        (void)ref; (void)node; (void)st; (void)filetype; return true;
    }
};

enum file_panel_mode {
    B_OPEN_PANEL = 0,
    B_SAVE_PANEL = 1
};

enum button_which {
    B_DEFAULT_BUTTON = 0,
    B_CANCEL_BUTTON = 1
};

class BFilePanel : public BWindow {
public:
    BFilePanel(file_panel_mode mode = B_OPEN_PANEL,
               BMessenger* target = nullptr,
               const entry_ref* startDirectory = nullptr,
               uint32 nodeFlavors = B_FILE_NODE,
               bool allowMultipleSelection = false,
               BMessage* message = nullptr,
               BRefFilter* filter = nullptr,
               bool modal = false,
               bool hideWhenDone = true)
        : BWindow(BRect(), "FilePanel", B_TITLED_WINDOW, B_NOT_ZOOMABLE) {
        (void)mode; (void)target; (void)startDirectory; (void)nodeFlavors;
        (void)allowMultipleSelection; (void)message; (void)filter; (void)modal; (void)hideWhenDone;
    }
    virtual ~BFilePanel() = default;
    
    virtual void Show() {}
    virtual void Hide() {}
    virtual void SetButtonLabel(button_which which, const char* label) { (void)which; (void)label; }
    virtual const char* ButtonLabel(button_which which) const { (void)which; return ""; }
    virtual void SetSaveText(const char* text) { (void)text; }
    virtual BWindow* Window() { return this; }
};

#endif