#ifndef MOCK_VIEW_H
#define MOCK_VIEW_H

#include "Types.h"
#include "Handler.h"
#include "Rect.h"
#include "Message.h"
#include "InterfaceDefs.h"

struct BFont;

class BShape {
public:
    BShape() = default;
    virtual ~BShape() = default;
};

class BBitmap;  // forward declaration
class BWindow;  // forward declaration

class BView : public virtual BHandler {
public:
    BView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
        : BHandler(name) {
        (void)frame; (void)resizingMode; (void)flags;
    }
    BView(const char* name, uint32 flags)
        : BHandler(name) {
        (void)flags;
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
    virtual status_t Archive(BMessage* archive, bool deep = true) const { (void)archive; (void)deep; return B_OK; }
    virtual void ScrollTo(BPoint where) { (void)where; }
    virtual void ScrollBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void Invalidate(BRect rect) { (void)rect; }
    virtual void Invalidate() {}
    virtual void Hide() {}
    virtual void Show() {}
    virtual bool IsHidden() const { return false; }
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
    virtual void SetTarget(BView* view, BHandler* target) { (void)view; (void)target; }
    virtual BHandler* Target() const { return nullptr; }
    virtual void SetLabel(const char* label) { (void)label; }
    virtual const char* Label() const { return ""; }
    virtual BSize MinSize() { return BSize(0, 0); }
    virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED); }
    virtual BSize PreferredSize() { return BSize(0, 0); }
    virtual void SetExplicitMinSize(BSize size) { (void)size; }
    virtual void SetExplicitMaxSize(BSize size) { (void)size; }
    virtual void SetExplicitPreferredSize(BSize size) { (void)size; }
    virtual void InvalidateLayout() {}
    virtual void SetViewUIColor(color_which which) { (void)which; }
    virtual void SetLowUIColor(color_which which) { (void)which; }
    virtual void FillRect(BRect rect) { (void)rect; }
    virtual void StrokeRect(BRect rect) { (void)rect; }
    virtual BPoint ConvertToScreen(BPoint point) const { return point; }
    virtual void ConvertToScreen(BPoint* point) { (void)point; }
    virtual void ConvertToScreen(BPoint& point) { (void)point; }
    virtual void GetMouse(BPoint* where, uint32* buttons, bool async = false) const { if (where) *where = BPoint(); if (buttons) *buttons = 0; (void)async; }
    virtual void DrawBitmap(BBitmap* bitmap, BPoint where, uint32 flags = B_OP_COPY) { (void)bitmap; (void)where; (void)flags; }
    virtual void DrawBitmap(BBitmap* bitmap, BRect dest, BRect src, uint32 flags = B_OP_COPY) { (void)bitmap; (void)dest; (void)src; (void)flags; }
    virtual BScrollBar* ScrollBar(int32 which) const { (void)which; return nullptr; }
    virtual void DragMessage(BMessage* message, BBitmap* dragBitmap, drawing_mode mode, BPoint offset) { (void)message; (void)dragBitmap; (void)mode; (void)offset; }
    
    virtual void SetDrawingMode(drawing_mode mode) { (void)mode; }
    virtual drawing_mode DrawingMode() const { return B_OP_COPY; }
    virtual void DrawString(const char* string, BPoint where = BPoint()) { (void)string; (void)where; }
    virtual void Sync() {}
    
    enum {
        B_FOLLOW_LEFT = 1,
        B_FOLLOW_RIGHT = 2,
        B_FOLLOW_LEFT_RIGHT = B_FOLLOW_LEFT | B_FOLLOW_RIGHT,
        B_FOLLOW_TOP = 4,
        B_FOLLOW_BOTTOM = 8,
        B_FOLLOW_H_CENTER = 16,
        B_FOLLOW_V_CENTER = 32,
        B_FOLLOW_ALL = B_FOLLOW_LEFT | B_FOLLOW_RIGHT | B_FOLLOW_TOP | B_FOLLOW_BOTTOM,
        B_FOLLOW_ALL_SIDES = B_FOLLOW_ALL,
        B_FOLLOW_LEFT_TOP = B_FOLLOW_LEFT | B_FOLLOW_TOP,
        B_FOLLOW_RIGHT_TOP = B_FOLLOW_RIGHT | B_FOLLOW_TOP,
        B_FOLLOW_LEFT_BOTTOM = B_FOLLOW_LEFT | B_FOLLOW_BOTTOM,
        B_FOLLOW_RIGHT_BOTTOM = B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM
    };

    // View flags
    enum {
        B_WILL_DRAW = 1,
        B_FRAME_EVENTS = 2,
        B_NAVIGABLE = 4,
        B_NAVIGABLE_JUMP = 8,
        B_SUBPIXEL_PRECISE = 16,
        B_FULL_UPDATE_ON_RESIZE = 32,
        B_PULSE_NEEDED = 64
    };
};

#endif