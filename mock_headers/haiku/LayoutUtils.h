#ifndef MOCK_LAYOUT_UTILS_H
#define MOCK_LAYOUT_UTILS_H

#include "Types.h"
#include "View.h"
#include "Rect.h"
#include "Size.h"

class BLayoutUtils {
public:
    static void SetFontSize(BView* view, float size) { (void)view; (void)size; }
    static float FontSize(const BView* view) { (void)view; return 12.0f; }
    static BSize ComposeLayoutItem(BView* view) { (void)view; return BSize(0, 0); }
};

#endif