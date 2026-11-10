#ifndef MOCK_REGION_H
#define MOCK_REGION_H

#include "Types.h"
#include "Rect.h"

class BRegion {
public:
    BRegion() = default;
    BRegion(const BRegion& other) { (void)other; }
    BRegion(const BRect& rect) { (void)rect; }
    virtual ~BRegion() = default;
    
    BRegion& operator=(const BRegion& other) { (void)other; return *this; }
    BRegion& operator|=(const BRegion& other) { (void)other; return *this; }
    BRegion& operator&=(const BRegion& other) { (void)other; return *this; }
    BRegion& operator-=(const BRegion& other) { (void)other; return *this; }
    BRegion& operator^=(const BRegion& other) { (void)other; return *this; }
    
    bool Includes(const BPoint& point) const { (void)point; return false; }
    bool Includes(const BRect& rect) const { (void)rect; return false; }
    bool Intersects(const BRect& rect) const { (void)rect; return false; }
    bool Intersects(const BRegion& region) const { (void)region; return false; }
    
    void Set(const BRect& rect) { (void)rect; }
    void Set(const BRegion& region) { (void)region; }
    void MakeEmpty() {}
    
    BRect Frame() const { return BRect(); }
    int32 CountRects() const { return 0; }
    BRect RectAt(int32 index) const { (void)index; return BRect(); }
};

#endif