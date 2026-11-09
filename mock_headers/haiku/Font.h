#ifndef MOCK_FONT_H
#define MOCK_FONT_H

#include <cstdint>

struct font_height {
    float ascent;
    float descent;
    float leading;
};

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
    virtual float StringWidth(const char* string, int32_t len = -1) const {
        (void)string; (void)len; return 0;
    }
    virtual void SetFace(uint16_t face) { (void)face; }
    virtual uint16_t Face() const { return 0; }
    virtual void GetHeight(font_height* out) const {
        if (out) {
            out->ascent = 10.0f;
            out->descent = 3.0f;
            out->leading = 1.0f;
        }
    }
};

// Font faces
const uint16_t B_REGULAR_FACE = 0;
const uint16_t B_BOLD_FACE = 1;
const uint16_t B_ITALIC_FACE = 2;
const uint16_t B_OBLIQUE_FACE = 4;
const uint16_t B_UNDERSCORE_FACE = 0x0004;
const uint16_t B_STRIKEOUT_FACE = 0x0008;

// Font flags
const uint32_t B_FONT_FAMILY_AND_STYLE = 1;
const uint32_t B_FONT_SIZE = 2;
const uint32_t B_FONT_SHEAR = 4;
const uint32_t B_FONT_ROTATION = 8;
const uint32_t B_FONT_SPACING = 16;
const uint32_t B_FONT_ALL = B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE | B_FONT_SHEAR | B_FONT_ROTATION | B_FONT_SPACING;

extern BFont* be_plain_font;
extern BFont* be_fixed_font;
extern BFont* be_bold_font;

#endif