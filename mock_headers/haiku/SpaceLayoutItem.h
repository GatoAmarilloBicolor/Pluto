#ifndef MOCK_SPACE_LAYOUT_ITEM_H
#define MOCK_SPACE_LAYOUT_ITEM_H

#include "Types.h"
#include "Layout.h"

class BSpaceLayoutItem : public BLayoutItem {
public:
    BSpaceLayoutItem(float width, float maxWidth = B_SIZE_UNLIMITED) { (void)width; (void)maxWidth; }
    virtual ~BSpaceLayoutItem() = default;
    
    virtual void SetWidth(float width) { (void)width; }
    virtual float Width() const { return 0; }
    virtual void SetMaxWidth(float width) { (void)width; }
    virtual float MaxWidth() const { return 0; }
    
    static BSpaceLayoutItem* CreateHorizontalStrut(float width) { (void)width; return nullptr; }
    static BSpaceLayoutItem* CreateVerticalStrut(float height) { (void)height; return nullptr; }
    static BSpaceLayoutItem* CreateGlue() { return nullptr; }
};

#endif