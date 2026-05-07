/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef IMAGE_CACHE_H
#define IMAGE_CACHE_H

#include <Bitmap.h>
#include <Referenceable.h>
#include <String.h>

#include <map>


class ImageCache : public BReferenceable {
public:
								ImageCache();
	virtual						~ImageCache();

			BBitmap*			LoadImage(const BString& path);
			void				Clear();

			void				DumpInfo();

private:
			BBitmap*			_LoadFromFile(const BString& path);

			std::map<BString, BBitmap*>	fCache;
};


typedef BReference<ImageCache> ImageCacheRef;


#endif // IMAGE_CACHE_H
