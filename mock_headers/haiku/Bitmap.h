#ifndef MOCK_BITMAP_H
#define MOCK_BITMAP_H

#include "Types.h"
#include "Rect.h"

class BView;
class BBitmap {
public:
    BBitmap() = default;
    BBitmap(BRect bounds, color_space format) { (void)bounds; (void)format; }
    BBitmap(BRect bounds, color_space format, bool acceptViews) { (void)bounds; (void)format; (void)acceptViews; }
    BBitmap(const BMessage* archive) { (void)archive; }
    BBitmap(BMessage* archive) { (void)archive; }
    BBitmap(const BBitmap* other) { (void)other; }
    BBitmap(BBitmap* other) { (void)other; }
    virtual ~BBitmap() = default;
    
    virtual status_t InitCheck() const { return B_OK; }
    virtual BRect Bounds() const { return BRect(); }
    virtual color_space ColorSpace() const { return B_RGB32; }
    virtual int32 BitsLength() const { return 0; }
    virtual void* Bits() { return nullptr; }
    virtual const void* Bits() const { return nullptr; }
    virtual int32 BytesPerRow() const { return 0; }
    virtual bool IsValid() const { return true; }
    
    virtual bool Lock() { return true; }
    virtual void Unlock() {}
    virtual bool AddChild(BView* child) { (void)child; return true; }
    virtual bool RemoveChild(BView* child) { (void)child; return true; }
};

#endif