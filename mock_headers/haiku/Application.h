#ifndef MOCK_APPLICATION_H
#define MOCK_APPLICATION_H

#include "Types.h"
#include "Looper.h"
#include "Handler.h"

// BeOS scripting constants
const int32 B_APP_IMAGE = 1;

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
    
    thread_id Run() { return BLooper::Run(); }
};

BApplication* BApplication::be_app = nullptr;

#endif
// Application scripting constants
const uint32 B_GET_PROPERTY = 'BGET';
const uint32 B_SET_PROPERTY = 'BSET';
const uint32 B_COUNT_PROPERTIES = 'BCNT';
const uint32 B_CREATE_PROPERTY = 'BCRT';
const uint32 B_DELETE_PROPERTY = 'BDLT';
const uint32 B_EXECUTE_PROPERTY = 'BEXE';

struct app_info {
    team_id team;
    char signature[B_MIME_TYPE_LENGTH];
};

status_t GetAppInfo(app_info* info) {
    (void)info;
    return B_OK;
}

class BRoster {
public:
    BRoster() = default;
    ~BRoster() = default;
    
    status_t GetAppInfo(const char* sig, app_info* info) {
        (void)sig; (void)info;
        return B_NAME_NOT_FOUND;
    }
    
    status_t GetRunningAppInfo(team_id team, app_info* info) {
        (void)team; (void)info;
        return B_NAME_NOT_FOUND;
    }
    
    status_t GetAppList(BList* list) {
        (void)list;
        return B_OK;
    }
    
    bool IsRunning(const char* sig) {
        (void)sig;
        return false;
    }
    
    status_t Launch(const char* sig, int argc = 0, const char** argv = nullptr) {
        (void)sig; (void)argc; (void)argv;
        return B_OK;
    }
    
    status_t ActivateApp(team_id team) {
        (void)team;
        return B_OK;
    }
};

extern BRoster* be_roster;

