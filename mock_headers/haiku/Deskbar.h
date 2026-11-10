#ifndef MOCK_DESKBAR_H
#define MOCK_DESKBAR_H

#include "View.h"
#include "Rect.h"
#include "Bitmap.h"

class BMessage;
class BHandler;

class BDeskbar {
public:
    BDeskbar(BHandler* target = nullptr, team_id team = 0) {
        (void)target; (void)team;
    }
    virtual ~BDeskbar() = default;

    status_t AddItem(BView* view) { (void)view; return B_OK; }
    status_t RemoveItem(BView* view) { (void)view; return B_OK; }
    status_t RemoveItem(const char* name) { (void)name; return B_OK; }
    bool IsRunning() const { return true; }
    BView* ItemView(int32 index) const { (void)index; return nullptr; }
    int32 CountItems() const { return 0; }
    bool HasItem(BView* view) const { (void)view; return false; }
};

#endif