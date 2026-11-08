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
        GroupBuilder& Add(BLayoutItem* item) { (void)item; return *this; }
        GroupBuilder& Add(BLayoutItem* item, float weight) { (void)item; (void)weight; return *this; }
        GroupBuilder& AddGlue() { return *this; }
        GroupBuilder& SetInsets(float insets) { (void)insets; return *this; }
        GroupBuilder& AddGroup(int orient, float spacing = 0) { (void)orient; (void)spacing; return *this; }
        GroupBuilder& End() { return *this; }
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