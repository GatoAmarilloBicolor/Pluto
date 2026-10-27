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