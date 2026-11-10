#ifndef MOCK_CONTROL_LOOK_H
#define MOCK_CONTROL_LOOK_H

#include "Types.h"
#include "View.h"

class BControlLook {
public:
    static rgb_color HighlightColor(rgb_color base) { (void)base; return {255,255,255,255}; }
    static rgb_color ShadowColor(rgb_color base) { (void)base; return {0,0,0,255}; }
    static void DrawTextControlBorder(BView* view, BRect rect, BRect clip, const rgb_color& base, uint32 flags) { (void)view; (void)rect; (void)clip; (void)base; (void)flags; }
    static void DrawButtonBackground(BView* view, BRect rect, BRect clip, const rgb_color& base, uint32 flags, orientation orient) { (void)view; (void)rect; (void)clip; (void)base; (void)flags; (void)orient; }
    static void DrawBorder(BView* view, BRect rect, BRect clip, const rgb_color& base, uint32 flags) { (void)view; (void)rect; (void)clip; (void)base; (void)flags; }
    static float DefaultLabelSpacing() { return 5.0f; }
    static float DefaultTextSpacing() { return 2.0f; }
};

#endif