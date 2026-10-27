#ifndef MOCK_KERNEL_IMAGE_H
#define MOCK_KERNEL_IMAGE_H

#include "SupportDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Image types (matching Haiku kernel/image.h)
#define B_APP_IMAGE 1
#define B_LIBRARY_IMAGE 2
#define B_ADD_ON_IMAGE 3
#define B_SYSTEM_IMAGE 4

// Symbol types
#define B_SYMBOL_TYPE_DATA 0x1
#define B_SYMBOL_TYPE_TEXT 0x2
#define B_SYMBOL_TYPE_ANY 0x5

// Image info structure (matching Haiku kernel/image.h)
struct image_info {
	image_id id;
	int32 type;  // image_type
	int32 sequence;
	int32 init_order;
	void (*init_routine)();
	void (*term_routine)();
	dev_t device;
	ino_t node;
	char name[256];  // MAXPATHLEN
	void* text;
	void* data;
	int32 text_size;
	int32 data_size;
	int32 api_version;
	int32 abi;
};

// Function prototypes (matching Haiku API)
image_id load_add_on(const char* path);
status_t unload_add_on(image_id image);
status_t get_image_symbol(image_id image, const char* name, int32 symbolType, void** outSymbol);
status_t get_next_image_info(team_id team, int32* cookie, image_info* info);
team_id find_team(const char* name);
status_t get_image_info(image_id image, image_info* info, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MOCK_KERNEL_IMAGE_H