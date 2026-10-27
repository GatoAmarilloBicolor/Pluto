#ifndef MOCK_KERNEL_IMAGE_H
#define MOCK_KERNEL_IMAGE_H

#include "SupportDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Image loading flags
#define B_NORMAL_IMAGE 0
#define B_SYSTEM_IMAGE 1
#define B_APP_IMAGE 2

// Symbol types
#define B_SYMBOL_TYPE_TEXT 0
#define B_SYMBOL_TYPE_DATA 1
#define B_SYMBOL_TYPE_ANY 2

// Image info structure
struct image_info {
    int32 api_version;
    int32 flags;
    char name[256];
    char path[1024];
    void* text;
    void* data;
    void* bss;
    int32 text_size;
    int32 data_size;
    int32 bss_size;
    int32 num_symbols;
    int32 num_relocations;
    int32 num_symbol_tables;
    int32 num_string_tables;
    void* symbol_table;
    void* string_table;
    void* relocation_table;
    void* debug_info;
};

// Function prototypes
image_id load_image(const char* path, int32 flags, const char** argv, const char** env);
status_t unload_image(image_id image);
status_t get_image_symbol(image_id image, const char* name, int32 symbolType, void** outSymbol);
status_t get_next_image_info(team_id team, int32* cookie, image_info* info);
team_id find_team(const char* name);
status_t get_image_info(image_id image, image_info* info, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MOCK_KERNEL_IMAGE_H