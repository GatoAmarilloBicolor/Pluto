#ifndef MOCK_IMAGE_H
#define MOCK_IMAGE_H

#include "Types.h"

#define B_CURRENT_TEAM 0

struct image_info {
    image_id id;
    char name[256];
    char path[256];
    void* text;
    void* data;
    int32 text_size;
    int32 data_size;
    int32 type;
};

inline status_t get_next_image_info(team_id team, int32* cookie, image_info* info) {
    (void)team; (void)cookie; (void)info;
    return -1;
}

inline status_t load_image(int32 argc, const char** argv, const char** envp) {
    (void)argc; (void)argv; (void)envp;
    return -1;
}

inline status_t unload_image(image_id id) {
    (void)id;
    return -1;
}

inline status_t get_image_symbol(image_id id, const char* name, int32_t type, void** out) {
    (void)id; (void)name; (void)type; (void)out;
    return -1;
}

#endif
