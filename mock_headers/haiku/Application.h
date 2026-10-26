#ifndef MOCK_APPLICATION_H
#define MOCK_APPLICATION_H

#include "Types.h"
#include "Looper.h"
#include "Handler.h"

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
    
    static BApplication* be_app;
    
    thread_id Run() { return BLooper::Run(); }
};

BApplication* BApplication::be_app = nullptr;

#endif