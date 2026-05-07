/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef PARAGRAPH_STYLE_H
#define PARAGRAPH_STYLE_H

#include "ParagraphStyleData.h"


class ParagraphStyle {
public:
								ParagraphStyle();
								ParagraphStyle(const ParagraphStyle& other);

			ParagraphStyle&		operator=(const ParagraphStyle& other);
			bool				operator==(const ParagraphStyle& other) const;
			bool				operator!=(const ParagraphStyle& other) const;

			bool				SetAlignment(::Alignment alignment);
			::Alignment			Alignment() const;

			bool				SetJustify(bool justify);
			bool				Justify() const;

			bool				SetFirstLineInset(float inset);
			float				FirstLineInset() const;

			bool				SetLineInset(float inset);
			float				LineInset() const;

			bool				SetLineSpacing(float spacing);
			float				LineSpacing() const;

			bool				SetSpacingTop(float spacing);
			float				SpacingTop() const;

			bool				SetSpacingBottom(float spacing);
			float				SpacingBottom() const;

			bool				SetBullet(const ::Bullet& bullet);
			const ::Bullet&		Bullet() const;

			bool				SetKind(::ParagraphKind kind);
			::ParagraphKind		Kind() const;

			bool				SetBackgroundColor(color_which which);
			bool				SetBackgroundColor(rgb_color color);
			bool				HasBackgroundColor() const;
			rgb_color			BackgroundColor() const;

			bool				SetForegroundColor(color_which which);
			bool				SetForegroundColor(rgb_color color);
			rgb_color			ForegroundColor() const;
			bool		 		HasForegroundColor() const;

private:
			ParagraphStyleDataRef fStyleData;
};


#endif // PARAGRAPH_STYLE_H
