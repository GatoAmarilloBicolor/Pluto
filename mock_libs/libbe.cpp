// Mock Haiku libbe - minimal stubs for linking on non-Haiku platforms
// Only provides global symbols and kernel functions; method implementations
// are inline in mock headers and compiled directly into application code.
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "../mock_headers/haiku/Types.h"
#include "../mock_headers/haiku/kernel/image.h"

extern "C" {

// Kernel add-on functions (from <kernel/image.h>) - implementations
image_id load_add_on(const char* path) { return -1; }
status_t unload_add_on(image_id image) { return 0; }
status_t get_image_symbol(image_id image, const char* name, int32 symbolType, void** outSymbol) { return -1; }
status_t get_next_image_info(team_id team, int32* cookie, image_info* info) { return -1; }

// Thread functions
thread_id spawn_thread(void (*func)(void*), const char* name, int32 priority, void* data) { return -1; }
status_t resume_thread(thread_id id) { (void)id; return B_OK; }
status_t wait_for_thread(thread_id id, status_t* return_value) { (void)id; (void)return_value; return B_OK; }
status_t kill_thread(thread_id id) { (void)id; return B_OK; }
thread_id find_thread(const char* name) { (void)name; return -1; }

} // extern "C"

#include "../mock_headers/haiku/Application.h"

// BApplication/roster globals (C++ linkage)
BApplication* BApplication::be_app = nullptr;
BRoster* BRoster::be_roster = nullptr;
BApplication* be_app = nullptr;
BRoster* be_roster = nullptr;

status_t GetAppInfo(app_info* info) {
    (void)info;
    return B_OK;
}