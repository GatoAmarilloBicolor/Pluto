/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "StyleColoursData.h"


StyleColoursData::StyleColoursData()
	:
	fHasFgColor(false),
	fHashBgColor(false),
	fFgColor(),
	fBgColor()
{
}


StyleColoursData::StyleColoursData(const StyleColoursData& other)
	:
	fHasFgColor(other.fHasFgColor),
	fHashBgColor(other.fHashBgColor),
	fFgColor(other.fFgColor),
	fBgColor(other.fBgColor)
{
}


bool
StyleColoursData::operator==(const StyleColoursData& other) const
{
	if (this == &other)
		return true;

	return fHasFgColor == other.fHasFgColor
		&& fHashBgColor == other.fHashBgColor
		&& fFgColor == other.fFgColor
		&& fBgColor == other.fBgColor;
}


bool
StyleColoursData::operator!=(const StyleColoursData& other) const
{
	return !(*this == other);
}


StyleColoursData&
StyleColoursData::operator=(const StyleColoursData& other)
{
	if (this == &other)
		return *this;

	fHasFgColor = other.fHasFgColor;
	fHashBgColor = other.fHashBgColor;
	fFgColor = other.fFgColor;
	fBgColor = other.fBgColor;

	return *this;
}


void
StyleColoursData::SetForegroundColor(color_which which)
{
	SetForegroundColor (ui_color(which));
}


void
StyleColoursData::SetForegroundColor(rgb_color color)
{
	fHasFgColor = true;
	fFgColor = color;
}


void
StyleColoursData::SetBackgroundColor(color_which which)
{
	SetBackgroundColor(ui_color(which));
}


void
StyleColoursData::SetBackgroundColor(rgb_color color)
{
	fHashBgColor = true;
	fBgColor = color;
}
