#ifndef MOCK_COLUMN_LIST_VIEW_H
#define MOCK_COLUMN_LIST_VIEW_H

#include "Types.h"
#include "View.h"
#include "Rect.h"
#include "Point.h"
#include "Message.h"
#include "Font.h"
#include "ScrollBar.h"
#include "ListView.h"
#include "Bitmap.h"

class BColumn;
class BRow;
class BField;
class BScrollBar;
class BPopUpMenu;
class BView;
class BBitmap;
class BRegion;
class BMenuItem;
class BMenu;
class BColumnListView;

typedef int32 list_view_type;

enum {
    B_SINGLE_SELECTION_LIST = 0,
    B_MULTIPLE_SELECTION_LIST = 1
};

enum {
    B_NAVIGABLE = 1 << 0,
    B_NAVIGABLE_JUMP = 1 << 1
};

enum ColumnListViewColor {
    B_BACKGROUND_COLOR = 0,
    B_SELECTION_COLOR,
    B_EDIT_COLOR,
    B_COLOR_TOTAL
};

enum ColumnListViewFont {
    B_TITLE_FONT = 0,
    B_FIELD_FONT,
    B_FONT_TOTAL
};

enum LatchType {
    B_CLOSED_LATCH = 0,
    B_OPEN_LATCH,
    B_NO_LATCH
};

enum column_flags {
    B_FIXED_SIZE_COLUMN = 0,
    B_WIDTH_AS_PERCENT = 1,
    B_ALLOW_COLUMN_MOVE = 2,
    B_ALLOW_COLUMN_REMOVE = 4,
    B_ALLOW_COLUMN_POPUP = 8,
    B_ALLOW_COLUMN_RESIZE = 16,
    B_HIDDEN_COLUMN = 32,
    B_NOT_SORTABLE = 64
};

class BField {
public:
    BField();
    virtual ~BField();

    virtual int32 Compare(const BField* field) const { (void)field; return 0; }
    virtual BField* Clone() const { return new BField(*this); }
    virtual int32 ID() const { return 0; }
    virtual void SetWidth(float width) { fWidth = width; }
    virtual float Width() const { return fWidth; }
    virtual void SetClippedString(const char* string) { (void)string; fClipped = true; }
    virtual bool HasClippedString() const { return fClipped; }
    virtual const char* ClippedString() const { return ""; }
    virtual const char* FieldType() const { return "BField"; }

protected:
    float fWidth = 0;
    bool fClipped = false;
    char* fClippedString = nullptr;
};

class BStringField : public BField {
public:
    BStringField(const char* str = "");
    virtual ~BStringField() = default;
    virtual int32 Compare(const BField* field) const override { (void)field; return 0; }
    virtual BField* Clone() const override { return new BStringField(fString.c_str()); }
    virtual int32 ID() const override { return 0; }
    const char* FieldType() const override { return "BStringField"; }
    const char* String() const { return fString.c_str(); }
private:
    std::string fString;
};

class BBitmapField : public BField {
public:
    BBitmapField() = default;
    virtual ~BBitmapField() = default;
    virtual int32 Compare(const BField* field) const override { (void)field; return 0; }
    virtual BField* Clone() const override { return new BBitmapField(); }
    virtual int32 ID() const override { return 0; }
    const char* FieldType() const override { return "BBitmapField"; }
};

class BColumn {
public:
    BColumn(float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
    virtual ~BColumn();

    virtual void MouseMoved(BColumnListView* parent, BRow* row, BField* field, BPoint point, uint32 transit, BMessage* dragMessage) { (void)parent; (void)row; (void)field; (void)point; (void)transit; (void)dragMessage; }
    virtual void MouseDown(BColumnListView* parent, BRow* row, BField* field, BPoint point) { (void)parent; (void)row; (void)field; (void)point; }
    virtual void MouseUp(BColumnListView* parent, BRow* row, BField* field, BPoint point) { (void)parent; (void)row; (void)field; (void)point; }
    virtual void DrawField(BField* field, BRect rect, BView* parent) = 0;
    virtual float GetPreferredWidth(BField* field, BView* parent) const { (void)field; (void)parent; return 100; }
    virtual int CompareFields(BField* field1, BField* field2) { (void)field1; (void)field2; return 0; }
    virtual bool AcceptsField(const BField* field) const { (void)field; return true; }

    float Width() const { return fWidth; }
    float MinWidth() const { return fMinWidth; }
    float MaxWidth() const { return fMaxWidth; }
    void SetWidth(float width) { fWidth = width; }
    void SetMinWidth(float minWidth) { fMinWidth = minWidth; }
    void SetMaxWidth(float maxWidth) { fMaxWidth = maxWidth; }
    alignment Alignment() const { return fAlignment; }
    void SetAlignment(alignment align) { fAlignment = align; }
    void SetTitle(const char* title) { fTitle = title ? title : ""; }
    const char* Title() const { return fTitle.c_str(); }
    void SetKey(const char* key) { fKey = key ? key : ""; }
    const char* Key() const { return fKey.c_str(); }
    void SetFlags(column_flags flags) { fFlags = flags; }
    column_flags Flags() const { return fFlags; }
    void SetComparisonFunction(int (*func)(const BField*, const BField*)) { (void)func; }
    int (*ComparisonFunction() const)(const BField*, const BField*) { return nullptr; }
    void SetPopUpMenu(BPopUpMenu* menu) { (void)menu; }
    BPopUpMenu* PopUpMenu() const { return nullptr; }
    void SetBitmap(BBitmap* bitmap) { (void)bitmap; }
    BBitmap* Bitmap() const { return nullptr; }

protected:
    float fWidth;
    float fMinWidth;
    float fMaxWidth;
    alignment fAlignment;
    BString fTitle;
    BString fKey;
    column_flags fFlags;
};

class BRow {
public:
    BRow();
    virtual ~BRow();

    virtual BField* GetField(int32 index) const { (void)index; return nullptr; }
    virtual BField* GetField(const char* key) const { (void)key; return nullptr; }
    virtual int32 CountFields() const { return 0; }
    virtual void SetField(BField* field, int32 index) { (void)field; (void)index; }
    virtual void SetField(BField* field, const char* key) { (void)field; (void)key; }
    virtual void AddField(BField* field, int32 index) { (void)field; (void)index; }
    virtual void AddField(BField* field, const char* key) { (void)field; (void)key; }
    virtual void RemoveField(int32 index) { (void)index; }
    virtual void RemoveField(const char* key) { (void)key; }
    virtual void SetKey(const char* key) { (void)key; }
    virtual const char* Key() const { return ""; }
    virtual bool HasLatch() const { return false; }
    virtual void SetLatchState(LatchType state) { (void)state; }
    virtual LatchType LatchState() const { return B_NO_LATCH; }
    virtual void SetExpanded(bool expanded) { (void)expanded; }
    virtual bool IsExpanded() const { return false; }
    virtual int32 Level() const { return 0; }
    virtual void SetLevel(int32 level) { (void)level; }
    virtual BRow* Parent() const { return nullptr; }
};

class BColumnListView : public BListView {
public:
    BColumnListView(BRect frame, const char* name, list_view_type type = B_SINGLE_SELECTION_LIST,
                    uint32 resizingMode = B_FOLLOW_ALL_SIDES, uint32 flags = B_WILL_DRAW | B_NAVIGABLE);
    BColumnListView(const char* name, uint32 resizingMode = B_FOLLOW_ALL_SIDES, uint32 flags = B_WILL_DRAW | B_NAVIGABLE);
    virtual ~BColumnListView();

    virtual void AddColumn(BColumn* column, int32 index = -1) { (void)column; (void)index; }
    virtual void RemoveColumn(BColumn* column) { (void)column; }
    virtual void RemoveColumn(int32 index) { (void)index; }
    virtual BColumn* ColumnAt(int32 index) const { (void)index; return nullptr; }
    virtual int32 CountColumns() const { return 0; }
    virtual int32 IndexOf(BColumn* column) const { (void)column; return -1; }
    virtual void SetColumnWidth(BColumn* column, float width) { (void)column; (void)width; }
    virtual float ColumnWidth(BColumn* column) const { (void)column; return 0; }
    virtual void SetSortColumn(BColumn* column) { (void)column; }
    virtual void SetSortColumn(BColumn* column, bool ascending, bool secondary) { (void)column; (void)ascending; (void)secondary; }
    virtual BColumn* SortColumn() const { return nullptr; }
    virtual void SetSortAscending(bool ascending) { (void)ascending; }
    virtual bool SortAscending() const { return true; }
    virtual void SetSortFunction(int (*func)(const BRow*, const BRow*)) { (void)func; }
    virtual void SortItems() {}
    virtual void SetSortingEnabled(bool enabled) { (void)enabled; }
    virtual bool SortingEnabled() const { return false; }
    virtual void SetRowHeight(float height) { (void)height; }
    virtual float RowHeight() const { return 20; }
    virtual void SetLatchColumn(BColumn* column) { (void)column; }
    virtual BColumn* LatchColumn() const { return nullptr; }
    virtual void SetFixedHeightMode(bool fixed) { (void)fixed; }
    virtual bool FixedHeightMode() const { return false; }
    virtual void SetSelectionMessage(BMessage* message) { (void)message; }
    virtual void SetInvocationMessage(BMessage* message) { (void)message; }
    virtual void SetAutoDelete(bool autoDelete) { (void)autoDelete; }
    virtual void SetEditMessage(BMessage* message) { (void)message; }
    virtual void SetDoubleClickMessage(BMessage* message) { (void)message; }
    virtual void SetCompareFunction(int (*func)(const BField*, const BField*)) { (void)func; }

    // Row management
    virtual BRow* RowAt(int32 index) const { (void)index; return nullptr; }
    virtual BRow* RemoveRow(BRow* row) { (void)row; return nullptr; }
    virtual bool AddRow(BRow* row) { (void)row; return false; }
    virtual BRow* CurrentSelection() const { return nullptr; }

    virtual void KeyDown(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    virtual void MouseDown(BPoint where) { (void)where; }
    virtual void Draw(BRect updateRect) { (void)updateRect; }
    virtual void FrameResized(float width, float height) { (void)width; (void)height; }
    virtual void AttachedToWindow() {}
    virtual void DetachedFromWindow() {}
    virtual void MessageReceived(BMessage* message) { (void)message; }
    virtual void ScrollTo(BPoint where) { (void)where; }
    virtual void ScrollBy(float dx, float dy) { (void)dx; (void)dy; }

    // BView overrides
    virtual void SetFont(const BFont* font) { (void)font; }
    virtual void GetFont(BFont* font) const { (void)font; }
    virtual void SetFontSize(float size) { (void)size; }
    virtual void SetTarget(BHandler* target) { (void)target; }
    virtual void SetTarget(BView* view, BHandler* target) { (void)view; (void)target; }

protected:
    virtual void _Init() {}
    virtual void _UpdateColors() {}
    void _GetChildViewRects(const BRect& bounds, BRect& titleRect, BRect& listRect, BRect& statusRect) const { (void)bounds; (void)titleRect; (void)listRect; (void)statusRect; }
    void _LayoutChildren() {}
    void _UpdateScrollBars() {}
    void _ScrollTo(BPoint point) { (void)point; }
    void _Select(int32 index, bool select, bool extend) { (void)index; (void)select; (void)extend; }
    void _Deselect(int32 index) { (void)index; }
    void _UpdateSelection(BPoint point, bool extend) { (void)point; (void)extend; }
    void _TrackSelection(BPoint point) { (void)point; }
    void _ScrollToSelection() {}
    void _HandleKeyDown(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    void _HandleMouseDown(BPoint where) { (void)where; }
    void _HandleMouseMoved(BPoint where, uint32 transit, BMessage* message) { (void)where; (void)transit; (void)message; }
    void _HandleMouseUp(BPoint where) { (void)where; }

private:
    BColumnListView(const BColumnListView&);
    BColumnListView& operator=(const BColumnListView&);

    friend class BRow;
    friend class BColumn;
    friend class BField;
};

#endif