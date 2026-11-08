#ifndef MOCK_BITMAP_STREAM_H
#define MOCK_BITMAP_STREAM_H

#include "Types.h"
#include "Bitmap.h"
#include "DataIO.h"
#include "Rect.h"

class BBitmapStream : public BPositionIO {
public:
    BBitmapStream(BBitmap* bitmap) { (void)bitmap; }
    virtual ~BBitmapStream() = default;
    
    virtual BBitmap* DetachBitmap(BBitmap** bitmap) { (void)bitmap; return nullptr; }
    virtual status_t SetBitmap(const BBitmap* bitmap) { (void)bitmap; return B_OK; }
    virtual const BBitmap* Bitmap() const { return nullptr; }
};

#endif