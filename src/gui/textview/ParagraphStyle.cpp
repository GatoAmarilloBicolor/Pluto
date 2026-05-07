/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "ParagraphStyle.h"


ParagraphStyle::ParagraphStyle()
	:
	fStyleData(new ParagraphStyleData(), true)
{
}


ParagraphStyle::ParagraphStyle(const ParagraphStyle& other)
	:
	fStyleData(other.fStyleData)
{
}


ParagraphStyle&
ParagraphStyle::operator=(const ParagraphStyle& other)
{
	if (this == &other)
		return *this;

	fStyleData = other.fStyleData;
	return *this;
}


bool
ParagraphStyle::operator==(const ParagraphStyle& other) const
{
	if (this == &other)
		return true;

	if (fStyleData == other.fStyleData)
		return true;

	if (fStyleData.IsSet() && other.fStyleData.IsSet())
		return *fStyleData == *other.fStyleData;

	return false;
}


bool
ParagraphStyle::operator!=(const ParagraphStyle& other) const
{
	return !(*this == other);
}


bool
ParagraphStyle::SetAlignment(::Alignment alignment)
{
	ParagraphStyleDataRef data = fStyleData->SetAlignment(alignment);
	if (data == fStyleData)
		return data->Alignment() == alignment;

	fStyleData = data;
	return true;
}


::Alignment
ParagraphStyle::Alignment() const
{
	return fStyleData->Alignment();
}


bool
ParagraphStyle::SetJustify(bool justify)
{
	ParagraphStyleDataRef data = fStyleData->SetJustify(justify);
	if (data == fStyleData)
		return data->Justify() == justify;

	fStyleData = data;
	return true;
}


bool
ParagraphStyle::Justify() const
{
	return fStyleData->Justify();
}


bool
ParagraphStyle::SetFirstLineInset(float inset)
{
	ParagraphStyleDataRef data = fStyleData->SetFirstLineInset(inset);
	if (data == fStyleData)
		return data->FirstLineInset() == inset;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::FirstLineInset() const
{
	return fStyleData->FirstLineInset();
}


bool
ParagraphStyle::SetLineInset(float inset)
{
	ParagraphStyleDataRef data = fStyleData->SetLineInset(inset);
	if (data == fStyleData)
		return data->LineInset() == inset;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::LineInset() const
{
	return fStyleData->LineInset();
}


bool
ParagraphStyle::SetLineSpacing(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetLineSpacing(spacing);
	if (data == fStyleData)
		return data->LineSpacing() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::LineSpacing() const
{
	return fStyleData->LineSpacing();
}


bool
ParagraphStyle::SetSpacingTop(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetSpacingTop(spacing);
	if (data == fStyleData)
		return data->SpacingTop() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::SpacingTop() const
{
	return fStyleData->SpacingTop();
}


bool
ParagraphStyle::SetSpacingBottom(float spacing)
{
	ParagraphStyleDataRef data = fStyleData->SetSpacingBottom(spacing);
	if (data == fStyleData)
		return data->SpacingBottom() == spacing;

	fStyleData = data;
	return true;
}


float
ParagraphStyle::SpacingBottom() const
{
	return fStyleData->SpacingBottom();
}


bool
ParagraphStyle::SetBullet(const ::Bullet& bullet)
{
	ParagraphStyleDataRef data = fStyleData->SetBullet(bullet);
	if (data == fStyleData)
		return data->Bullet() == bullet;

	fStyleData = data;
	return true;
}


const ::Bullet&
ParagraphStyle::Bullet() const
{
	return fStyleData->Bullet();
}


bool
ParagraphStyle::SetKind(::ParagraphKind kind)
{
	ParagraphStyleDataRef data = fStyleData->SetKind(kind);
	if (data == fStyleData)
		return data->Kind() == kind;

	fStyleData = data;
	return true;
}


::ParagraphKind
ParagraphStyle::Kind() const
{
	return fStyleData->Kind();
}


bool
ParagraphStyle::SetBackgroundColor(color_which which)
{
	ParagraphStyleDataRef data = fStyleData->SetBackgroundColor(which);
	if (data == fStyleData)
		return data->BackgroundColor() == ui_color(which);

	fStyleData = data;
	return true;
}


bool
ParagraphStyle::SetBackgroundColor(rgb_color color)
{
	ParagraphStyleDataRef data = fStyleData->SetBackgroundColor(color);
	if (data == fStyleData)
		return data->BackgroundColor() == color;

	fStyleData = data;
	return true;
}

bool
ParagraphStyle::HasBackgroundColor() const
{
	return fStyleData->HasBackgroundColor();
}


rgb_color
ParagraphStyle::BackgroundColor() const
{
	return fStyleData->BackgroundColor();
}

bool
ParagraphStyle::SetForegroundColor(color_which which)
{
	ParagraphStyleDataRef data = fStyleData->SetForegroundColor(which);
	if (data == fStyleData)
		return data->ForegroundColor() == ui_color(which);

	fStyleData = data;
	return true;
}


bool
ParagraphStyle::SetForegroundColor(rgb_color color)
{
	ParagraphStyleDataRef data = fStyleData->SetForegroundColor(color);
	if (data == fStyleData)
		return data->ForegroundColor() == color;

	fStyleData = data;
	return true;
}

rgb_color
ParagraphStyle::ForegroundColor() const
{
	return fStyleData->ForegroundColor();
}


bool
ParagraphStyle::HasForegroundColor() const
{
	return fStyleData->HasForegroundColor();
}


