#ifndef MOCK_FONT_H
#define MOCK_FONT_H

#include "Types.h"

struct BFont {
    BFont() = default;
    BFont(const BFont&) = default;
    virtual ~BFont() = default;
    
    virtual void SetSize(float size) { (void)size; }
    virtual float Size() const { return 12.0f; }
    virtual void SetFamilyAndStyle(const char* family, const char* style) {
        (void)family; (void)style;
    }
    virtual void GetFamilyAndStyle(char* family, char* style) const {
        if (family) *family = '\0';
        if (style) *style = '\0';
    }
    virtual float StringWidth(const char* string, int32 len = -1) const {
        (void)string; (void)len; return 0;
    }
};

#define be_plain_font (new BFont())

#endif