#ifndef MOCK_TRANSLATION_UTILS_H
#define MOCK_TRANSLATION_UTILS_H

#include "Bitmap.h"
#include "DataIO.h"

// Mock BTranslationUtils for cross-compilation
// Real Haiku implementation provides GetBitmapFile() and GetBitmap()
// which load images using the translator roster.

class BTranslationUtils {
public:
    // Get bitmap from file using translator roster
    // Returns nullptr on failure (mock always returns nullptr)
    static BBitmap* GetBitmapFile(const char* path) {
        (void)path;
        return nullptr;
    }

    // Get bitmap from BPositionIO
    static BBitmap* GetBitmap(BPositionIO* source) {
        (void)source;
        return nullptr;
    }
};

#endif