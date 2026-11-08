#ifndef MOCK_SCREEN_H
#define MOCK_SCREEN_H

#include "Types.h"
#include "Rect.h"

class BScreen {
public:
    BScreen() = default;
    BScreen(BWindow* window) { (void)window; }
    virtual ~BScreen() = default;
    
    virtual BRect Frame() const { return BRect(0, 0, 1920, 1080); }
    virtual BRect Frame(int32 workspace) const { (void)workspace; return BRect(0, 0, 1920, 1080); }
    virtual color_space ColorSpace() const { return B_RGBA32; }
    virtual uint32 ColorSpace(int32 workspace) const { (void)workspace; return B_RGBA32; }
    virtual BRect Frame(int32 workspace, BRect* frame) const { (void)workspace; if (frame) *frame = BRect(0, 0, 1920, 1080); return BRect(0, 0, 1920, 1080); }
};

#endif