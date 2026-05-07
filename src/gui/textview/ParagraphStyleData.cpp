/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "ParagraphStyleData.h"

#include <new>


ParagraphStyleData::ParagraphStyleData()
	:
	BReferenceable(),
	StyleColoursData(),
	fAlignment(ALIGN_LEFT),
	fJustify(false),

	fFirstLineInset(0.0f),
	fLineInset(0.0f),
	fLineSpacing(0.0f),

	fSpacingTop(0.0f),
	fSpacingBottom(0.0f),

	fBullet(),
	fKind(PARAGRAPH_KIND_PLAIN)
{
}


ParagraphStyleData::ParagraphStyleData(const ParagraphStyleData& other)
	:
	BReferenceable(),
	StyleColoursData(other),
	fAlignment(other.fAlignment),
	fJustify(other.fJustify),

	fFirstLineInset(other.fFirstLineInset),
	fLineInset(other.fLineInset),
	fLineSpacing(other.fLineSpacing),

	fSpacingTop(other.fSpacingTop),
	fSpacingBottom(other.fSpacingBottom),

	fBullet(other.fBullet),
	fKind(other.fKind)
{
}


bool
ParagraphStyleData::operator==(const ParagraphStyleData& other) const
{
	if (this == &other)
		return true;

	return fAlignment == other.fAlignment
		&& fJustify == other.fJustify
		&& fFirstLineInset == other.fFirstLineInset
		&& fLineInset == other.fLineInset
		&& fLineSpacing == other.fLineSpacing
		&& fSpacingTop == other.fSpacingTop
		&& fSpacingBottom == other.fSpacingBottom
		&& fBullet == other.fBullet
		&& fKind == other.fKind
		&& StyleColoursData::operator==(other);
}


bool
ParagraphStyleData::operator!=(const ParagraphStyleData& other) const
{
	return !(*this == other);
}


ParagraphStyleDataRef
ParagraphStyleData::SetAlignment(::Alignment alignment)
{
	if (fAlignment == alignment)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fAlignment = alignment;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetJustify(bool justify)
{
	if (fJustify == justify)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fJustify = justify;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetFirstLineInset(float inset)
{
	if (fFirstLineInset == inset)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fFirstLineInset = inset;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetLineInset(float inset)
{
	if (fLineInset == inset)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fLineInset = inset;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetLineSpacing(float spacing)
{
	if (fLineSpacing == spacing)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fLineSpacing = spacing;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetSpacingTop(float spacing)
{
	if (fSpacingTop == spacing)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fSpacingTop = spacing;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetSpacingBottom(float spacing)
{
	if (fSpacingBottom == spacing)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fSpacingBottom = spacing;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetBullet(const ::Bullet& bullet)
{
	if (fBullet == bullet)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fBullet = bullet;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetKind(::ParagraphKind kind)
{
	if (fKind == kind)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->fKind = kind;
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetBackgroundColor(color_which which)
{
	return SetBackgroundColor(ui_color(which));
}

ParagraphStyleDataRef
ParagraphStyleData::SetBackgroundColor(rgb_color color)
{
	if (BackgroundColor() == color)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->StyleColoursData::SetBackgroundColor(color);
	return ParagraphStyleDataRef(ret, true);
}


ParagraphStyleDataRef
ParagraphStyleData::SetForegroundColor(color_which which)
{
	return SetForegroundColor(ui_color(which));
}

ParagraphStyleDataRef
ParagraphStyleData::SetForegroundColor(rgb_color color)
{
	if (ForegroundColor() == color)
		return ParagraphStyleDataRef(this);

	ParagraphStyleData* ret = new(std::nothrow) ParagraphStyleData(*this);
	if (ret == NULL)
		return ParagraphStyleDataRef(this);

	ret->StyleColoursData::SetForegroundColor(color);
	return ParagraphStyleDataRef(ret, true);
}

// #pragma mark - private


ParagraphStyleData&
ParagraphStyleData::operator=(const ParagraphStyleData& other)
{
	return *this;
}
