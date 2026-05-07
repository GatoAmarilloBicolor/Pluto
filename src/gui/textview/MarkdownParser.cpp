/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "MarkdownParser.h"
#include "md4c.h"

#include <new>
#include <stdio.h>
#include <string.h>


MarkdownParser::MarkdownParser()
	:
	fNormalStyle(),
	fBoldStyle(),
	fItalicStyle(),
	fBoldItalicStyle(),
	fCodeStyle(),
	fLinkStyle(),
	fStrikethroughStyle(),
	fHeading1Style(),
	fHeading2Style(),
	fHeading3Style(),
	fHeading4Style(),
	fHeading5Style(),
	fHeading6Style(),

	fParagraphStyle(),
	fHeading1ParagraphStyle(),
	fHeading2ParagraphStyle(),
	fHeading3ParagraphStyle(),
	fHeading4ParagraphStyle(),
	fHeading5ParagraphStyle(),
	fHeading6ParagraphStyle(),
	fCodeBlockStyle(),
	fBulletStyle(),
	fOrderedListStyle(),
	fBlockQuoteStyle(),
	fHRLineStyle(),
	fListLevel(0),
	fListItemNumber(0),
	fInCodeBlock(false),
	fInBlockQuote(0),
	fNeedsParagraphStart(true),
	fSuppressNewline(false),
	fCurrentImageUrl(),
	fCurrentImageAlt()
{
	_InitStyles();
}


MarkdownParser::MarkdownParser(const CharacterStyle& characterStyle,
	const ParagraphStyle& paragraphStyle)
	:
	fNormalStyle(characterStyle),
	fBoldStyle(),
	fItalicStyle(),
	fBoldItalicStyle(),
	fCodeStyle(),
	fLinkStyle(),
	fStrikethroughStyle(),
	fHeading1Style(),
	fHeading2Style(),
	fHeading3Style(),
	fHeading4Style(),
	fHeading5Style(),
	fHeading6Style(),

	fParagraphStyle(paragraphStyle),
	fHeading1ParagraphStyle(),
	fHeading2ParagraphStyle(),
	fHeading3ParagraphStyle(),
	fHeading4ParagraphStyle(),
	fHeading5ParagraphStyle(),
	fHeading6ParagraphStyle(),
	fCodeBlockStyle(),
	fBulletStyle(),
	fOrderedListStyle(),
	fBlockQuoteStyle(),
	fHRLineStyle(),
	fListLevel(0),
	fListItemNumber(0),
	fInCodeBlock(false),
	fInBlockQuote(0),
	fNeedsParagraphStart(true),
	fSuppressNewline(false),
	fCurrentImageUrl(),
	fCurrentImageAlt()
{
	// Ensure normal style has no formatting before deriving other styles
	fNormalStyle.SetBold(false);
	fNormalStyle.SetItalic(false);
	_InitStyles();
}


void
MarkdownParser::SetStyles(const CharacterStyle& characterStyle,
	const ParagraphStyle& paragraphStyle)
{
	fNormalStyle = characterStyle;
	fNormalStyle.SetBold(false);
	fNormalStyle.SetItalic(false);
	fParagraphStyle = paragraphStyle;
	_InitStyles();
}


void
MarkdownParser::UpdateColors(bool useSystem,
	rgb_color codeFg, rgb_color codeBg, rgb_color linkColor)
{
	if (useSystem) {
		fCodeStyle.SetForegroundColor(B_PANEL_TEXT_COLOR);
		fCodeStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
		fLinkStyle.SetForegroundColor(B_LINK_TEXT_COLOR);
		fCodeBlockStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
		fCodeBlockStyle.SetForegroundColor(B_PANEL_TEXT_COLOR);
		fBlockQuoteStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
		fHRLineStyle.SetForegroundColor(B_PANEL_BACKGROUND_COLOR);
	} else {
		fCodeStyle.SetForegroundColor(codeFg);
		fCodeStyle.SetBackgroundColor(codeBg);
		fLinkStyle.SetForegroundColor(linkColor);
		fCodeBlockStyle.SetBackgroundColor(codeBg);
		fCodeBlockStyle.SetForegroundColor(codeFg);
		fBlockQuoteStyle.SetBackgroundColor(codeBg);
		fHRLineStyle.SetForegroundColor(codeBg);
	}
}


TextDocumentRef
MarkdownParser::CreateDocumentFromMarkdown(const BString& text)
{
	TextDocumentRef document(new(std::nothrow) TextDocument(), true);
	if (!document.IsSet())
		return document;

	AppendMarkdown(document, text);

	return document;
}


void
MarkdownParser::AppendMarkdown(const TextDocumentRef& document, const BString& text)
{
	fTextDocument.SetTo(document);

	// Clear style stack
	while (!fStyleStack.empty())
		fStyleStack.pop();
	fStyleStack.push(fNormalStyle);

	fCurrentParagraph = Paragraph(fParagraphStyle);
	fListLevel = 0;
	fListItemNumber = 0;
	fInCodeBlock = false;
	fInBlockQuote = 0;
	fNeedsParagraphStart = true;
	fSuppressNewline = false;
	fCurrentLink = "";

	_ParseMarkdown(text);

	fTextDocument.Unset();
}


// #pragma mark - private


void
MarkdownParser::_InitStyles()
{
	// Ensure normal style has no bold/italic
	fNormalStyle.SetBold(false);
	fNormalStyle.SetItalic(false);

	// Bold style
	fBoldStyle = fNormalStyle;
	fBoldStyle.SetBold(true);

	// Italic style
	fItalicStyle = fNormalStyle;
	fItalicStyle.SetItalic(true);

	// Bold + Italic
	fBoldItalicStyle = fNormalStyle;
	fBoldItalicStyle.SetBold(true);
	fBoldItalicStyle.SetItalic(true);

	// Code style (monospace)
	fCodeStyle = fNormalStyle;
	fCodeStyle.SetFont(BFont(be_fixed_font));
	fCodeStyle.SetForegroundColor(B_PANEL_TEXT_COLOR);
	fCodeStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
	fCodeStyle.SetInsets(5.0f, 0.0f, 5.0f, 0.0f);

	// Link style (blue, underlined)
	fLinkStyle = fNormalStyle;
	fLinkStyle.SetForegroundColor(B_LINK_TEXT_COLOR);
	fLinkStyle.SetUnderline(1);  // Enable underline

	// Strikethrough
	fStrikethroughStyle = fNormalStyle;
	fStrikethroughStyle.SetStrikeOut(1);  // Enable strikethrough

	// Heading styles (larger fonts, bold)
	fHeading1Style = fNormalStyle;
	fHeading1Style.SetFontSize(fNormalStyle.FontSize() * 2.0);
	fHeading1Style.SetBold(true);

	fHeading2Style = fNormalStyle;
	fHeading2Style.SetFontSize(fNormalStyle.FontSize() * 1.5);
	fHeading2Style.SetBold(true);

	fHeading3Style = fNormalStyle;
	fHeading3Style.SetFontSize(fNormalStyle.FontSize() * 1.3);
	fHeading3Style.SetBold(true);

	fHeading4Style = fNormalStyle;
	fHeading4Style.SetFontSize(fNormalStyle.FontSize() * 1.2);
	fHeading4Style.SetBold(true);

	fHeading5Style = fNormalStyle;
	fHeading5Style.SetFontSize(fNormalStyle.FontSize() * 1.1);
	fHeading5Style.SetBold(true);

	fHeading6Style = fNormalStyle;
	fHeading6Style.SetFontSize(fNormalStyle.FontSize());
	fHeading6Style.SetBold(true);

	// Heading paragraph styles (extra spacing)
	fHeading1ParagraphStyle = fParagraphStyle;
	fHeading1ParagraphStyle.SetSpacingTop(12.0f);
	fHeading1ParagraphStyle.SetSpacingBottom(8.0f);
	fHeading1ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_1);

	fHeading2ParagraphStyle = fParagraphStyle;
	fHeading2ParagraphStyle.SetSpacingTop(10.0f);
	fHeading2ParagraphStyle.SetSpacingBottom(6.0f);
	fHeading2ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_2);

	fHeading3ParagraphStyle = fParagraphStyle;
	fHeading3ParagraphStyle.SetSpacingTop(8.0f);
	fHeading3ParagraphStyle.SetSpacingBottom(5.0f);
	fHeading3ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_3);

	fHeading4ParagraphStyle = fParagraphStyle;
	fHeading4ParagraphStyle.SetSpacingTop(6.0f);
	fHeading4ParagraphStyle.SetSpacingBottom(4.0f);
	fHeading4ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_4);

	fHeading5ParagraphStyle = fParagraphStyle;
	fHeading5ParagraphStyle.SetSpacingTop(5.0f);
	fHeading5ParagraphStyle.SetSpacingBottom(3.0f);
	fHeading5ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_5);

	fHeading6ParagraphStyle = fParagraphStyle;
	fHeading6ParagraphStyle.SetSpacingTop(4.0f);
	fHeading6ParagraphStyle.SetSpacingBottom(2.0f);
	fHeading6ParagraphStyle.SetKind(PARAGRAPH_KIND_HEADING_6);

	// Code block style
	fCodeBlockStyle = fParagraphStyle;
	fCodeBlockStyle.SetLineInset(20.0f);
	fCodeBlockStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
	fCodeBlockStyle.SetForegroundColor(B_PANEL_TEXT_COLOR);
	fCodeBlockStyle.SetKind(PARAGRAPH_KIND_CODE_BLOCK);

	// Bullet list style
	fBulletStyle = fParagraphStyle;
	fBulletStyle.SetBullet(Bullet("•", 10.0f));
	fBulletStyle.SetLineInset(20.0f);
	fBulletStyle.SetKind(PARAGRAPH_KIND_LIST_BULLET);

	// Ordered list style
	fOrderedListStyle = fParagraphStyle;
	fOrderedListStyle.SetLineInset(20.0f);
	fOrderedListStyle.SetKind(PARAGRAPH_KIND_LIST_ORDERED);

	// Block quote style
	fBlockQuoteStyle = fParagraphStyle;
	fBlockQuoteStyle.SetLineInset(30.0f);
	fBlockQuoteStyle.SetBackgroundColor(B_PANEL_BACKGROUND_COLOR);
	fBlockQuoteStyle.SetKind(PARAGRAPH_KIND_BLOCK_QUOTE);

	fHRLineStyle = fParagraphStyle;
	fHRLineStyle.SetForegroundColor(B_PANEL_BACKGROUND_COLOR);
	fHRLineStyle.SetKind(PARAGRAPH_KIND_HORIZONTAL_RULE);
}


void
MarkdownParser::_ParseMarkdown(const BString& text)
{
	MD_PARSER parser = {
		0,  // abi_version
		MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS |
			MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_UNDERLINE,  // flags (GitHub extensions)
		&MarkdownParser::_EnterBlockCallback,
		&MarkdownParser::_LeaveBlockCallback,
		&MarkdownParser::_EnterSpanCallback,
		&MarkdownParser::_LeaveSpanCallback,
		&MarkdownParser::_TextCallback,
		NULL,  // debug_log
		NULL   // syntax
	};

	md_parse(text.String(), text.Length(), &parser, this);

	// Finish any remaining paragraph
	if (!fNeedsParagraphStart && fCurrentParagraph.CountTextSpans() > 0)
		_FinishParagraph();
}


// Static callback wrappers
int
MarkdownParser::_EnterBlockCallback(MD_BLOCKTYPE blockType, void* detail, void* userdata)
{
	MarkdownParser* parser = (MarkdownParser*)userdata;
	parser->_EnterBlock(blockType, detail);
	return 0;
}


int
MarkdownParser::_LeaveBlockCallback(MD_BLOCKTYPE blockType, void* detail, void* userdata)
{
	MarkdownParser* parser = (MarkdownParser*)userdata;
	parser->_LeaveBlock(blockType, detail);
	return 0;
}


int
MarkdownParser::_EnterSpanCallback(MD_SPANTYPE spanType, void* detail, void* userdata)
{
	MarkdownParser* parser = (MarkdownParser*)userdata;
	parser->_EnterSpan(spanType, detail);
	return 0;
}


int
MarkdownParser::_LeaveSpanCallback(MD_SPANTYPE spanType, void* detail, void* userdata)
{
	MarkdownParser* parser = (MarkdownParser*)userdata;
	parser->_LeaveSpan(spanType, detail);
	return 0;
}


int
MarkdownParser::_TextCallback(MD_TEXTTYPE textType, const MD_CHAR* text,
	MD_SIZE size, void* userdata)
{
	MarkdownParser* parser = (MarkdownParser*)userdata;
	parser->_ProcessText(textType, text, size);
	return 0;
}


void
MarkdownParser::_EnterBlock(MD_BLOCKTYPE blockType, void* detail)
{
	switch (blockType) {
		case MD_BLOCK_H:
		{
			MD_BLOCK_H_DETAIL* heading = (MD_BLOCK_H_DETAIL*)detail;
			_StartNewParagraph();

			switch (heading->level) {
				case 1:
					fCurrentParagraph.SetStyle(fHeading1ParagraphStyle);
					_PushCharacterStyle(fHeading1Style);
					break;
				case 2:
					fCurrentParagraph.SetStyle(fHeading2ParagraphStyle);
					_PushCharacterStyle(fHeading2Style);
					break;
				case 3:
					fCurrentParagraph.SetStyle(fHeading3ParagraphStyle);
					_PushCharacterStyle(fHeading3Style);
					break;
				case 4:
					fCurrentParagraph.SetStyle(fHeading4ParagraphStyle);
					_PushCharacterStyle(fHeading4Style);
					break;
				case 5:
					fCurrentParagraph.SetStyle(fHeading5ParagraphStyle);
					_PushCharacterStyle(fHeading5Style);
					break;
				case 6:
					fCurrentParagraph.SetStyle(fHeading6ParagraphStyle);
					_PushCharacterStyle(fHeading6Style);
					break;
			}
			break;
		}

		case MD_BLOCK_CODE:
		{
			_StartNewParagraph();
			fCurrentParagraph.SetStyle(fCodeBlockStyle);
			_PushCharacterStyle(fCodeStyle);
			fInCodeBlock = true;
			break;
		}

		case MD_BLOCK_P:
		{
			_StartNewParagraph();
			if (fInBlockQuote > 0) {
				ParagraphStyle quoteStyle = fBlockQuoteStyle;
				float indent = 30.0f * fInBlockQuote;
				quoteStyle.SetLineInset(indent);
				quoteStyle.SetKind(PARAGRAPH_KIND_BLOCK_QUOTE);
				fCurrentParagraph.SetStyle(quoteStyle);
			}
			break;
		}

		case MD_BLOCK_UL:
		{
			fListLevel++;
			break;
		}

		case MD_BLOCK_OL:
		{
			MD_BLOCK_OL_DETAIL* ol = (MD_BLOCK_OL_DETAIL*)detail;
			fListLevel++;
			fListItemNumber = ol->start;
			break;
		}

		case MD_BLOCK_LI:
		{
			_StartNewParagraph();

			//Indentation position
			float indent = 20.0f * fListLevel;

			// Check if we're in ordered or unordered list
			// For simplicity, use bullet style and set custom bullet for ordered lists
			if (fListItemNumber > 0) {
				char bullet[32];
				snprintf(bullet, sizeof(bullet), "%d.", fListItemNumber);
				ParagraphStyle orderedStyle = fOrderedListStyle;
				orderedStyle.SetBullet(Bullet(bullet, 20.0f));
				orderedStyle.SetLineInset(indent);
				orderedStyle.SetKind(PARAGRAPH_KIND_LIST_ORDERED);
				fCurrentParagraph.SetStyle(orderedStyle);
				fListItemNumber++;
			} else {
				ParagraphStyle bulletSyle = fBulletStyle;
				bulletSyle.SetBullet(Bullet("•", 20.0f));
				bulletSyle.SetLineInset(indent);
				bulletSyle.SetKind(PARAGRAPH_KIND_LIST_BULLET);
				fCurrentParagraph.SetStyle(bulletSyle);
			}
			break;
		}

		case MD_BLOCK_QUOTE:
		{
			fInBlockQuote++;
			break;
		}

		case MD_BLOCK_HR:
		{
			_StartNewParagraph();
			fCurrentParagraph.SetStyle(fHRLineStyle);
			_AppendText(" ");
			_FinishParagraph();
			break;
		}
	}
}


void
MarkdownParser::_LeaveBlock(MD_BLOCKTYPE blockType, void* detail)
{
	switch (blockType) {
		case MD_BLOCK_H:
		case MD_BLOCK_CODE:
			_PopCharacterStyle();
			_FinishParagraph();
			if (blockType == MD_BLOCK_CODE)
				fInCodeBlock = false;
			break;
		case MD_BLOCK_LI:
			//ok!
			break;
		case MD_BLOCK_P:
			_FinishParagraph();
			break;
		case MD_BLOCK_QUOTE:
			if (fInBlockQuote > 0)
				fInBlockQuote--;
			break;

		case MD_BLOCK_UL:
			fListLevel--;
			break;

		case MD_BLOCK_OL:
			fListLevel--;
			if (fListLevel == 0)
				fListItemNumber = 0;
			break;
	}
}


void
MarkdownParser::_EnterSpan(MD_SPANTYPE spanType, void* detail)
{
	switch (spanType) {
		case MD_SPAN_EM:
			_PushCharacterStyle(fItalicStyle);
			break;

		case MD_SPAN_STRONG:
		{
			// Check if we're already in italic mode
			if (!fStyleStack.empty() && fStyleStack.top() == fItalicStyle)
				_PushCharacterStyle(fBoldItalicStyle);
			else
				_PushCharacterStyle(fBoldStyle);
			break;
		}

		case MD_SPAN_CODE:
			_PushCharacterStyle(fCodeStyle);
			break;

		case MD_SPAN_A:
		{
			MD_SPAN_A_DETAIL* link = (MD_SPAN_A_DETAIL*)detail;
			fCurrentLink.SetTo(link->href.text, link->href.size);
			_PushCharacterStyle(fLinkStyle);
			break;
		}

		case MD_SPAN_IMG:
		{
			MD_SPAN_IMG_DETAIL* img = (MD_SPAN_IMG_DETAIL*)detail;
			fCurrentImageUrl.SetTo(img->src.text, img->src.size);
			// Alt text will be captured via _ProcessText
			fCurrentImageAlt = "";
			break;
		}

		case MD_SPAN_DEL:
			_PushCharacterStyle(fStrikethroughStyle);
			break;
		case MD_SPAN_LATEXMATH:
		case MD_SPAN_LATEXMATH_DISPLAY:
		case MD_SPAN_WIKILINK:
		case MD_SPAN_U:
			//unsupported tags
			break;
	}
}


void
MarkdownParser::_LeaveSpan(MD_SPANTYPE spanType, void* detail)
{
	switch (spanType) {
		case MD_SPAN_EM:
		case MD_SPAN_STRONG:
			_PopCharacterStyle();
			break;

		case MD_SPAN_IMG:
		{
			// Load image and create span
			if (fCurrentImageUrl.Length() > 0) {
				BBitmap* bitmap = fTextDocument->LoadImage(fCurrentImageUrl);
				if (bitmap != NULL) {
					TextSpan imageSpan;
					imageSpan.SetImage(bitmap, fCurrentImageAlt);
					fCurrentParagraph.Append(imageSpan);
				} else  {
					if (fCurrentImageAlt.IsEmpty())
						fCurrentImageAlt = fCurrentImageUrl;
					TextSpan altSpan(BString("[").Append(fCurrentImageAlt).Append("]"), fItalicStyle);
					fCurrentParagraph.Append(altSpan);
				}

				fprintf(stderr, "[MarkdownParser] Image load %s!\n", fCurrentImageUrl.String());
				fflush(stderr);
			}
			fCurrentImageUrl = "";
			fCurrentImageAlt = "";
			break;
		}
		case MD_SPAN_CODE:
		case MD_SPAN_DEL:
			_PopCharacterStyle();
			break;

		case MD_SPAN_A:
			_PopCharacterStyle();
			fCurrentLink = "";
			break;
		case MD_SPAN_LATEXMATH:
		case MD_SPAN_LATEXMATH_DISPLAY:
		case MD_SPAN_WIKILINK:
		case MD_SPAN_U:
			//unsupported tags
			break;
	}
}


void
MarkdownParser::_ProcessText(MD_TEXTTYPE textType, const MD_CHAR* text, MD_SIZE size)
{
	BString textStr(text, size);

	// If we're processing image alt text, capture it
	if (fCurrentImageUrl.Length() > 0) {
		fCurrentImageAlt << textStr;
		return;  // Don't append to paragraph
	}

	switch (textType) {
		case MD_TEXT_NORMAL:
		case MD_TEXT_ENTITY:
			printf("TEXT: '%s' (inBlockQuote=%d)\n", textStr.String(), fInBlockQuote);
			_AppendText(textStr);
			break;

		case MD_TEXT_CODE:
			_AppendText(textStr);
			break;

		case MD_TEXT_SOFTBR:
			if (!fInCodeBlock)
				_AppendText(" ");
			else
				_AppendText("\n");
			break;

		case MD_TEXT_BR:
		case MD_TEXT_NULLCHAR:
			_AppendText("\n");
			break;
	}
}


void
MarkdownParser::_PushCharacterStyle(const CharacterStyle& style)
{
	fStyleStack.push(style);
}


void
MarkdownParser::_PopCharacterStyle()
{
	if (fStyleStack.size() > 1)
		fStyleStack.pop();
}


const CharacterStyle&
MarkdownParser::_CurrentCharacterStyle() const
{
	return fStyleStack.top();
}


void
MarkdownParser::_AppendText(const BString& text)
{
	if (text.Length() == 0)
		return;

	TextSpan span(text, _CurrentCharacterStyle());

	// Add link handler if we're in a link
	if (fCurrentLink.Length() > 0) {
		BMessage* clickMessage = new BMessage('link');
		clickMessage->AddString("url", fCurrentLink);
		span.SetClickMessage(clickMessage);
		// Could also set custom cursor here
	}

	fCurrentParagraph.Append(span);
	fNeedsParagraphStart = false;
}


void
MarkdownParser::_FinishParagraph()
{
	if (fCurrentParagraph.CountTextSpans() > 0 && fTextDocument.IsSet()) {
		printf("FINISH_PARA: %d spans, inBlockQuote=%d, first span='%s'\n",
               fCurrentParagraph.CountTextSpans(), fInBlockQuote,
               fCurrentParagraph.TextSpanAtIndex(0).Text().String());
		fTextDocument->Append(fCurrentParagraph);
	}
	fNeedsParagraphStart = true;
}


void
MarkdownParser::_StartNewParagraph()
{
	if (!fNeedsParagraphStart)
		_FinishParagraph();

	fCurrentParagraph = Paragraph(fParagraphStyle);
	fNeedsParagraphStart = false;
}
