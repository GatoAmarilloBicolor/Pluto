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
    BView(BMessage* archive) : BHandler("archived") {
        (void)archive;
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
    virtual void SetViewColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) { (void)red; (void)green; (void)blue; (void)alpha; }
    virtual rgb_color ViewColor() const { return {0,0,0,255}; }
    virtual void SetLowColor(rgb_color color) { (void)color; }
    virtual void SetLowColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) { (void)red; (void)green; (void)blue; (void)alpha; }
    virtual rgb_color LowColor() const { return {0,0,0,255}; }
    virtual void SetHighColor(rgb_color color) { (void)color; }
    virtual void SetHighColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) { (void)red; (void)green; (void)blue; (void)alpha; }
    virtual void SetHighUIColor(color_which which) { (void)which; }
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
    virtual void InvalidateLayout(bool layoutRoot) { (void)layoutRoot; }
    virtual void SetViewUIColor(color_which which) { (void)which; }
    virtual void SetLowUIColor(color_which which) { (void)which; }
    virtual void FillRect(BRect rect) { (void)rect; }
    virtual void StrokeRect(BRect rect) { (void)rect; }
    virtual void TruncateString(BString* string, uint32 mode, float maxWidth) { (void)string; (void)mode; (void)maxWidth; }
    virtual void MovePenTo(BPoint point) { (void)point; }
    virtual void MovePenTo(float x, float y) { (void)x; (void)y; }
    virtual void MovePenBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void DrawString(const char* string) { (void)string; }
    virtual void DrawString(const char* string, BPoint where) { (void)string; (void)where; }
    virtual void SetPenSize(float size) { (void)size; }
    virtual float PenSize() const { return 1.0f; }
    virtual void StrokeLine(BPoint start, BPoint end) { (void)start; (void)end; }
    virtual void StrokeBezier(BPoint p1, BPoint p2, BPoint p3, BPoint p4) { (void)p1; (void)p2; (void)p3; (void)p4; }
    virtual void StrokeBezier(const BPoint points[4]) { (void)points; }
    virtual void FillEllipse(BPoint center, float rx, float ry) { (void)center; (void)rx; (void)ry; }
    virtual void FillEllipse(BRect rect) { (void)rect; }
    virtual void StrokeEllipse(BPoint center, float rx, float ry) { (void)center; (void)rx; (void)ry; }
    virtual void FillTriangle(BPoint p1, BPoint p2, BPoint p3) { (void)p1; (void)p2; (void)p3; }
    virtual void FillRoundRect(BRect rect, float rx, float ry) { (void)rect; (void)rx; (void)ry; }
    virtual void StrokeRoundRect(BRect rect, float rx, float ry) { (void)rect; (void)rx; (void)ry; }
    virtual float StringWidth(const char* string) const { (void)string; return 0; }
    virtual void GetMouse(BPoint* where, uint32* buttons = nullptr, bool includeModifiers = false) const { (void)where; (void)buttons; (void)includeModifiers; }
    virtual void SetDrawingMode(drawing_mode mode) { (void)mode; }
    virtual void SetBlendingMode(int alphaMode, int overlayMode) { (void)alphaMode; (void)overlayMode; }
    virtual drawing_mode DrawingMode() const { return B_OP_COPY; }
    virtual void DrawBitmap(const BBitmap* bitmap, BPoint where) { (void)bitmap; (void)where; }
    virtual void DrawBitmap(const BBitmap* bitmap, BRect srcRect, BRect dstRect) { (void)bitmap; (void)srcRect; (void)dstRect; }
    virtual void ConvertToScreen(BPoint* point) const { (void)point; }
    virtual BPoint ConvertToScreen(BPoint point) const { return point; }
    virtual void Sync() {}
    virtual void DragMessage(const BMessage* message, BBitmap* dragBitmap,
                              drawing_mode mode = B_OP_COPY, BPoint offset = BPoint()) {
        (void)message; (void)dragBitmap; (void)mode; (void)offset;
    }
    virtual void SetMouseEventMask(uint32 mask, uint32 options = 0) { (void)mask; (void)options; }
    virtual BView* Parent() const { return nullptr; }
    virtual void SetToolTip(const char* tip) { (void)tip; }
    virtual void SetTargetForItems(BHandler* target) { (void)target; }

    enum {
        B_FOLLOW_NONE = 0,
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

// Global constants (as in real Haiku headers)
enum {
    B_FOLLOW_LEFT_G = 1,
    B_FOLLOW_RIGHT_G = 2,
    B_FOLLOW_LEFT_RIGHT_G = B_FOLLOW_LEFT_G | B_FOLLOW_RIGHT_G,
    B_FOLLOW_TOP_G = 4,
    B_FOLLOW_BOTTOM_G = 8,
    B_FOLLOW_H_CENTER_G = 16,
    B_FOLLOW_V_CENTER_G = 32,
    B_FOLLOW_ALL_G = B_FOLLOW_LEFT_G | B_FOLLOW_RIGHT_G | B_FOLLOW_TOP_G | B_FOLLOW_BOTTOM_G,
    B_FOLLOW_ALL_SIDES_G = B_FOLLOW_ALL_G,
    B_FOLLOW_LEFT_TOP_G = B_FOLLOW_LEFT_G | B_FOLLOW_TOP_G,
    B_FOLLOW_RIGHT_TOP_G = B_FOLLOW_RIGHT_G | B_FOLLOW_TOP_G,
    B_FOLLOW_LEFT_BOTTOM_G = B_FOLLOW_LEFT_G | B_FOLLOW_BOTTOM_G,
    B_FOLLOW_RIGHT_BOTTOM_G = B_FOLLOW_RIGHT_G | B_FOLLOW_BOTTOM_G
};

enum {
    B_WILL_DRAW_G = 1,
    B_FRAME_EVENTS_G = 2,
    B_NAVIGABLE_G = 4,
    B_NAVIGABLE_JUMP_G = 8,
    B_SUBPIXEL_PRECISE_G = 16,
    B_FULL_UPDATE_ON_RESIZE_G = 32,
    B_PULSE_NEEDED_G = 64
};

#define B_FOLLOW_LEFT B_FOLLOW_LEFT_G
#define B_FOLLOW_RIGHT B_FOLLOW_RIGHT_G
#define B_FOLLOW_LEFT_RIGHT B_FOLLOW_LEFT_RIGHT_G
#define B_FOLLOW_TOP B_FOLLOW_TOP_G
#define B_FOLLOW_BOTTOM B_FOLLOW_BOTTOM_G
#define B_FOLLOW_H_CENTER B_FOLLOW_H_CENTER_G
#define B_FOLLOW_V_CENTER B_FOLLOW_V_CENTER_G
#define B_FOLLOW_ALL B_FOLLOW_ALL_G
#define B_FOLLOW_ALL_SIDES B_FOLLOW_ALL_SIDES_G
#define B_FOLLOW_LEFT_TOP B_FOLLOW_LEFT_TOP_G
#define B_FOLLOW_RIGHT_TOP B_FOLLOW_RIGHT_TOP_G
#define B_FOLLOW_LEFT_BOTTOM B_FOLLOW_LEFT_BOTTOM_G
#define B_FOLLOW_RIGHT_BOTTOM B_FOLLOW_RIGHT_BOTTOM_G

#define B_WILL_DRAW B_WILL_DRAW_G
#define B_FRAME_EVENTS B_FRAME_EVENTS_G
#define B_NAVIGABLE B_NAVIGABLE_G
#define B_NAVIGABLE_JUMP B_NAVIGABLE_JUMP_G
#define B_SUBPIXEL_PRECISE B_SUBPIXEL_PRECISE_G
#define B_FULL_UPDATE_ON_RESIZE B_FULL_UPDATE_ON_RESIZE_G
#define B_PULSE_NEEDED B_PULSE_NEEDED_G

#endif