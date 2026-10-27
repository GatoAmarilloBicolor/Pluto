#ifndef MOCK_BITMAP_H
#define MOCK_BITMAP_H

#include "Types.h"
#include "Rect.h"

class BBitmap {
public:
    BBitmap() = default;
    BBitmap(BRect bounds, color_space format) { (void)bounds; (void)format; }
    virtual ~BBitmap() = default;
    
    virtual status_t InitCheck() const { return B_OK; }
    virtual BRect Bounds() const { return BRect(); }
    virtual color_space ColorSpace() const { return B_RGB32; }
    virtual int32 BitsLength() const { return 0; }
    virtual void* Bits() { return nullptr; }
    virtual const void* Bits() const { return nullptr; }
    virtual int32 BytesPerRow() const { return 0; }
};

#endif