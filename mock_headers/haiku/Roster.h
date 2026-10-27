#ifndef MOCK_ROSTER_H
#define MOCK_ROSTER_H

#include "Types.h"
#include "Entry.h"
#include "Path.h"
#include "Message.h"

#define B_MIME_TYPE_LENGTH 256

struct app_info {
    team_id team;
    char signature[B_MIME_TYPE_LENGTH];
    entry_ref ref;
    thread_id thread;
    const char* args;
    int32 arg_count;
};

class BRoster {
public:
    BRoster() = default;
    ~BRoster() = default;
    
    static BRoster* be_roster;
    
    status_t Launch(const char* signature, int32 argc, const char** argv, const BMessage* message = nullptr) {
        (void)signature; (void)argc; (void)argv; (void)message;
        return B_ERROR;
    }
    
    status_t Launch(const char* signature, const BMessage* message, const entry_ref* ref, int32 flags = 0) {
        (void)signature; (void)message; (void)ref; (void)flags;
        return B_ERROR;
    }
    
    status_t Launch(const entry_ref* ref, int32 argc, const char** argv, const BMessage* message = nullptr) {
        (void)ref; (void)argc; (void)argv; (void)message;
        return B_ERROR;
    }
    
    status_t GetAppInfo(const char* signature, app_info* info) const {
        (void)signature; (void)info;
        return B_ERROR;
    }
    
    status_t GetRunningAppInfo(team_id team, app_info* info) const {
        (void)team; (void)info;
        return B_ERROR;
    }
    
    status_t GetAppList(app_info* list, int32* count) const {
        (void)list; (void)count;
        return B_ERROR;
    }
    
    status_t AddToRecentDocuments(const entry_ref* ref, const char* mimeType = nullptr) {
        (void)ref; (void)mimeType;
        return B_ERROR;
    }
    
    status_t GetRecentDocuments(BMessage* list, const char* mimeType = nullptr) const {
        (void)list; (void)mimeType;
        return B_ERROR;
    }
};

BRoster* BRoster::be_roster = nullptr;

// Global be_roster pointer (Haiku API)
BRoster* be_roster = BRoster::be_roster;

#endif