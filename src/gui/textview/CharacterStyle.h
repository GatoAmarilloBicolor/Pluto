/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef CHARACTER_STYLE_H
#define CHARACTER_STYLE_H

#include "CharacterStyleData.h"


class CharacterStyle {
public:
								CharacterStyle();
								CharacterStyle(const CharacterStyle& other);

			CharacterStyle&		operator=(const CharacterStyle& other);
			bool				operator==(const CharacterStyle& other) const;
			bool				operator!=(const CharacterStyle& other) const;

			bool				SetFont(const BFont& font);
			const BFont&		Font() const;

			bool				SetFontSize(float size);
			float				FontSize() const;

			bool				SetBold(bool bold);
			bool				IsBold() const;

			bool				SetItalic(bool italic);
			bool				IsItalic() const;

			bool				SetStrikeout(bool strikeout);
			bool				IsStrikeout(bool strikeout);

			bool				SetAscent(float ascent);
			float				Ascent() const;

			bool				SetDescent(float descent);
			float				Descent() const;

			bool				SetWidth(float width);
			float				Width() const;

			bool				SetGlyphSpacing(float spacing);
			float				GlyphSpacing() const;

			bool				SetForegroundColor(
									uint8 r, uint8 g, uint8 b,
									uint8 a = 255);
			bool				SetForegroundColor(color_which which);
			bool				SetForegroundColor(rgb_color color);
			rgb_color			ForegroundColor() const;
			bool				HasForegroundColor() const;

			bool				SetBackgroundColor(
									uint8 r, uint8 g, uint8 b,
									uint8 a = 255);
			bool				SetBackgroundColor(color_which which);
			bool				SetBackgroundColor(rgb_color color);
			rgb_color			BackgroundColor() const;
			color_which			WhichBackgroundColor() const;

			bool				SetStrikeOutColor(color_which which);
			bool				SetStrikeOutColor(rgb_color color);
			rgb_color			StrikeOutColor() const;
			bool				HasBackgroundColor() const;

			bool				SetUnderlineColor(color_which which);
			bool				SetUnderlineColor(rgb_color color);
			rgb_color			UnderlineColor() const;
			color_which			WhichUnderlineColor() const;

			bool				SetStrikeOut(uint8 strikeOut);
			uint8				StrikeOut() const;

			bool				SetUnderline(uint8 underline);
			uint8				Underline() const;

			bool				SetInsetLeft(float inset);
			float				InsetLeft() const;

			bool				SetInsetTop(float inset);
			float				InsetTop() const;

			bool				SetInsetRight(float inset);
			float				InsetRight() const;

			bool				SetInsetBottom(float inset);
			float				InsetBottom() const;

			// Convenience methods for setting multiple insets at once
			bool				SetInsets(float left, float top,
								float right, float bottom);
			bool				SetInsets(float horizontal, float vertical);
			bool				SetInsets(float all);

private:
			BFont				_FindFontForFace(uint16 face) const;
			bool				_SetFontFaceFlag(uint16 face_flag, bool enable);

private:
			CharacterStyleDataRef fStyleData;
};


#endif // CHARACTER_STYLE_H
