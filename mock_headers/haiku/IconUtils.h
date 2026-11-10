#ifndef MOCK_ICON_UTILS_H
#define MOCK_ICON_UTILS_H

#include "Bitmap.h"
#include "Types.h"

namespace BIconUtils {
    status_t GetVectorIcon(const char* name, BBitmap* icon, float size) {
        (void)name; (void)icon; (void)size;
        return B_OK;
    }

    status_t GetVectorIcon(type_code type, const char* mime, BBitmap* icon, float size) {
        (void)type; (void)mime; (void)icon; (void)size;
        return B_OK;
    }

    status_t GetAppIcon(const char* signature, BBitmap* icon, float size) {
        (void)signature; (void)icon; (void)size;
        return B_OK;
    }

    status_t GetGenericIcon(BBitmap* icon, float size, const char* category) {
        (void)icon; (void)size; (void)category;
        return B_OK;
    }

    // Used by DeskbarReplicant - raw icon data
    status_t GetVectorIcon(const void* data, size_t size, BBitmap* icon) {
        (void)data; (void)size; (void)icon;
        return B_OK;
    }
}

#endif