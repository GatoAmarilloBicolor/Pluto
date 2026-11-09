// Minimal GroupView mock for cross-compilation
#ifndef _GROUP_VIEW_H
#define _GROUP_VIEW_H

#include "View.h"

class BGroupView : public BView {
public:
    BGroupView(orientation o = B_HORIZONTAL, float spacing = 0);
    virtual ~BGroupView();
};

class BGroupLayout : public BLayout {
public:
    BGroupLayout(orientation o = B_HORIZONTAL);
    virtual ~BGroupLayout();
};

#endif // _GROUP_VIEW_H