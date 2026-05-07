/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "ImageCache.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <TranslatorRoster.h>

#include <stdio.h>


ImageCache::ImageCache()
{
}


ImageCache::~ImageCache()
{
	Clear();
}


BBitmap*
ImageCache::LoadImage(const BString& path)
{
	// Check cache first
	auto it = fCache.find(path);
	if (it != fCache.end()) {
		return it->second;
	}

	// Load from file
	fprintf(stderr, "[ImageCache] Not in cache, loading from file...\n");
	fflush(stderr);
	BBitmap* bitmap = _LoadFromFile(path);

	// Cache it (even if NULL to avoid repeated load attempts)
	fCache[path] = bitmap;

	return bitmap;
}


void
ImageCache::Clear()
{
	// Delete all cached bitmaps
	for (auto& pair : fCache) {
		delete pair.second;
	}
	fCache.clear();
}

void
ImageCache::DumpInfo()
{
	for(auto&& [k, v] : fCache) {
		printf("fCache %s\n", k.String());
	}
}


BBitmap*
ImageCache::_LoadFromFile(const BString& path)
{
	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK) {
		fprintf(stderr, "ImageCache: Failed to open file: %s\n", path.String());
		return NULL;
	}

	BTranslatorRoster* roster = BTranslatorRoster::Default();
	if (roster == NULL) {
		fprintf(stderr, "ImageCache: No translator roster\n");
		return NULL;
	}

	BBitmapStream stream;
	status_t status = roster->Translate(&file, NULL, NULL, &stream, B_TRANSLATOR_BITMAP);
	if (status != B_OK) {
		fprintf(stderr, "ImageCache: Translation failed for %s: %s\n",
			path.String(), strerror(status));
		return NULL;
	}

	BBitmap* bitmap = NULL;
	status = stream.DetachBitmap(&bitmap);
	if (status != B_OK || bitmap == NULL) {
		fprintf(stderr, "ImageCache: Failed to detach bitmap: %s\n", strerror(status));
		return NULL;
	}
	fprintf(stderr, "[ImageCache] Successfully loaded bitmap: %dx%d\n",
		(int)bitmap->Bounds().Width(), (int)bitmap->Bounds().Height());

	// DEBUG: Dump bitmap information
	fprintf(stderr, "[ImageCache] Bitmap info:\n");
	fprintf(stderr, "  - ColorSpace: %d\n", (int)bitmap->ColorSpace());
	fprintf(stderr, "  - BytesPerRow: %d\n", (int)bitmap->BytesPerRow());
	fprintf(stderr, "  - BitsLength: %d\n", (int)bitmap->BitsLength());
	fprintf(stderr, "  - Bits pointer: %p\n", bitmap->Bits());

	// Dump first 32 bytes of bitmap data
	if (bitmap->Bits() != NULL) {
		unsigned char* data = (unsigned char*)bitmap->Bits();
		fprintf(stderr, "  - First 32 bytes: ");
		for (int i = 0; i < 32 && i < bitmap->BitsLength(); i++) {
			fprintf(stderr, "%02x ", data[i]);
		}
		fprintf(stderr, "\n");
	}
	fflush(stderr);

	return bitmap;
}
