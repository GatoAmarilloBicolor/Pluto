#ifndef MOCK_TEXT_VIEW_H
#define MOCK_TEXT_VIEW_H

#include "View.h"
#include "Rect.h"
#include "Types.h"

class BTextView : public BView {
public:
    BTextView(BRect frame, const char* name, BRect textRect,
              uint32 resizingMode, uint32 flags)
        : BView(frame, name, resizingMode, flags) {
        (void)textRect;
    }
    // Convenience constructor used by ChatColors.h
    BTextView(const char* name)
        : BView(BRect(), name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED) {}
    virtual ~BTextView() = default;
    
    virtual void Insert(const char* text) { (void)text; }
    virtual void Insert(int32 offset, const char* text) { (void)offset; (void)text; }
    virtual void Insert(int32 offset, const char* text, int32 length) { (void)offset; (void)text; (void)length; }
    virtual void Delete(int32 start, int32 end) { (void)start; (void)end; }
    virtual void SetText(const char* text) { (void)text; }
    virtual void SetText(const char* text, int32 length) { (void)text; (void)length; }
    virtual const char* Text() const { return ""; }
    virtual int32 TextLength() const { return 0; }
    virtual void SetFontAndColor(const BFont* font, uint32 properties = 0,
                                  const rgb_color* color = nullptr) {
        (void)font; (void)properties; (void)color;
    }
    virtual void SetFontAndColor(int32 start, int32 end, const BFont* font,
                                  uint32 properties = 0, const rgb_color* color = nullptr) {
        (void)start; (void)end; (void)font; (void)properties; (void)color;
    }
    virtual void MakeEditable(bool editable) { (void)editable; }
    virtual bool IsEditable() const { return false; }
    virtual void SetAlignment(alignment align) { (void)align; }
    virtual alignment Alignment() const { return B_ALIGN_LEFT; }
    
    // Missing methods from ChatColors.h
    virtual void SetStylable(bool stylable) { (void)stylable; }
    virtual void SetWordWrap(bool wrap) { (void)wrap; }
    virtual void SetInsets(float left, float top, float right, float bottom) { (void)left; (void)top; (void)right; (void)bottom; }
    virtual int32 OffsetAt(BPoint point) const { (void)point; return 0; }
    virtual void MouseDown(BPoint point) { (void)point; }
    virtual void MessageReceived(BMessage* msg) { (void)msg; }
    virtual void Select(int32 start, int32 end) { (void)start; (void)end; }
    virtual void Select(int32 start) { (void)start; }
    virtual void ScrollToSelection() {}
    virtual void ScrollToOffset(int32 offset) { (void)offset; }
    virtual int32 LineAt(int32 offset) const { (void)offset; return 0; }
    virtual BRect LineBounds(int32 line) const { (void)line; return BRect(); }
    virtual int32 CountLines() const { return 0; }
    virtual void GetFontAndColor(int32 offset, BFont* font, int32* properties, rgb_color* color) const { (void)offset; (void)font; (void)properties; (void)color; }
    virtual void GetFontAndColor(int32 offset, BFont* font) const { (void)offset; (void)font; }
    virtual void SetLowUIColor(color_which which) { (void)which; }
    virtual BPoint PointAt(int32 offset) const { (void)offset; return BPoint(); }
    virtual void ScrollTo(float x, float y) { (void)x; (void)y; }
    virtual void SelectAll() {}
    virtual BScrollBar* ScrollBar(int32 which) const { (void)which; return nullptr; }
};

#endif