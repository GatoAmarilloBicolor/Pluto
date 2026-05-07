/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef TEXT_SPAN_H
#define TEXT_SPAN_H


#include <Bitmap.h>
#include <Cursor.h>
#include <String.h>
#include <View.h>

#include "CharacterStyle.h"


class TextSpan {
public:
								TextSpan();
								TextSpan(const BString& text,
									const CharacterStyle& style);
								TextSpan(const TextSpan& other);

			TextSpan&			operator=(const TextSpan& other);
			bool				operator==(const TextSpan& other) const;
			bool				operator!=(const TextSpan& other) const;

			void				SetText(const BString& text);
	inline	const BString&		Text() const
									{ return fText; }

			void				SetStyle(const CharacterStyle& style);
	inline	const CharacterStyle& Style() const
									{ return fStyle; }

	inline	int32				CountChars() const
									{ return fCharCount; }

			bool				Append(const BString& text);
			bool				Insert(int32 offset, const BString& text);
			bool				Remove(int32 start, int32 count);

			TextSpan			SubSpan(int32 start, int32 count) const;

			void				SetCursor(const BCursor& cursor);
	inline	const BCursor&		Cursor() const
									{ return fCursor; }
			void				SetClickMessage(BMessage* message);
	inline	const BMessage*		ClickMessage() const
									{ return &fClickMessage; }

			// Image support
			void				SetImage(const BBitmap* bitmap,
									const BString& altText);
	inline	const BBitmap*		Image() const
									{ return fImage; }
	inline	bool				IsImage() const
									{ return fImage != NULL; }
	inline	const BString&		AltText() const
									{ return fAltText; }

private:
			void				_TruncateInsert(int32& start) const;
			void				_TruncateRemove(int32& start,
									int32& count) const;

private:
			BString				fText;
			int32				fCharCount;
			CharacterStyle		fStyle;

			BCursor				fCursor;
			BMessage			fClickMessage;

			// Image data (not owned, managed by ImageCache)
			const BBitmap*		fImage;
			BString				fAltText;
};


#endif // TEXT_SPAN_H
