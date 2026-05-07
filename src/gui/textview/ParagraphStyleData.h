/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef PARAGRAPH_STYLE_DATA_H
#define PARAGRAPH_STYLE_DATA_H

#include "Bullet.h"
#include "InterfaceDefs.h"
#include "StyleColoursData.h"


enum Alignment {
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2,
};


enum ParagraphKind {
	PARAGRAPH_KIND_PLAIN = 0,
	PARAGRAPH_KIND_HEADING_1,
	PARAGRAPH_KIND_HEADING_2,
	PARAGRAPH_KIND_HEADING_3,
	PARAGRAPH_KIND_HEADING_4,
	PARAGRAPH_KIND_HEADING_5,
	PARAGRAPH_KIND_HEADING_6,
	PARAGRAPH_KIND_CODE_BLOCK,
	PARAGRAPH_KIND_BLOCK_QUOTE,
	PARAGRAPH_KIND_LIST_BULLET,
	PARAGRAPH_KIND_LIST_ORDERED,
	PARAGRAPH_KIND_HORIZONTAL_RULE,
	PARAGRAPH_KIND_TABLE_CELL,
	PARAGRAPH_KIND_CUSTOM
};


class ParagraphStyleData;
typedef BReference<ParagraphStyleData> ParagraphStyleDataRef;


// You cannot modify a ParagraphStyleData object once it has been
// created.
class ParagraphStyleData : public BReferenceable, public StyleColoursData {
public:
	ParagraphStyleData();
	ParagraphStyleData(const ParagraphStyleData& other);

	bool operator==(const ParagraphStyleData& other) const;
	bool operator!=(const ParagraphStyleData& other) const;

	ParagraphStyleDataRef SetAlignment(::Alignment alignment);
	inline ::Alignment Alignment() const { return fAlignment; }

	ParagraphStyleDataRef SetJustify(bool justify);
	inline bool Justify() const { return fJustify; }

	ParagraphStyleDataRef SetFirstLineInset(float inset);
	inline float FirstLineInset() const { return fFirstLineInset; }

	ParagraphStyleDataRef SetLineInset(float inset);
	inline float LineInset() const { return fLineInset; }

	ParagraphStyleDataRef SetLineSpacing(float spacing);
	inline float LineSpacing() const { return fLineSpacing; }

	ParagraphStyleDataRef SetSpacingTop(float spacing);
	inline float SpacingTop() const { return fSpacingTop; }

	ParagraphStyleDataRef SetSpacingBottom(float spacing);
	inline float SpacingBottom() const { return fSpacingBottom; }

	ParagraphStyleDataRef SetBullet(const ::Bullet& bullet);
	inline const ::Bullet& Bullet() const { return fBullet; }

	ParagraphStyleDataRef SetKind(::ParagraphKind kind);
	inline ::ParagraphKind Kind() const { return fKind; }

	ParagraphStyleDataRef SetBackgroundColor(color_which which);
	ParagraphStyleDataRef SetBackgroundColor(rgb_color color);
	ParagraphStyleDataRef SetForegroundColor(color_which which);
	ParagraphStyleDataRef SetForegroundColor(rgb_color color);

private:
	ParagraphStyleData& operator=(const ParagraphStyleData& other);

private:
	::Alignment fAlignment;
	bool fJustify;

	float fFirstLineInset;
	float fLineInset;
	float fLineSpacing;

	float fSpacingTop;
	float fSpacingBottom;

	::Bullet fBullet;
	::ParagraphKind fKind;
};


#endif // PARAGRAPH_STYLE_DATA_H
