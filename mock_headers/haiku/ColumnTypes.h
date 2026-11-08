#ifndef MOCK_COLUMN_TYPES_H
#define MOCK_COLUMN_TYPES_H

#include "Types.h"
#include "String.h"
#include "View.h"
#include "InterfaceDefs.h"
#include "DateTimeFormat.h"
#include "NumberFormat.h"

// Forward declarations
class BBitmap;
struct BFont;
class BField;
class BColumn;
class BRow;

class BTitledColumn : public BColumn {
public:
    BTitledColumn(const char* title, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BTitledColumn() = default;
    virtual void DrawTitle(BRect rect, BView* parent) { (void)rect; (void)parent; }
    virtual void GetColumnName(BString* into) const { (void)into; }
    void SetTitle(const char* title) { (void)title; }
    void Title(BString* forTitle) const { (void)forTitle; }
    float FontHeight() const { return 12.0f; }
protected:
    BString fTitle;
    float fFontHeight = 12.0f;
};

// BStringField is defined in InterfaceDefs.h

class BStringColumn : public BTitledColumn {
public:
    BStringColumn(const char* title, float width, float minWidth, float maxWidth, uint32 truncate = 0, alignment align = B_ALIGN_LEFT);
    virtual ~BStringColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual float GetPreferredWidth(BField* field, BView* parent) const { (void)field; (void)parent; return 100; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
    virtual bool AcceptsField(const BField* field) const { (void)field; return false; }
protected:
    uint32 fTruncate = 0;
};

class BDateField : public BField {
public:
    BDateField(time_t* time = nullptr);
    virtual ~BDateField() = default;
    void SetWidth(float width) { (void)width; }
    float Width() { return 0; }
    void SetClippedString(const char* string) { (void)string; }
    const char* ClippedString() { return ""; }
    time_t Seconds() { return 0; }
    time_t UnixTime() { return 0; }
    const char* FieldType() const { return "BDateField"; }
private:
    tm fTime;
    time_t fUnixTime = 0;
    time_t fSeconds = 0;
    BString fClippedString;
    float fWidth = 0;
};

class BDateColumn : public BTitledColumn {
public:
    BDateColumn(const char* title, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BDateColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
protected:
    BString fTitle;
    BDateTimeFormat fDateTimeFormat;
    BDateFormat fDateFormat;
};

class BSizeField : public BField {
public:
    BSizeField(off_t size = 0);
    virtual ~BSizeField() = default;
    void SetSize(off_t size) { (void)size; }
    off_t Size() { return 0; }
    void SetWidth(float width) { (void)width; }
    float Width() { return 0; }
    const char* FieldType() const { return "BSizeField"; }
private:
    off_t fSize = 0;
};

class BSizeColumn : public BTitledColumn {
public:
    BSizeColumn(const char* title, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BSizeColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
protected:
    BNumberFormat fNumberFormat;
};

class BIntegerField : public BField {
public:
    BIntegerField(int32 number = 0);
    virtual ~BIntegerField() = default;
    void SetValue(int32 value) { (void)value; }
    int32 Value() { return 0; }
    const char* FieldType() const { return "BIntegerField"; }
private:
    int32 fInteger = 0;
};

class BIntegerColumn : public BTitledColumn {
public:
    BIntegerColumn(const char* title, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BIntegerColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
protected:
    BNumberFormat fNumberFormat;
};

class GraphColumn : public BIntegerColumn {
public:
    GraphColumn(const char* name, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~GraphColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
};

// BBitmapField is defined in InterfaceDefs.h

class BBitmapColumn : public BTitledColumn {
public:
    BBitmapColumn(const char* title, float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BBitmapColumn() = default;
    virtual void DrawField(BField* field, BRect rect, BView* parent) { (void)field; (void)rect; (void)parent; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
    virtual bool AcceptsField(const BField* field) const { (void)field; return false; }
};

#endif