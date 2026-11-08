#ifndef MOCK_MESSAGE_H
#define MOCK_MESSAGE_H

#include "Types.h"
#include <map>
#include <string>
#include <vector>
#include "BString.h"
#include "Rect.h"

class BMessage {
public:
    BMessage() = default;
    BMessage(uint32_t what) : what(what) {}
    virtual ~BMessage() = default;
    
    uint32_t what;
    
    status_t AddData(const char* name, uint32_t type, const void* data, size_t len) { (void)name; (void)type; (void)data; (void)len; return B_OK; }
    status_t AddString(const char* name, const char* str) { (void)name; (void)str; return B_OK; }
    status_t AddInt32(const char* name, int32_t val) { (void)name; (void)val; return B_OK; }
    status_t AddInt64(const char* name, int64_t val) { (void)name; (void)val; return B_OK; }
    status_t AddFloat(const char* name, float val) { (void)name; (void)val; return B_OK; }
    status_t AddDouble(const char* name, double val) { (void)name; (void)val; return B_OK; }
    status_t AddBool(const char* name, bool val) { (void)name; (void)val; return B_OK; }
    status_t AddPointer(const char* name, void* ptr) { (void)name; (void)ptr; return B_OK; }
    status_t AddMessage(const char* name, const BMessage* msg) { (void)name; (void)msg; return B_OK; }
    status_t AddRef(const char* name, const entry_ref* ref) { (void)name; (void)ref; return B_OK; }
    status_t AddFlat(const char* name, const void* obj) { (void)name; (void)obj; return B_OK; }
    
    status_t AddSpecifier(const char* name, const char* value = nullptr) { (void)name; (void)value; return B_OK; }
    status_t AddSpecifier(const char* name, int32 index) { (void)name; (void)index; return B_OK; }
    
    status_t FindData(const char* name, uint32_t type, const void** data, ssize_t* len) const { (void)name; (void)type; (void)data; (void)len; return B_ERROR; }
    status_t FindData(const char* name, uint32_t type, int32 index, const void** data, ssize_t* len) const { (void)name; (void)type; (void)index; (void)data; (void)len; return B_ERROR; }
    
    status_t GetInfo(type_code type, int32 index, char** name, type_code* out_type, int32* count) const {
        (void)type; (void)index; (void)name; (void)out_type; (void)count; return B_ERROR;
    }
    status_t FindString(const char* name, const char** str) const { (void)name; (void)str; return B_ERROR; }
    status_t FindString(const char* name, int32 index, const char** str) const { (void)name; (void)index; (void)str; return B_ERROR; }
    status_t FindString(const char* name, BString* str) const { (void)name; (void)str; return B_ERROR; }
    status_t FindInt32(const char* name, int32_t* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindInt32(const char* name, int32 index, int32_t* val) const { (void)name; (void)index; (void)val; return B_ERROR; }
    status_t FindInt64(const char* name, int64_t* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindInt64(const char* name, int32 index, int64_t* val) const { (void)name; (void)index; (void)val; return B_ERROR; }
    status_t FindFloat(const char* name, float* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindFloat(const char* name, int32 index, float* val) const { (void)name; (void)index; (void)val; return B_ERROR; }
    status_t FindDouble(const char* name, double* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindDouble(const char* name, int32 index, double* val) const { (void)name; (void)index; (void)val; return B_ERROR; }
    
    status_t FindRect(const char* name, BRect* rect) const { (void)name; (void)rect; return B_ERROR; }
    status_t FindRect(const char* name, int32 index, BRect* rect) const { (void)name; (void)index; (void)rect; return B_ERROR; }
    status_t FindPoint(const char* name, BPoint* pt) const { (void)name; (void)pt; return B_ERROR; }
    status_t FindPoint(const char* name, int32 index, BPoint* pt) const { (void)name; (void)index; (void)pt; return B_ERROR; }
    status_t FindSize(const char* name, BSize* size) const { (void)name; (void)size; return B_ERROR; }
    status_t FindSize(const char* name, int32 index, BSize* size) const { (void)name; (void)index; (void)size; return B_ERROR; }
    status_t FindBool(const char* name, bool* val) const { (void)name; (void)val; return B_ERROR; }
    status_t FindBool(const char* name, int32 index, bool* val) const { (void)name; (void)index; (void)val; return B_ERROR; }
    status_t FindPointer(const char* name, void** ptr) const { (void)name; (void)ptr; return B_ERROR; }
    status_t FindMessage(const char* name, BMessage* msg) const { (void)name; (void)msg; return B_ERROR; }
    status_t FindMessage(const char* name, int32 index, BMessage* msg) const { (void)name; (void)index; (void)msg; return B_ERROR; }
    status_t FindRef(const char* name, int32 index, entry_ref* ref) const { (void)name; (void)index; (void)ref; return B_ERROR; }
    status_t FindRef(const char* name, entry_ref* ref) const { (void)name; (void)ref; return B_ERROR; }
    
    ssize_t FlattenedSize() const { return 0; }
    status_t Flatten(void* buf, ssize_t size) const { (void)buf; (void)size; return B_OK; }
    status_t Unflatten(const void* buf, ssize_t size) { (void)buf; (void)size; return B_OK; }
    
    bool HasData(const char* name, uint32_t type) const { (void)name; (void)type; return false; }
    bool HasString(const char* name) const { (void)name; return false; }
    bool HasInt32(const char* name) const { (void)name; return false; }
    bool HasBool(const char* name) const { (void)name; return false; }
    
    // Scripting support - matches real Haiku API
    status_t GetCurrentSpecifier(int32* index = nullptr, BMessage* specifier = nullptr, int32* what = nullptr, const char** property = nullptr) const { 
        (void)index; (void)specifier; (void)what; (void)property; return B_OK; 
    }
    status_t SendReply(BMessage* reply) const { (void)reply; return B_OK; }
    
    static const int32_t B_OK = 0;
    static const int32_t B_ERROR = -1;
    static const int32_t B_ENTRY_NOT_FOUND = -2;
    static const int32_t B_BAD_VALUE = -3;
    static const int32_t B_NO_MEMORY = -4;
    static const int32_t B_TIMED_OUT = -5;
    
    // Scripting constants
    static const uint32 B_GET_PROPERTY = 'BGPB';
    static const uint32 B_SET_PROPERTY = 'BSPB';
    static const uint32 B_CREATE_PROPERTY = 'BCPB';
    static const uint32 B_DELETE_PROPERTY = 'BDPB';
    static const uint32 B_EXECUTE_PROPERTY = 'BEPB';
    static const uint32 B_COUNT_PROPERTIES = 'BCOP';
    static const uint32 B_QUIT_REQUESTED = 'BQRQ';
    
    status_t SendMessage(BMessage* msg) { (void)msg; return B_OK; }
    status_t SendMessage(uint32 what) { (void)what; return B_OK; }
};

// Forward declare BHandler, BLooper, BView, and BWindow for BMessenger
class BHandler;
class BLooper;
class BView;
class BWindow;

// BMessenger - for sending messages to handlers/loopers
class BMessenger {
public:
    BMessenger() = default;
    BMessenger(const BHandler* handler, const BLooper* looper = nullptr) { (void)handler; (void)looper; }
    BMessenger(const BHandler* handler) { (void)handler; }
    BMessenger(BHandler* handler) { (void)handler; }
    BMessenger(const BView* view) { (void)view; }
    BMessenger(BView* view) { (void)view; }
    BMessenger(const BWindow* window) { (void)window; }
    BMessenger(BWindow* window) { (void)window; }
    template<typename T>
    BMessenger(T* ptr) { (void)ptr; }
    BMessenger(const char* signature, team_id team = -1) { (void)signature; (void)team; }
    BMessenger(const BMessenger& other) = default;
    ~BMessenger() = default;
    
    BMessenger& operator=(const BMessenger& other) = default;
    
    status_t SendMessage(BMessage* msg) const { (void)msg; return B_OK; }
    status_t SendMessage(uint32 what) const { (void)what; return B_OK; }
    status_t SendMessage(BMessage* msg, BHandler* handler) const { (void)msg; (void)handler; return B_OK; }
    status_t SendMessage(BMessage* msg, const BHandler* handler) const { (void)msg; (void)handler; return B_OK; }
    
    status_t SendMessage(BMessage* msg, BHandler* handler, BHandler* replyTo, bigtime_t timeout) const { 
        (void)msg; (void)handler; (void)replyTo; (void)timeout; return B_OK; }
    status_t SendMessage(BMessage* msg, const BHandler* handler, BHandler* replyTo, bigtime_t timeout) const { 
        (void)msg; (void)handler; (void)replyTo; (void)timeout; return B_OK; }
    
    // 4-arg version with send and receive timeouts
    status_t SendMessage(BMessage* msg, BMessage* reply, bigtime_t sendTimeout, bigtime_t recvTimeout) const {
        (void)msg; (void)reply; (void)sendTimeout; (void)recvTimeout; return B_OK; }
    status_t SendMessage(BMessage* msg, BMessage* reply, bigtime_t sendTimeout, bigtime_t recvTimeout, BHandler* handler) const {
        (void)msg; (void)reply; (void)sendTimeout; (void)recvTimeout; (void)handler; return B_OK; }
    
    bool IsValid() const { return false; }
    bool IsTargetLocal() const { return false; }
    BHandler* Target(BLooper** looper = nullptr) const { (void)looper; return nullptr; }
    team_id Team() const { return -1; }
};

// BeOS scripting constants
const uint32 B_REFS_RECEIVED = 'refs';
const uint32 B_REPLY = 'BREP';
const uint32 B_NO_REPLY = 'BNRE';
const uint32 B_QUIT_REQUESTED = 'BQRQ';
const uint32 B_GET_PROPERTY = 'BGPB';
const uint32 B_SIMPLE_DATA = 'BsdT';
const uint32 B_SAVE_REQUESTED = 'BsvR';

#endif