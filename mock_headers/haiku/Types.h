#ifndef MOCK_TYPES_H
#define MOCK_TYPES_H

// Basic Haiku types - use existing system types where available
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <initializer_list>
#include <algorithm>
#include <vector>

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

#ifndef uint32
typedef uint32_t uint32;
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

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

// Include Font.h for BFont definition needed by text_run (after typedefs)
#include "Font.h"

// Layout spacing constants
const float B_USE_SMALL_SPACING = 4.0f;
const float B_USE_DEFAULT_SPACING = 8.0f;
const float B_USE_HALF_ITEM_SPACING = 6.0f;
const float B_USE_ITEM_SPACING = 8.0f;
const float B_USE_BIG_SPACING = 16.0f;

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
const status_t B_TIMEOUT = B_TIMED_OUT;
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

// Message codes for drag and drop
const uint32 B_COPY_TARGET = 'BCTR';

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
#define B_BOOL_TYPE 41
#define B_RGB_COLOR_TYPE 32

struct rgb_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};

struct entry_ref {
    dev_t device;
    ino_t directory;
    char name[256];
    
    void set_name(const char* n) { 
        if (n) { 
            size_t nlen = strlen(n); 
            size_t copylen = nlen < sizeof(name)-1 ? nlen : sizeof(name)-1; 
            memcpy(name, n, copylen); 
            name[copylen] = '\0'; 
        } 
    }
};

struct attr_info {
    uint32_t type;
    off_t size;
};

const int B_FILE_NAME_LENGTH = 255;
const int B_ATTR_NAME_LENGTH = 255;

typedef uint32_t type_code;

// Missing Haiku types
typedef enum {
	B_WHICH_0 = 0,
	B_WHICH_1 = 1,
	B_WHICH_2 = 2,
	B_WHICH_3 = 3,
	B_DOCUMENT_BACKGROUND_COLOR = 4,
	B_DOCUMENT_TEXT_COLOR = 5,
	B_CONTROL_BACKGROUND_COLOR = 6,
	B_CONTROL_TEXT_COLOR = 7,
	B_CONTROL_HIGHLIGHT_COLOR = 8,
	B_NAVIGATION_BASE_COLOR = 9,
	B_NAVIGATION_PULSE_COLOR = 10,
	B_KEYBOARD_NAVIGATION_COLOR = 11,
	B_DESKTOP_COLOR = 12,
	B_PANEL_BACKGROUND_COLOR = 13,
	B_PANEL_TEXT_COLOR = 14,
	B_MENU_BACKGROUND_COLOR = 15,
	B_MENU_SELECTED_BACKGROUND_COLOR = 16,
	B_MENU_SELECTED_BORDER_COLOR = 17,
	B_MENU_ITEM_TEXT_COLOR = 18,
	B_MENU_ITEM_SELECTED_TEXT_COLOR = 19,
	B_TOOL_TIP_BACKGROUND_COLOR = 20,
	B_TOOL_TIP_TEXT_COLOR = 21,
	B_LIST_BACKGROUND_COLOR = 22,
	B_LIST_SELECTED_BACKGROUND_COLOR = 23,
	B_LIST_ITEM_TEXT_COLOR = 24,
	B_LIST_SELECTED_ITEM_TEXT_COLOR = 25,
	B_SUCCESS_COLOR = 26,
	B_FAILURE_COLOR = 27,
	B_WARNING_COLOR = 28,
	B_LINK_COLOR = 29,
	B_LINK_TEXT_COLOR = 29,
	B_WINDOW_TAB_COLOR = 30,
	B_WINDOW_TAB_SELECTED_COLOR = 31,
} color_which;

// Allow implicit conversion from rgb_color to color_which for ui_color()
inline color_which operator+(color_which lhs, const rgb_color& rhs) { (void)rhs; return lhs; }
inline color_which operator+(const rgb_color& lhs, color_which rhs) { (void)lhs; return rhs; }

typedef uint32_t color_space;

#ifndef uint8
typedef unsigned char uint8;
#endif

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef uint32
typedef uint32_t uint32;
#endif

#ifndef uint64
typedef unsigned long long uint64;
#endif

// Missing constants
const int B_MIME_TYPE_LENGTH = 256;
const status_t B_NAME_NOT_FOUND = -40;
const status_t B_ALREADY_RUNNING = -47;
const color_space B_RGB32 = 0;
const color_space B_RGB16 = 1;
const color_space B_GRAY8 = 3;
const color_space B_RGBA32 = 4;
const color_space B_RGBA32_BIG = 5;

// Window flags
const uint32 B_NOT_ZOOMABLE = 0x0001;

// Node flavors
const uint32 B_FILE_NODE = 0x0001;
const uint32 B_DIRECTORY_NODE = 0x0002;
const uint32 B_SYMLINK_NODE = 0x0004;

// File open modes
const uint32 B_READ_ONLY = 0x0001;
const uint32 B_WRITE_ONLY = 0x0002;
const uint32 B_READ_WRITE = 0x0003;
const uint32 B_CREATE_FILE = 0x0100;
const uint32 B_ERASE_FILE = 0x0200;
const uint32 B_OPEN_AT_END = 0x0400;
const uint32 B_FAIL_IF_EXISTS = 0x0800;

// Keyboard constants
const int32 B_ESCAPE = 0x1B;
const int32 B_LEFT_ARROW = 0x1C;
const int32 B_RIGHT_ARROW = 0x1D;
const int32 B_UP_ARROW = 0x1E;
const int32 B_DOWN_ARROW = 0x1F;
const int32 B_INSERT = 0x20;
const int32 B_DELETE = 0x21;
const int32 B_HOME = 0x22;
const int32 B_END = 0x23;
const int32 B_PAGE_UP = 0x24;
const int32 B_PAGE_DOWN = 0x25;
const int32 B_FUNCTION_KEY = 0x26;
const int32 B_TAB = 0x09;
const int32 B_ENTER = 0x0A;
const int32 B_RETURN = 0x0D;
const int32 B_SPACE = 0x20;
const int32 B_BACKSPACE = 0x08;

// truncate_mode enum for BView::TruncateString
enum truncate_mode {
    B_TRUNCATE_BEGIN = 0,
    B_TRUNCATE_MIDDLE = 1,
    B_TRUNCATE_END = 2,
    B_TRUNCATE_SMART = 3
};

// Control states
const int32 B_CONTROL_ON = 1;
const int32 B_CONTROL_OFF = 0;
const int32 B_CONTROL_PARTIALLY_ON = 2;

// Keyboard modifiers
const uint32 B_SHIFT_KEY = 0x01;
const uint32 B_COMMAND_KEY = 0x02;
const uint32 B_CONTROL_KEY = 0x04;
const uint32 B_OPTION_KEY = 0x08;
const uint32 B_CAPS_LOCK = 0x10;
const uint32 B_SCROLL_LOCK = 0x20;
const uint32 B_NUM_LOCK = 0x40;
const uint32 B_MENU_KEY = 0x80;

// Mouse event masks
const uint32 B_POINTER_EVENTS = 0x01;
const uint32 B_NO_POINTER_HISTORY = 0x02;
const uint32 B_KEYBOARD_EVENTS = 0x04;
const uint32 B_TRACKING_EVENTS = 0x08;

// Mouse transit constants
const uint32 B_ENTERED_VIEW = 1;
const uint32 B_INSIDE_VIEW = 2;
const uint32 B_EXITED_VIEW = 3;
const uint32 B_MOUSE_WHEEL_CHANGED = 4;

// Thread priorities
const int32 B_LOW_PRIORITY = 10;
const int32 B_NORMAL_PRIORITY = 20;
const int32 B_HIGH_PRIORITY = 30;
const int32 B_URGENT_DISPLAY_PRIORITY = 40;
const int32 B_REAL_TIME_DISPLAY_PRIORITY = 50;
const int32 B_URGENT_PRIORITY = 60;
const int32 B_REAL_TIME_PRIORITY = 70;

// Alignment
struct BAlignment {
    enum horizontal_alignment { B_ALIGN_HORIZONTAL_UNSET, B_ALIGN_LEFT, B_ALIGN_RIGHT, B_ALIGN_CENTER, B_ALIGN_HORIZONTAL_CENTER = B_ALIGN_CENTER, B_ALIGN_USE_FULL_WIDTH };
    enum vertical_alignment { B_ALIGN_VERTICAL_UNSET, B_ALIGN_TOP, B_ALIGN_MIDDLE, B_ALIGN_BOTTOM, B_ALIGN_VERTICAL_CENTER = B_ALIGN_MIDDLE, B_ALIGN_NO_VERTICAL };
    horizontal_alignment horizontal;
    vertical_alignment vertical;
    BAlignment() : horizontal(B_ALIGN_HORIZONTAL_UNSET), vertical(B_ALIGN_VERTICAL_UNSET) {}
    BAlignment(horizontal_alignment h, vertical_alignment v) : horizontal(h), vertical(v) {}
};

// text_run_array for styled text (used by BTextView)
struct text_run {
    int32 offset = 0;
    class BFont* font = nullptr;
    rgb_color color = {0,0,0,0};
    text_run() = default;
    text_run(int32 o, class BFont* f, rgb_color c) : offset(o), font(f), color(c) {}
    text_run(int32 o, class BFont& f, rgb_color c) : offset(o), font(&f), color(c) {}
    text_run& operator=(const text_run& other) = default;
    text_run& operator=(std::initializer_list<void*> list) {
        auto it = list.begin();
        if (it != list.end()) offset = (int32)(intptr_t)*it++;
        if (it != list.end()) font = (class BFont*)*it++;
        if (it != list.end()) color = *(rgb_color*)*it;
        return *this;
    }
};

struct text_run_array {
    int32 count;
    std::vector<text_run> runs;
    text_run_array() : count(0) {}
    text_run_array(int32 c) : count(c), runs(c) {}
    text_run_array(std::initializer_list<text_run> list) : count((int32)list.size()), runs(list) {}
    text_run& operator[](size_t i) { return runs[i]; }
    const text_run& operator[](size_t i) const { return runs[i]; }
};

// Mouse button constants
const uint32 B_PRIMARY_MOUSE_BUTTON = 0x01;
const uint32 B_SECONDARY_MOUSE_BUTTON = 0x02;
const uint32 B_TERTIARY_MOUSE_BUTTON = 0x04;

// Layout spacing constants (B_USE_SMALL_SPACING defined at line 56)
const float B_USE_WINDOW_SPACING = -2.0f;

// GroupLayout orientation (also available globally for compatibility)
const int B_HORIZONTAL = 0;
const int B_VERTICAL = 1;

typedef int orientation;

// Window flags (additional)
const uint32 B_ASYNCHRONOUS_CONTROLS = 0x00001000;

// UTF-8 ellipsis character
#define B_UTF8_ELLIPSIS "\xE2\x80\xA6"

// BSize constants
const float B_SIZE_UNSET = -1.0f;
const float B_SIZE_UNLIMITED = -2.0f;

struct BSize {
    float width, height;
    BSize() : width(B_SIZE_UNSET), height(B_SIZE_UNSET) {}
    BSize(float w, float h) : width(w), height(h) {}
};

const rgb_color B_TRANSPARENT_COLOR = {0, 0, 0, 0};

const int B_MINI_ICON = 16;

// POSIX environ
extern char** environ;

// Missing Haiku types
typedef enum {
    B_ALIGN_LEFT = 0,
    B_ALIGN_RIGHT = 1,
    B_ALIGN_CENTER = 2,
    B_ALIGN_HORIZONTAL_CENTER = 2,
    B_ALIGN_VERTICAL_CENTER = 3,
    B_ALIGN_USE_FULL_WIDTH = 4
} alignment;

typedef enum {
    B_NO_BORDER = 0,
    B_PLAIN_BORDER = 1,
    B_FANCY_BORDER = 2
} border_style;

// Translator constants
#define B_TRANSLATOR_BITMAP 1

// Notification types
#define B_IMPORTANT_NOTIFICATION 4

// Screen/color constants
const uint32 B_COLORS_UPDATED = 0x434C5250;  // 'CLRP'
const color_which B_NO_COLOR = static_cast<color_which>(-1);

// Forward declarations
class BMessage;

// BArchivable interface
class BArchivable {
public:
    virtual ~BArchivable() = default;
    virtual status_t Archive(BMessage* archive, bool deep = true) const = 0;
    static BArchivable* Instantiate(BMessage* archive) { (void)archive; return nullptr; }
};

// Forward declarations to avoid circular dependencies
class BEntry;
class BDirectory;
class BFile;
class BPath;
class BNode;

class BScrollBar;
class BScrollView;

// Translator formats
#define B_PNG_FORMAT "image/png"
#define B_TRANSLATOR_BITMAP 1

// Button
class BButton;

// Thread functions
extern "C" thread_id spawn_thread(int32 (*func)(void*), const char* name, int32 priority, void* data);
extern "C" status_t resume_thread(thread_id id);
extern "C" status_t wait_for_thread(thread_id id, status_t* return_value);
extern "C" status_t kill_thread(thread_id id);
extern "C" thread_id find_thread(const char* name);

// POSIX functions
extern "C" char* getcwd(char* buf, size_t size);
#include <sys/stat.h>
extern "C" int stat(const char* path, struct stat* buf);

// Date/Time format constants
enum {
    B_LONG_DATE_FORMAT = 0,
    B_MEDIUM_DATE_FORMAT = 1,
    B_SHORT_DATE_FORMAT = 2,
    B_FULL_DATE_FORMAT = 3,
};

typedef enum {
    B_LONG_TIME_FORMAT = 0,
    B_MEDIUM_TIME_FORMAT = 1,
    B_SHORT_TIME_FORMAT = 2,
    B_FULL_TIME_FORMAT = 3,
} BTimeFormatStyle;

// BDateFormatStyle is same as BTimeFormatStyle in Haiku
typedef BTimeFormatStyle BDateFormatStyle;

// B_COUNT_OF macro
#define B_COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

#endif