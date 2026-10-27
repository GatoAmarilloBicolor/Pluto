#ifndef MOCK_VIEW_H
#define MOCK_VIEW_H

#include "Types.h"
#include "Handler.h"
#include "Rect.h"
#include "Message.h"

struct BFont;

struct rgb_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};

class BWindow;  // forward declaration

class BView : public BHandler {
public:
    BView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
        : BHandler(name) {
        (void)frame; (void)resizingMode; (void)flags;
    }
    virtual ~BView() = default;
    
    virtual void Draw(BRect updateRect) { (void)updateRect; }
    virtual void FrameResized(float width, float height) { (void)width; (void)height; }
    virtual void MouseDown(BPoint where) { (void)where; }
    virtual void MouseUp(BPoint where) { (void)where; }
    virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* msg) { (void)where; (void)transit; (void)msg; }
    virtual void KeyDown(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    virtual void KeyUp(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    virtual void MessageReceived(BMessage* message) { (void)message; }
    virtual void AttachedToWindow() {}
    virtual void DetachedFromWindow() {}
    virtual void AllAttached() {}
    virtual void AllDetached() {}
    virtual void MakeFocus(bool focused = true) { (void)focused; }
    virtual bool IsFocus() const { return false; }
    virtual void ScrollTo(BPoint where) { (void)where; }
    virtual void ScrollBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void Invalidate(BRect rect) { (void)rect; }
    virtual void Invalidate() {}
    virtual void AddChild(BView* view) { (void)view; }
    virtual void RemoveChild(BView* view) { (void)view; }
    virtual BView* FindView(const char* name) { (void)name; return nullptr; }
    virtual int32 CountChildren() const { return 0; }
    virtual BView* ChildAt(int32 index) const { (void)index; return nullptr; }
    virtual BRect Bounds() const { return BRect(); }
    virtual BRect Frame() const { return BRect(); }
    virtual void MoveTo(float x, float y) { (void)x; (void)y; }
    virtual void ResizeTo(float width, float height) { (void)width; (void)height; }
    virtual void MoveBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void ResizeBy(float dw, float dh) { (void)dw; (void)dh; }
    virtual void SetViewColor(rgb_color color) { (void)color; }
    virtual rgb_color ViewColor() const { return {0,0,0,255}; }
    virtual void SetLowColor(rgb_color color) { (void)color; }
    virtual rgb_color LowColor() const { return {0,0,0,255}; }
    virtual void SetHighColor(rgb_color color) { (void)color; }
    virtual rgb_color HighColor() const { return {0,0,0,255}; }
    virtual void SetFont(const BFont* font) { (void)font; }
    virtual void GetFont(BFont* font) const { (void)font; }
    virtual void SetFontSize(float size) { (void)size; }
    virtual float FontSize() const { return 12.0f; }
    virtual void PushState() {}
    virtual void PopState() {}
    virtual status_t LockLooper() { return B_OK; }
    virtual void UnlockLooper() {}
    virtual BLooper* Looper() const { return nullptr; }
    virtual BWindow* Window() const { return nullptr; }
    virtual void SetTarget(BHandler* target) { (void)target; }
    virtual BHandler* Target() const { return nullptr; }
    
    enum drawing_mode {
        B_OP_COPY = 0,
        B_OP_OVER = 1,
        B_OP_ERASE = 2,
        B_OP_SELECT = 3,
        B_OP_MIN = 4,
        B_OP_MAX = 5,
        B_OP_ADD = 6,
        B_OP_SUBTRACT = 7,
        B_OP_BLEND = 8
    };
    
    enum {
        B_FOLLOW_LEFT = 1,
        B_FOLLOW_RIGHT = 2,
        B_FOLLOW_TOP = 4,
        B_FOLLOW_BOTTOM = 8,
        B_FOLLOW_H_CENTER = 16,
        B_FOLLOW_V_CENTER = 32,
        B_FOLLOW_ALL = B_FOLLOW_LEFT | B_FOLLOW_RIGHT | B_FOLLOW_TOP | B_FOLLOW_BOTTOM,
        B_FOLLOW_LEFT_TOP = B_FOLLOW_LEFT | B_FOLLOW_TOP,
        B_FOLLOW_RIGHT_TOP = B_FOLLOW_RIGHT | B_FOLLOW_TOP,
        B_FOLLOW_LEFT_BOTTOM = B_FOLLOW_LEFT | B_FOLLOW_BOTTOM,
        B_FOLLOW_RIGHT_BOTTOM = B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM
    };
};

#endif