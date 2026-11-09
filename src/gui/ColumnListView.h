#ifndef COLUMN_LIST_VIEW_H
#define COLUMN_LIST_VIEW_H

#include <ListView.h>
#include <Rect.h>
#include <Point.h>
#include <Size.h>
#include <Message.h>
#include <Font.h>
#include <string>

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

	virtual int32 Compare(const BField* field) const = 0;
	virtual BField* Clone() const = 0;
	virtual int32 ID() const = 0;
	virtual void SetWidth(float width);
	virtual float Width() const;
	virtual void SetClippedString(const char* string);
	virtual bool HasClippedString() const;
	virtual const char* ClippedString() const;
	virtual const char* FieldType() const { return "BField"; }

protected:
	float fWidth;
	bool fClipped;
	char* fClippedString;
};

class BStringField : public BField {
public:
	BStringField(const char* str = "");
	virtual ~BStringField() = default;
	const char* FieldType() const override { return "BStringField"; }
	const char* String() const;
private:
	std::string fString;
};

class BBitmapField : public BField {
public:
	BBitmapField() = default;
	virtual ~BBitmapField() = default;
	const char* FieldType() const override { return "BBitmapField"; }
};

class BColumn {
public:
	BColumn(float width, float minWidth, float maxWidth, alignment align = B_ALIGN_LEFT);
	virtual ~BColumn();

	virtual void MouseMoved(BColumnListView* parent, BRow* row, BField* field, BPoint point, uint32 transit, BMessage* dragMessage);
	virtual void MouseDown(BColumnListView* parent, BRow* row, BField* field, BPoint point);
	virtual void MouseUp(BColumnListView* parent, BRow* row, BField* field, BPoint point);
	virtual void DrawField(BField* field, BRect rect, BView* parent) = 0;
	virtual float GetPreferredWidth(BField* field, BView* parent) const;
	virtual int CompareFields(BField* field1, BField* field2);
	virtual bool AcceptsField(const BField* field) const;

	float Width() const;
	float MinWidth() const;
	float MaxWidth() const;
	void SetWidth(float width);
	void SetMinWidth(float minWidth);
	void SetMaxWidth(float maxWidth);
	alignment Alignment() const;
	void SetAlignment(alignment align);
	void SetTitle(const char* title);
	const char* Title() const;
	void SetKey(const char* key);
	const char* Key() const;
	void SetFlags(column_flags flags);
	column_flags Flags() const;
	void SetComparisonFunction(int (*func)(const BField*, const BField*));
	int (*ComparisonFunction() const)(const BField*, const BField*);
	void SetPopUpMenu(BPopUpMenu* menu);
	BPopUpMenu* PopUpMenu() const;
	void SetBitmap(BBitmap* bitmap);
	BBitmap* Bitmap() const;

protected:
	float fWidth;
	float fMinWidth;
	float fMaxWidth;
	alignment fAlignment;
	BString fTitle;
	BString fKey;
	column_flags fFlags;
	int (*fCompareFunc)(const BField*, const BField*);
	BPopUpMenu* fPopUpMenu;
	BBitmap* fBitmap;
};

class BRow : public BListItem {
public:
	BRow();
	BRow(int32 numFields);
	virtual ~BRow();

	void SetField(BField* field, int32 index);
	BField* GetField(int32 index) const;
	int32 CountFields() const;
	void SetField(BField* field, BColumn* column);
	BField* GetField(BColumn* column) const;
	void SetFieldAt(int32 index, BField* field);
	BField* FieldAt(int32 index) const;
	void AddField(BField* field);
	void InsertField(BField* field, int32 index);
	void RemoveField(int32 index);
	void RemoveFields();
	void SetExpanded(bool expanded);
	bool IsExpanded() const;
	void SetLevel(int32 level);
	int32 Level() const;
	void SetHasLatch(bool hasLatch);
	bool HasLatch() const;
	void SetColor(rgb_color color);
	rgb_color Color() const;

private:
	BList* fFields;
	bool fExpanded;
	int32 fLevel;
	bool fHasLatch;
	rgb_color fColor;
};

class BColumnListView : public BView {
public:
	enum {
		kMsgDrag = 'Cdrg',
		kMsgColumnResized = 'Ccrs',
		kMsgColumnMoved = 'Ccmv',
		kMsgSort = 'Cst',
		kMsgInvoke = 'Cinv',
		kMsgSelection = 'Csel',
		kMsgExpand = 'Cexp',
		kMsgCollapse = 'Ccol',
		kMsgKeyDown = 'Ckdn'
	};

	BColumnListView(BRect rect, const char* name, uint32 resizingMode = B_FOLLOW_ALL_SIDES,
		uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE | B_NAVIGABLE_JUMP,
		list_view_type type = B_SINGLE_SELECTION_LIST,
		bool hierarchical = true,
		bool horizontal = false,
		bool vertical = true,
		border_style border = B_FANCY_BORDER);
	BColumnListView(const char* name, uint32 flags, border_style border = B_FANCY_BORDER);
	virtual ~BColumnListView();

	virtual void AttachedToWindow();
	virtual void DetachedFromWindow();
	virtual void Draw(BRect updateRect);
	virtual void MessageReceived(BMessage* message);
	virtual void KeyDown(const char* bytes, int32 numBytes);
	virtual void MakeFocus(bool focused = true);
	virtual BSize MinSize();
	virtual BSize MaxSize();
	virtual BSize PreferredSize();
	virtual void FrameResized(float width, float height);
	virtual void WindowActivated(bool active);

	void AddColumn(BColumn* column, int32 logicalFieldIndex = -1);
	void MoveColumn(BColumn* column, int32 index);
	void RemoveColumn(BColumn* column);
	int32 CountColumns() const;
	BColumn* ColumnAt(int32 index) const;
	BColumn* ColumnAt(BPoint point) const;
	void SetColumnVisible(BColumn* column, bool visible);
	void SetColumnVisible(int32 index, bool isVisible);
	bool IsColumnVisible(int32 index) const;
	void SetColumnFlags(column_flags flags);
	void ResizeColumnToPreferred(int32 index);
	void ResizeAllColumnsToPreferred();
	void SetSortColumn(BColumn* column, bool add, bool ascending);
	void ClearSortColumns();
	BColumn* SortColumn(bool& ascending) const;
	void SetSortingEnabled(bool enabled);
	bool SortingEnabled() const;

	void AddRow(BRow* row, BRow* parentRow = nullptr);
	void AddRow(BRow* row, int32 index, BRow* parentRow);
	void AddRows(BList* rows, int32 index, BRow* parent);
	void RemoveRow(BRow* row);
	void RemoveRows(BList* rows);
	void UpdateRow(BRow* row);
	void SwapRows(int32 index1, int32 index2, BRow* parentRow1, BRow* parentRow2);
	void Clear();

	BRow* RowAt(int32 index, BRow* parentRow = nullptr) const;
	BRow* RowAt(int32 index, BRow* parentRow);
	BRow* RowAt(BPoint point) const;
	BRow* RowAt(BPoint point);
	void GetRowRect(const BRow* row, BRect* outRect) const;
	void FindParent(BRow* row, BRow** _parent, bool* _isVisible) const;
	int32 IndexOf(BRow* row) const;
	int32 CountRows(BRow* parentRow = nullptr) const;

	void Deselect(BRow* row);
	void AddToSelection(BRow* row);
	void DeselectAll();
	BRow* CurrentSelection(BRow* lastSelected = nullptr) const;
	void SelectionChanged();
	void SetSelectionMode(list_view_type mode);
	list_view_type SelectionMode() const;
	void SetSelectionMessage(BMessage* message);
	BMessage* SelectionMessage() const;
	void SetSelectionCommand(uint32 command);
	uint32 SelectionCommand() const;
	void SetInvocationMessage(BMessage* message);
	BMessage* InvocationMessage() const;
	void SetInvocationCommand(uint32 command);
	uint32 InvocationCommand() const;
	void Invoke(BMessage* message);
	void ItemInvoked();

	void SetFocusRow(int32 index, bool select);
	void SetFocusRow(BRow* row, bool select);
	BRow* FocusRow() const;

	void SetMouseTrackingEnabled(bool enabled);
	bool MouseTrackingEnabled() const;

	void AddStatusView(BView* view);
	void RemoveStatusView();
	BView* StatusView() const;

	void SetFont(const BFont* font, uint32 mask = B_FONT_ALL);
	void SetFont(ColumnListViewFont font_num, const BFont* font, uint32 mask = B_FONT_ALL);
	void GetFont(ColumnListViewFont font_num, BFont* font) const;

	void SetColor(ColumnListViewColor colorIndex, rgb_color color);
	void ResetColors();
	rgb_color Color(ColumnListViewColor colorIndex) const;

	void SetHighColor(rgb_color color);
	void SetSelectionColor(rgb_color color);
	void SetBackgroundColor(rgb_color color);
	void SetEditColor(rgb_color color);
	rgb_color SelectionColor() const;
	rgb_color BackgroundColor() const;
	rgb_color EditColor() const;

	void SuggestTextPosition(const BRow* row, const BColumn* column, BPoint* position, BRect* rect) const;
	void GetFieldRect(const BRow* row, const BColumn* inColumn, BRect* outRect) const;

	void SetLatchWidth(float width);
	float LatchWidth() const;
	void DrawLatch(BView* view, BRect rect, LatchType position, BRow* row);

	bool InitiateDrag(BPoint point, bool isSelect = false);
	void MessageDropped(BMessage* message, BPoint point);
	void ExpandOrCollapse(BRow* row, bool open);

	void SaveState(BMessage* message);
	void LoadState(BMessage* message);

	void SetEditMode(bool state);
	bool EditMode() const;
	void Refresh();

protected:
	virtual void _Init();
	virtual void _UpdateColors();
	void _GetChildViewRects(const BRect& bounds, BRect& titleRect, BRect& listRect, BRect& statusRect) const;
	void _LayoutChildren();
	void _UpdateScrollBars();
	void _ScrollTo(BPoint point);
	void _Select(int32 index, bool select, bool extend);
	void _Deselect(int32 index);
	void _UpdateSelection(BPoint point, bool extend);
	void _TrackSelection(BPoint point);
	void _ScrollToSelection();
	void _HandleKeyDown(const char* bytes, int32 numBytes);
	void _HandleMouseDown(BPoint where);
	void _HandleMouseMoved(BPoint where, uint32 transit, BMessage* message);
	void _HandleMouseUp(BPoint where);

private:
	BColumnListView(const BColumnListView&);
	BColumnListView& operator=(const BColumnListView&);

	friend class BRow;
	friend class BColumn;
	friend class BField;
};

#endif