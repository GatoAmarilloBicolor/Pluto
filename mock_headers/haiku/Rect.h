#ifndef MOCK_RECT_H
#define MOCK_RECT_H

#include "Types.h"

struct BPoint {
    float x;
    float y;
    BPoint() : x(0), y(0) {}
    BPoint(float x_, float y_) : x(x_), y(y_) {}
};

struct BRect {
    float left;
    float top;
    float right;
    float bottom;
    
    BRect() : left(0), top(0), right(0), bottom(0) {}
    BRect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
    BRect(const BPoint& p1, const BPoint& p2) : left(p1.x), top(p1.y), right(p2.x), bottom(p2.y) {}
    
    float Width() const { return right - left; }
    float Height() const { return bottom - top; }
    bool IsValid() const { return right >= left && bottom >= top; }
    bool Contains(float x, float y) const { return x >= left && x <= right && y >= top && y <= bottom; }
    bool Contains(const BPoint& point) const { return Contains(point.x, point.y); }
    BRect& InsetBy(float dx, float dy) { left += dx; right -= dx; top += dy; bottom -= dy; return *this; }
    BRect& OffsetBy(float dx, float dy) { left += dx; right += dx; top += dy; bottom += dy; return *this; }
    BRect& OffsetTo(float x, float y) { float w = Width(); float h = Height(); left = x; top = y; right = left + w; bottom = top + h; return *this; }
};

struct BSize {
    float width;
    float height;
    BSize() : width(0), height(0) {}
    BSize(float w, float h) : width(w), height(h) {}
};

#endif