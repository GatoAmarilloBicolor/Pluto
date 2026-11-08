#ifndef MOCK_WINDOW_H
#define MOCK_WINDOW_H

#include "Types.h"
#include "View.h"
#include "Handler.h"
#include "Looper.h"
#include "Message.h"
#include "Rect.h"

enum window_type {
    B_TITLED_WINDOW = 0,
    B_DOCUMENT_WINDOW = 1,
    B_MODAL_WINDOW = 2,
    B_BORDERED_WINDOW = 3,
    B_FLOATING_WINDOW = 4,
    B_NO_BORDER_WINDOW_LOOK = 5
};

// Window flags
enum {
    B_NOT_RESIZABLE = 0x00000002,
    B_NOT_MOVABLE = 0x00000008,
    B_NOT_MINIMIZABLE = 0x00000010,
    B_AVOID_FOCUS = 0x00000040,
    B_WILL_ACCEPT_FIRST_CLICK = 0x00000200,
    B_CLOSE_ON_ESCAPE = 0x00000400,
    B_AUTO_UPDATE_SIZE_LIMITS = 0x00000800
};

enum window_look {
    B_BORDERED_WINDOW_LOOK = B_BORDERED_WINDOW,
    B_DOCUMENT_WINDOW_LOOK = B_DOCUMENT_WINDOW,
    B_TITLED_WINDOW_LOOK = B_TITLED_WINDOW,
    B_MODAL_WINDOW_LOOK = B_MODAL_WINDOW,
    B_FLOATING_WINDOW_LOOK = B_FLOATING_WINDOW
};

enum window_feel {
    B_NORMAL_WINDOW_FEEL = 0,
    B_MODAL_WINDOW_FEEL = 1,
    B_FLOATING_WINDOW_FEEL = 2,
    B_FLOATING_APP_WINDOW_FEEL = 3,
    B_DOCK_WINDOW_FEEL = 4,
    B_MODAL_APP_WINDOW_FEEL = 5
};

class BWindow : public BLooper {
public:
    BWindow(BRect frame, const char* title, window_type type, uint32 flags,
            uint32 workspace = B_CURRENT_WORKSPACE) {
        (void)frame; (void)title; (void)type; (void)flags; (void)workspace;
    }
    BWindow(BRect frame, const char* title, window_look look, window_feel feel, uint32 flags,
            uint32 workspace = B_CURRENT_WORKSPACE) {
        (void)frame; (void)title; (void)look; (void)feel; (void)flags; (void)workspace;
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
    virtual BView* CurrentFocus() const { return nullptr; }
    virtual status_t Lock() { return B_OK; }
    virtual void Unlock() {}
    virtual bool IsLocked() const { return false; }
    virtual int32 CountLocks() const { return 0; }
    virtual status_t LockLooper() { return B_OK; }
    virtual void UnlockLooper() {}
    virtual BMessage* CurrentMessage() { return nullptr; }
    virtual void UpdateIfNeeded() {}
    virtual void Invalidate(BRect rect) { (void)rect; }
    virtual void Invalidate() {}
    virtual BRect Bounds() const { return BRect(); }
    virtual BRect Frame() const { return BRect(); }
    
    virtual void SetSizeLimits(float minWidth, float maxWidth, float minHeight, float maxHeight) { (void)minWidth; (void)maxWidth; (void)minHeight; (void)maxHeight; }
    virtual void CenterOnScreen() {}
    virtual void CenterIn(BRect frame) { (void)frame; }
    
    static const uint32 B_CURRENT_WORKSPACE = 0;
    static const uint32 B_ALL_WORKSPACES = 0xFFFFFFFF;
};

#endif