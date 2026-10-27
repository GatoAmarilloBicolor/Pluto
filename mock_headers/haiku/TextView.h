#ifndef MOCK_TEXT_VIEW_H
#define MOCK_TEXT_VIEW_H

#include "View.h"
#include "Rect.h"

class BTextView : public BView {
public:
    BTextView(BRect frame, const char* name, BRect textRect,
              uint32 resizingMode, uint32 flags) {
        (void)frame; (void)name; (void)textRect; (void)resizingMode; (void)flags;
    }
    virtual ~BTextView() = default;
    
    virtual void Insert(const char* text) { (void)text; }
    virtual void Insert(int32 offset, const char* text) { (void)offset; (void)text; }
    virtual void Delete(int32 start, int32 end) { (void)start; (void)end; }
    virtual void SetText(const char* text) { (void)text; }
    virtual const char* Text() const { return ""; }
    virtual int32 TextLength() const { return 0; }
    virtual void SetFontAndColor(const BFont* font, int32 properties = 0,
                                  const rgb_color* color = nullptr) {
        (void)font; (void)properties; (void)color;
    }
    virtual void MakeEditable(bool editable) { (void)editable; }
    virtual bool IsEditable() const { return false; }
    virtual void SetAlignment(alignment align) { (void)align; }
    virtual alignment Alignment() const { return B_ALIGN_LEFT; }
    
    enum alignment {
        B_ALIGN_LEFT = 0,
        B_ALIGN_RIGHT = 1,
        B_ALIGN_CENTER = 2
    };
};

#endif