/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "CharacterStyleData.h"

#include <new>


CharacterStyleData::CharacterStyleData()
	:
	BReferenceable(),
	StyleColoursData(),
	fFont(),

	fAscent(-1.0f),
	fDescent(-1.0f),
	fWidth(-1.0f),

	fGlyphSpacing(0.0f),

	fWhichStrikeOutColor(B_NO_COLOR), //deprecated!
	fWhichUnderlineColor(B_NO_COLOR), //deprecated!

	fStrikeOutColor(ForegroundColor()),
	fUnderlineColor(ForegroundColor()),

	fStrikeOutStyle(STRIKE_OUT_NONE),
	fUnderlineStyle(UNDERLINE_NONE),

	fInsetLeft(0.0f),
	fInsetTop(0.0f),
	fInsetRight(0.0f),
	fInsetBottom(0.0f)
{
}


CharacterStyleData::CharacterStyleData(const CharacterStyleData& other)
	:
	BReferenceable(),
	StyleColoursData(other),
	fFont(other.fFont),

	fAscent(other.fAscent),
	fDescent(other.fDescent),
	fWidth(other.fWidth),

	fGlyphSpacing(other.fGlyphSpacing),

	fWhichStrikeOutColor(other.fWhichStrikeOutColor),
	fWhichUnderlineColor(other.fWhichUnderlineColor),

	fStrikeOutColor(other.fStrikeOutColor),
	fUnderlineColor(other.fUnderlineColor),

	fStrikeOutStyle(other.fStrikeOutStyle),
	fUnderlineStyle(other.fUnderlineStyle),

	fInsetLeft(other.fInsetLeft),
	fInsetTop(other.fInsetTop),
	fInsetRight(other.fInsetRight),
	fInsetBottom(other.fInsetBottom)
{
}


bool
CharacterStyleData::operator==(const CharacterStyleData& other) const
{
	if (this == &other)
		return true;

	return fFont == other.fFont
		&& fAscent == other.fAscent
		&& fDescent == other.fDescent
		&& fWidth == other.fWidth

		&& fGlyphSpacing == other.fGlyphSpacing

		&& StyleColoursData::operator==(other)

		&& fWhichStrikeOutColor == other.fWhichStrikeOutColor
		&& fWhichUnderlineColor == other.fWhichUnderlineColor

		&& fStrikeOutColor == other.fStrikeOutColor

		&& fInsetLeft == other.fInsetLeft
		&& fInsetTop == other.fInsetTop
		&& fInsetRight == other.fInsetRight
		&& fInsetBottom == other.fInsetBottom;
}


bool
CharacterStyleData::operator!=(const CharacterStyleData& other) const
{
	return !(*this == other);
}


CharacterStyleDataRef
CharacterStyleData::SetFont(const BFont& font)
{
	if (fFont == font)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fFont = font;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetAscent(float ascent)
{
	if (fAscent == ascent)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fAscent = ascent;
	return CharacterStyleDataRef(ret, true);
}


float
CharacterStyleData::Ascent() const
{
	if (fAscent >= 0.0f)
		return fAscent;

	font_height fontHeight;
	fFont.GetHeight(&fontHeight);
	return fontHeight.ascent;
}


CharacterStyleDataRef
CharacterStyleData::SetDescent(float descent)
{
	if (fDescent == descent)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fDescent = descent;
	return CharacterStyleDataRef(ret, true);
}


float
CharacterStyleData::Descent() const
{
	if (fDescent >= 0.0f)
		return fDescent;

	font_height fontHeight;
	fFont.GetHeight(&fontHeight);
	return fontHeight.descent;
}


CharacterStyleDataRef
CharacterStyleData::SetWidth(float width)
{
	if (fWidth == width)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fWidth = width;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetGlyphSpacing(float glyphSpacing)
{
	if (fGlyphSpacing == glyphSpacing)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fGlyphSpacing = glyphSpacing;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetForegroundColor(color_which which)
{
	return SetForegroundColor(ui_color(which));
}


CharacterStyleDataRef
CharacterStyleData::SetForegroundColor(rgb_color color)
{
	if (ForegroundColor() == color)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->StyleColoursData::SetForegroundColor(color);
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetBackgroundColor(color_which which)
{
	return SetBackgroundColor(ui_color(which));
}


CharacterStyleDataRef
CharacterStyleData::SetBackgroundColor(rgb_color color)
{
	if (BackgroundColor() == color)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->StyleColoursData::SetBackgroundColor(color);
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetStrikeOutColor(color_which which)
{
	if (fWhichStrikeOutColor == which)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fWhichStrikeOutColor = which;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetStrikeOutColor(rgb_color color)
{
	if (fStrikeOutColor == color)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fStrikeOutColor = color;
	ret->fWhichStrikeOutColor = B_NO_COLOR;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetUnderlineColor(color_which which)
{
	if (fWhichUnderlineColor == which)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fWhichUnderlineColor = which;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetUnderlineColor(rgb_color color)
{
	if (fUnderlineColor == color)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fUnderlineColor = color;
	ret->fWhichUnderlineColor = B_NO_COLOR;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetStrikeOut(uint8 strikeOut)
{
	if (fStrikeOutStyle == strikeOut)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fStrikeOutStyle = strikeOut;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetUnderline(uint8 underline)
{
	if (fUnderlineStyle == underline)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fUnderlineStyle = underline;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsetLeft(float inset)
{
	if (fInsetLeft == inset)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fInsetLeft = inset;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsetTop(float inset)
{
	if (fInsetTop == inset)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fInsetTop = inset;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsetRight(float inset)
{
	if (fInsetRight == inset)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fInsetRight = inset;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsetBottom(float inset)
{
	if (fInsetBottom == inset)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fInsetBottom = inset;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsets(float left, float top, float right, float bottom)
{
	if (fInsetLeft == left && fInsetTop == top 
		&& fInsetRight == right && fInsetBottom == bottom)
		return CharacterStyleDataRef(this);

	CharacterStyleData* ret = new(std::nothrow) CharacterStyleData(*this);
	if (ret == NULL)
		return CharacterStyleDataRef(this);

	ret->fInsetLeft = left;
	ret->fInsetTop = top;
	ret->fInsetRight = right;
	ret->fInsetBottom = bottom;
	return CharacterStyleDataRef(ret, true);
}


CharacterStyleDataRef
CharacterStyleData::SetInsets(float horizontal, float vertical)
{
	return SetInsets(horizontal, vertical, horizontal, vertical);
}


CharacterStyleDataRef
CharacterStyleData::SetInsets(float all)
{
	return SetInsets(all, all, all, all);
}


// #pragma mark - private


CharacterStyleData&
CharacterStyleData::operator=(const CharacterStyleData& other)
{
	return *this;
}

