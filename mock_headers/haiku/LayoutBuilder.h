#ifndef MOCK_LAYOUT_BUILDER_H
#define MOCK_LAYOUT_BUILDER_H

#include "Layout.h"
#include "View.h"

namespace BLayoutBuilder {
    class GroupBuilder {
    public:
        GroupBuilder(BView* parent, int orient = 0, float spacing = 0) {
            (void)parent; (void)orient; (void)spacing;
        }
        GroupBuilder(BWindow* window, int orient = 0, float spacing = 0) {
            (void)window; (void)orient; (void)spacing;
        }
        GroupBuilder& Add(BView* view) { (void)view; return *this; }
        GroupBuilder& Add(BView* view, float weight) { (void)view; (void)weight; return *this; }
        GroupBuilder& Add(BView* view, int col, int row) { (void)view; (void)col; (void)row; return *this; }
        GroupBuilder& Add(BView* view, int col, int row, int colspan) { (void)view; (void)col; (void)row; (void)colspan; return *this; }
        GroupBuilder& Add(BView* view, int col, int row, int colspan, int rowspan) { (void)view; (void)col; (void)row; (void)colspan; (void)rowspan; return *this; }
        GroupBuilder& Add(BLayoutItem* item) { (void)item; return *this; }
        GroupBuilder& Add(BLayoutItem* item, float weight) { (void)item; (void)weight; return *this; }
        GroupBuilder& Add(BLayoutItem* item, int col, int row) { (void)item; (void)col; (void)row; return *this; }
        GroupBuilder& Add(BLayoutItem* item, int col, int row, int colspan) { (void)item; (void)col; (void)row; (void)colspan; return *this; }
        GroupBuilder& Add(BLayoutItem* item, int col, int row, int colspan, int rowspan) { (void)item; (void)col; (void)row; (void)colspan; (void)rowspan; return *this; }
        GroupBuilder& AddGlue() { return *this; }
        GroupBuilder& SetInsets(float insets) { (void)insets; return *this; }
        GroupBuilder& SetInsets(float left, float top, float right, float bottom) { (void)left; (void)top; (void)right; (void)bottom; return *this; }
        GroupBuilder& AddSplit(BView* view) { (void)view; return *this; }
        GroupBuilder& AddGroup(int orient, float spacing = 0) { (void)orient; (void)spacing; return *this; }
        GroupBuilder& End() { return *this; }
        
        // Grid layout support
        GroupBuilder& AddGrid(float hSpacing = 0, float vSpacing = 0) { (void)hSpacing; (void)vSpacing; return *this; }
    };
    
    // Template function for Group<>() syntax
    template<typename T = int>
    GroupBuilder Group(BView* parent, T orient = 0, float spacing = 0) {
        return GroupBuilder(parent, orient, spacing);
    }
    
    template<typename T = int>
    GroupBuilder Group(BWindow* window, T orient = 0, float spacing = 0) {
        return GroupBuilder(window, orient, spacing);
    }
}

#endif