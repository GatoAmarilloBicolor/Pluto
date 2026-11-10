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
#include "List.h"
#include "InterfaceDefs.h"  // For BField, BStringField, BBitmapField, BColumn, alignment

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
    CLV_NAVIGABLE = 1 << 0,
    CLV_NAVIGABLE_JUMP = 1 << 1
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

class BRow {
public:
    BRow() = default;
    virtual ~BRow() = default;

    virtual void AddField(BField* field, int32 columnIndex) { (void)field; (void)columnIndex; }
    virtual void SetField(BField* field, int32 columnIndex) { (void)field; (void)columnIndex; }
    virtual BField* GetField(int32 columnIndex) { (void)columnIndex; return nullptr; }
    virtual int32 CountFields() const { return 0; }
    virtual void SetHeight(float height) { fHeight = height; }
    virtual float Height() const { return fHeight; }
    virtual void SetLatchType(LatchType type) { (void)type; }
    virtual LatchType GetLatchType() const { return B_NO_LATCH; }
    virtual bool HasLatch() const { return false; }

protected:
    float fHeight = 0;
};

class BColumnListView : public BView {
public:
    BColumnListView(const char* name,
                    list_view_type type = B_SINGLE_SELECTION_LIST,
                    uint32 flags = B_FOLLOW_ALL,
                    uint32 listFlags = 0)
        : BView(BRect(), name, B_FOLLOW_ALL, B_WILL_DRAW) {
        (void)type; (void)flags; (void)listFlags;
    }
    virtual ~BColumnListView() = default;

    virtual void AddColumn(BColumn* column, int32 index = -1) { (void)column; (void)index; }
    virtual void RemoveColumn(BColumn* column) { (void)column; }
    virtual BColumn* ColumnAt(int32 index) { (void)index; return nullptr; }
    virtual int32 CountColumns() const { return 0; }
    virtual void AddRow(BRow* row) { (void)row; }
    virtual void RemoveRow(BRow* row) { (void)row; }
    virtual BRow* RowAt(int32 index) { (void)index; return nullptr; }
    virtual int32 CountRows() const { return 0; }
    virtual int32 FullListCountRows() const { return 0; }
    virtual void SetSortColumn(int32 index, bool ascending = true, bool resort = true) { (void)index; (void)ascending; (void)resort; }
    virtual void SetSortColumn(BColumn* column, bool ascending = true, bool resort = true) { (void)column; (void)ascending; (void)resort; }
    virtual void SetSortingEnabled(bool enabled) { (void)enabled; }
    virtual int32 SortColumn() const { return 0; }
    virtual void SetSortFunction(int32 columnIndex, int (*cmp)(BField*, BField*)) { (void)columnIndex; (void)cmp; }
    virtual void SetComparisonFunction(int (*cmp)(BRow*, BRow*)) { (void)cmp; }
    virtual void Sort() {}
    virtual void SetSelectionMessage(BMessage* message) { (void)message; }
    virtual void SetInvocationMessage(BMessage* message) { (void)message; }
    virtual void Select(int32 index) { (void)index; }
    virtual void Select(int32 start, int32 end) { (void)start; (void)end; }
    virtual void Deselect(int32 index) { (void)index; }
    virtual void DeselectAll() {}
    virtual bool IsSelected(int32 index) const { (void)index; return false; }
    virtual BRow* CurrentSelection(int32* cookie = nullptr) const { (void)cookie; return nullptr; }
    virtual void InvalidateRow(int32 index) { (void)index; }
    virtual void MakeEmpty() {}
    virtual void ScrollToSelection() {}
    virtual void KeyDown(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    virtual void MouseDown(BPoint where) { (void)where; }
    virtual void MouseUp(BPoint where) { (void)where; }
    virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* msg) { (void)where; (void)transit; (void)msg; }

    virtual void SetFont(BFont* font, uint32 flags = 0) { (void)font; (void)flags; }
    virtual void SetFontSize(float size) { (void)size; }
    virtual rgb_color Color(ColumnListViewColor which) const { (void)which; return {0,0,0,255}; }
    virtual void SetColor(ColumnListViewColor which, rgb_color color) { (void)which; (void)color; }
    virtual BFont* Font(ColumnListViewFont which) const { (void)which; return nullptr; }
    virtual void SetFont(ColumnListViewFont which, BFont* font) { (void)which; (void)font; }

protected:
    virtual void Draw(BRect updateRect) { (void)updateRect; }
    virtual void FrameResized(float width, float height) { (void)width; (void)height; }
    virtual void AttachedToWindow() {}
    virtual void AllAttached() {}
    virtual void ScrollTo(BPoint point) { (void)point; }
    virtual void ScrollBy(float dx, float dy) { (void)dx; (void)dy; }
    virtual void _Select(int32 index, bool select, bool extend) { (void)index; (void)select; (void)extend; }
    virtual void _Deselect(int32 index) { (void)index; }
    virtual void _UpdateSelection(BPoint point, bool extend) { (void)point; (void)extend; }
    virtual void _TrackSelection(BPoint point) { (void)point; }
    virtual void _ScrollToSelection() {}
    virtual void _HandleKeyDown(const char* bytes, int32 numBytes) { (void)bytes; (void)numBytes; }
    virtual void _HandleMouseDown(BPoint where) { (void)where; }
    virtual void _HandleMouseMoved(BPoint where, uint32 transit, BMessage* message) { (void)where; (void)transit; (void)message; }
    virtual void _HandleMouseUp(BPoint where) { (void)where; }

private:
    BColumnListView(const BColumnListView&);
    BColumnListView& operator=(const BColumnListView&);

    friend class BRow;
    friend class BColumn;
    friend class BField;
};

#endif