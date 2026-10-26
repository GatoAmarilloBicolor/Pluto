#ifndef MOCK_TYPES_H
#define MOCK_TYPES_H

// Basic Haiku types - use existing system types where available
#ifndef __HAIKU__
// On non-Haiku systems, use system types
#include <sys/types.h>
#endif

// Only define Haiku-specific types that don't exist on POSIX systems
#ifndef status_t
typedef long status_t;
#endif

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

#ifndef int32
typedef int int32;
#endif

#ifndef int64
typedef long long int64;
#endif

#ifndef int64_t
typedef long long int64_t;
#endif

#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

#ifndef thread_id
typedef int thread_id;
#endif

#ifndef bigtime_t
typedef long long bigtime_t;
#endif

#ifndef image_id
typedef int32 image_id;
#endif

#ifndef team_id
typedef int32 team_id;
#endif

const status_t B_OK = 0;
const status_t B_ERROR = -1;
const status_t B_ENTRY_NOT_FOUND = -2;
const status_t B_BAD_VALUE = -3;
const status_t B_NO_MEMORY = -4;
const status_t B_NOT_ALLOWED = -5;
const status_t B_NAME_TOO_LONG = -6;
const status_t B_WOULD_BLOCK = -7;
const status_t B_INTERRUPTED = -8;
const status_t B_TIMED_OUT = -9;
const status_t B_FILE_EXISTS = -10;
const status_t B_DEVICE_FULL = -11;
const status_t B_BAD_TYPE = -12;
const status_t B_CANCELED = -13;
const status_t B_NOT_A_DIRECTORY = -14;
const status_t B_IS_A_DIRECTORY = -15;
const status_t B_PATH_TOO_LONG = -16;
const status_t B_MISMATCHED_VALUES = -17;
const status_t B_BUFFER_OVERFLOW = -18;
const status_t B_IO_ERROR = -19;
const status_t B_PERMISSION_DENIED = -20;
const status_t B_BAD_SEMAPHORE_ID = -21;
const status_t B_BAD_PORT_ID = -22;
const status_t B_BAD_THREAD_ID = -23;
const status_t B_BAD_TEAM_ID = -24;
const status_t B_BAD_IMAGE_ID = -25;

// BFS attribute types
#define B_STRING_TYPE 0
#define B_INT64_TYPE 1
#define B_INT32_TYPE 2
#define B_FLOAT_TYPE 3
#define B_DOUBLE_TYPE 4
#define B_RAW_TYPE 5
#define B_POINTER_TYPE 6
#define B_MESSENGER_TYPE 7
#define B_MESSAGE_TYPE 8
#define B_TIME_TYPE 9
#define B_MIME_TYPE 10
#define B_REF_TYPE 11
#define B_COLOR_TYPE 12
#define B_RECT_TYPE 13
#define B_POINT_TYPE 14
#define B_SIZE_TYPE 15
#define B_PATTERN_TYPE 16
#define B_BITMAP_TYPE 17
#define B_MONITOR_TYPE 18
#define B_CURSOR_TYPE 19
#define B_HANDLER_TYPE 20
#define B_APPLICATION_TYPE 21
#define B_WINDOW_TYPE 22
#define B_VIEW_TYPE 23
#define B_MENU_TYPE 24
#define B_MENU_ITEM_TYPE 25
#define B_SCREEN_TYPE 26
#define B_CLIPBOARD_TYPE 27
#define B_TRANSLATOR_TYPE 28
#define B_BROWSER_TYPE 29
#define B_ARCHIVABLE_TYPE 30
#define B_ALERT_TYPE 31
#define B_FILE_PANEL_TYPE 32
#define B_OPEN_PANEL_TYPE 33
#define B_SAVE_PANEL_TYPE 34
#define B_FONT_TYPE 36
#define B_TYPED_LIST_TYPE 38
#define B_ANY_TYPE 39
#define B_SYMBOL_TYPE_TEXT 40

struct entry_ref {
    dev_t device;
    ino_t directory;
    char name[256];
};

struct attr_info {
    uint32_t type;
    off_t size;
};

const int B_FILE_NAME_LENGTH = 255;

// POSIX environ
extern char** environ;

// Forward declarations to avoid circular dependencies
class BEntry;
class BDirectory;
class BFile;
class BPath;

#endif