#ifndef MOCK_INTERFACE_DEFS_H
#define MOCK_INTERFACE_DEFS_H

#include "Types.h"
#include "Rect.h"
#include "String.h"

enum drawing_mode {
    B_OP_COPY = 0,
    B_OP_OVER = 1,
    B_OP_ERASE = 2,
    B_OP_SELECT = 3,
    B_OP_MIN = 4,
    B_OP_MAX = 5,
    B_OP_ADD = 6,
    B_OP_SUBTRACT = 7,
    B_OP_BLEND = 8,
    B_OP_ALPHA = 9
};

// BField base class
class BField {
public:
    BField() = default;
    virtual ~BField() = default;
    virtual void SetWidth(float width) { (void)width; }
    virtual float Width() const { return 0; }
    virtual void SetClippedString(const char* string) { (void)string; }
    virtual const char* ClippedString() const { return ""; }
    virtual const char* FieldType() const { return "BField"; }
};

// BStringField 
class BStringField : public BField {
public:
    BStringField(const char* string = "");
    virtual ~BStringField() = default;
    virtual void SetString(const char* string);
    virtual const char* String() const;
    virtual void SetWidth(float width);
    virtual float Width() const;
    virtual void SetClippedString(const char* string);
    virtual const char* ClippedString() const;
    virtual const char* FieldType() const { return "BStringField"; }
private:
    BString fString;
    float fWidth = 0;
    BString fClippedString;
};

// BBitmapField
class BBitmapField : public BField {
public:
    BBitmapField() = default;
    virtual ~BBitmapField() = default;
    virtual const char* FieldType() const { return "BBitmapField"; }
};

// BColumn base class
class BColumn {
public:
    BColumn(float width, float minWidth, float maxWidth, float truncate = 0, alignment align = B_ALIGN_LEFT)
        : fWidth(width), fMinWidth(minWidth), fMaxWidth(maxWidth), fTruncate(truncate), fAlign(align) {}
    virtual ~BColumn() = default;
    virtual void DrawTitle(BRect rect, class BView* parent) { (void)rect; (void)parent; }
    virtual void DrawField(BField* field, BRect rect, class BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual float GetPreferredWidth(BField* field, class BView* parent) const { (void)field; (void)parent; return fWidth; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
    virtual bool AcceptsField(const BField* field) const { (void)field; return true; }
    virtual void SetWidth(float width) { fWidth = width; }
    virtual float Width() const { return fWidth; }
    virtual float MinWidth() const { return fMinWidth; }
    virtual float MaxWidth() const { return fMaxWidth; }
    virtual alignment Alignment() const { return fAlign; }
protected:
    float fWidth;
    float fMinWidth;
    float fMaxWidth;
    float fTruncate;
    alignment fAlign;
};

// Color constants (for ui_color) - using existing color_which typedef from Types.h
inline rgb_color ui_color(color_which which) {
    (void)which;
    return { 255, 255, 255, 255 };
}

#endif

