#ifndef MOCK_APPLICATION_H
#define MOCK_APPLICATION_H

#include "Types.h"
#include "Looper.h"
#include "Handler.h"
#include "List.h"

// BeOS scripting constants
const int32 B_APP_IMAGE = 1;

struct app_info {
    team_id team;
    char signature[B_MIME_TYPE_LENGTH];
    entry_ref ref;
    thread_id thread;
    const char* args;
    int32 arg_count;
};

class BApplication : public BLooper {
public:
    BApplication() : BLooper("Application") {}
    BApplication(const char* signature) : BLooper("Application") { (void)signature; }
    virtual ~BApplication() = default;
    
    virtual void ReadyToRun() {}
    virtual void ArgvReceived(int32 argc, char** argv) { (void)argc; (void)argv; }
    virtual void RefsReceived(BMessage* message) { (void)message; }
    virtual void AboutRequested() {}
    virtual void Pulse() {}
    virtual void MessageReceived(BMessage* message) { (void)message; }
    virtual void QuitRequested() { Quit(); }
    
    // Scripting support
    virtual status_t GetSupportedSuites(BMessage* data) { (void)data; return B_OK; }
    virtual BHandler* ResolveSpecifier(BMessage* msg, int32 index,
                                       BMessage* specifier, int32 what,
                                       const char* property) {
        (void)msg; (void)index; (void)specifier; (void)what; (void)property;
        return nullptr;
    }
    
    static BApplication* be_app;
    
    status_t GetAppInfo(app_info* info) { (void)info; return B_OK; }
    
    thread_id Run() { return BLooper::Run(); }
};

BApplication* BApplication::be_app = nullptr;

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
    
    status_t GetAppList(BList* list) const {
        (void)list;
        return B_OK;
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
extern BRoster* be_roster;

BApplication* be_app = nullptr;

// BeOS scripting constants
const uint32 B_GET_PROPERTY = 'BGET';
const uint32 B_SET_PROPERTY = 'BSET';
const uint32 B_COUNT_PROPERTIES = 'BCNT';
const uint32 B_CREATE_PROPERTY = 'BCRT';
const uint32 B_DELETE_PROPERTY = 'BDLT';
const uint32 B_EXECUTE_PROPERTY = 'BEXE';

status_t GetAppInfo(app_info* info) {
    (void)info;
    return B_OK;
}

#endif
