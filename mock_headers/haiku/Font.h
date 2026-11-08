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
    virtual void SetFace(uint16 face) { (void)face; }
    virtual uint16 Face() const { return 0; }
};

// Font faces
const uint16 B_REGULAR_FACE = 0;
const uint16 B_BOLD_FACE = 1;
const uint16 B_ITALIC_FACE = 2;
const uint16 B_OBLIQUE_FACE = 4;
const uint16 B_UNDERSCORE_FACE = 0x0004;
const uint16 B_STRIKEOUT_FACE = 0x0008;

#define be_plain_font (new BFont())
#define be_fixed_font (new BFont())
#define be_bold_font (new BFont())

// Font flags
const uint32 B_FONT_FAMILY_AND_STYLE = 1;
const uint32 B_FONT_SIZE = 2;
const uint32 B_FONT_SHEAR = 4;
const uint32 B_FONT_ROTATION = 8;
const uint32 B_FONT_SPACING = 16;
const uint32 B_FONT_ALL = B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE | B_FONT_SHEAR | B_FONT_ROTATION | B_FONT_SPACING;

#define be_plain_font (new BFont())

#endif