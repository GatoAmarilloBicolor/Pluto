#ifndef MOCK_WINDOW_H
#define MOCK_WINDOW_H

#include "Types.h"
#include "View.h"
#include "Handler.h"
#include "Looper.h"
#include "Message.h"
#include "Rect.h"

class BWindow : public BLooper {
public:
    BWindow(BRect frame, const char* title, window_type type, uint32 flags,
            uint32 workspace = B_CURRENT_WORKSPACE) {
        (void)frame; (void)title; (void)type; (void)flags; (void)workspace;
    }
    virtual ~BWindow() = default;
    
    virtual bool QuitRequested() { return true; }
    virtual void MessageReceived(BMessage* message) { (void)message; }
    virtual void FrameResized(float width, float height) { (void)width; (void)height; }
    virtual void ScreenChanged(BRect frame, color_space mode) { (void)frame; (void)mode; }
    virtual void WorkspaceActivated(int32 workspace, bool active) { (void)workspace; (void)active; }
    virtual void MenusBeginning() {}
    virtual void MenusEnded() {}
    
    virtual void Show() {}
    virtual void Hide() {}
    virtual bool IsHidden() const { return false; }
    virtual void Minimize(bool minimize) { (void)minimize; }
    virtual bool IsMinimized() const { return false; }
    virtual void MoveTo(float x, float y) { (void)x; (void)y; }
    virtual void ResizeTo(float width, float height) { (void)width; (void)height; }
    virtual void MoveBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void ResizeBy(float dw, float dh) { (void)dw; (void)dh; }
    virtual void SetTitle(const char* title) { (void)title; }
    virtual const char* Title() const { return ""; }
    virtual void SetPulseRate(bigtime_t rate) { (void)rate; }
    virtual void SetFlags(uint32 flags) { (void)flags; }
    virtual uint32 Flags() const { return 0; }
    virtual void SetFeel(window_feel feel) { (void)feel; }
    virtual window_feel Feel() const { return B_NORMAL_WINDOW_FEEL; }
    virtual void SetWorkspaces(uint32 workspaces) { (void)workspaces; }
    virtual uint32 Workspaces() const { return B_CURRENT_WORKSPACE; }
    virtual void Activate(bool active = true) { (void)active; }
    virtual bool IsActive() const { return false; }
    virtual void AddChild(BView* view) { (void)view; }
    virtual void RemoveChild(BView* view) { (void)view; }
    virtual BView* FindView(const char* name) { (void)name; return nullptr; }
    virtual int32 CountChildren() const { return 0; }
    virtual BView* ChildAt(int32 index) const { (void)index; return nullptr; }
    virtual BView* CurrentFocus() const { return nullptr; }
    virtual status_t Lock() { return B_OK; }
    virtual void Unlock() {}
    virtual bool IsLocked() const { return false; }
    virtual int32 CountLocks() const { return 0; }
    virtual void UpdateIfNeeded() {}
    virtual void Invalidate(BRect rect) { (void)rect; }
    virtual void Invalidate() {}
    
    enum window_type {
        B_TITLED_WINDOW = 0,
        B_DOCUMENT_WINDOW = 1,
        B_MODAL_WINDOW = 2,
        B_BORDERED_WINDOW = 3,
        B_FLOATING_WINDOW = 4,
        B_NO_BORDER_WINDOW_LOOK = 5
    };
    
    enum window_feel {
        B_NORMAL_WINDOW_FEEL = 0,
        B_MODAL_WINDOW_FEEL = 1,
        B_FLOATING_WINDOW_FEEL = 2,
        B_DOCK_WINDOW_FEEL = 3
    };
    
    static const uint32 B_CURRENT_WORKSPACE = 0;
    static const uint32 B_ALL_WORKSPACES = 0xFFFFFFFF;
};

#endif