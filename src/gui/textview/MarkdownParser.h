/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include "TextDocument.h"
#include "ImageCache.h"
#include "md4c.h"
#include <stack>


class MarkdownParser {
public:
								MarkdownParser();
								MarkdownParser(
									const CharacterStyle& characterStyle,
									const ParagraphStyle& paragraphStyle);

			void				SetStyles(
									const CharacterStyle& characterStyle,
									const ParagraphStyle& paragraphStyle);

			// Update colors for forced dark/light mode.
			// When scheme is "auto", pass useSystem=true to use system color_which.
			// When scheme is forced, pass explicit colors.
			void				UpdateColors(bool useSystem,
									rgb_color codeFg, rgb_color codeBg,
									rgb_color linkColor);

			TextDocumentRef		CreateDocumentFromMarkdown(const BString& text);
			void				AppendMarkdown(const TextDocumentRef& document,
									const BString& text);

private:
			void				_InitStyles();
			void				_ParseMarkdown(const BString& text);

			// md4c callback handlers
	static int			_EnterBlockCallback(MD_BLOCKTYPE blockType, void* detail,
								void* userdata);
	static int			_LeaveBlockCallback(MD_BLOCKTYPE blockType, void* detail,
								void* userdata);
	static int			_EnterSpanCallback(MD_SPANTYPE spanType, void* detail,
								void* userdata);
	static int			_LeaveSpanCallback(MD_SPANTYPE spanType, void* detail,
								void* userdata);
	static int			_TextCallback(MD_TEXTTYPE textType, const MD_CHAR* text,
								MD_SIZE size, void* userdata);

	// Block handlers
	void				_EnterBlock(MD_BLOCKTYPE blockType, void* detail);
	void				_LeaveBlock(MD_BLOCKTYPE blockType, void* detail);
	void				_EnterSpan(MD_SPANTYPE spanType, void* detail);
	void				_LeaveSpan(MD_SPANTYPE spanType, void* detail);
	void				_ProcessText(MD_TEXTTYPE textType, const MD_CHAR* text,
								MD_SIZE size);
			// Style management
			void				_PushCharacterStyle(const CharacterStyle& style);
			void				_PopCharacterStyle();
			const CharacterStyle& _CurrentCharacterStyle() const;

			// Document building
			void				_AppendText(const BString& text);
			void				_FinishParagraph();
			void				_StartNewParagraph();

private:
			// Base styles
			CharacterStyle		fNormalStyle;
			CharacterStyle		fBoldStyle;
			CharacterStyle		fItalicStyle;
			CharacterStyle		fBoldItalicStyle;
			CharacterStyle		fCodeStyle;
			CharacterStyle		fLinkStyle;
			CharacterStyle		fStrikethroughStyle;
			CharacterStyle		fHeading1Style;
			CharacterStyle		fHeading2Style;
			CharacterStyle		fHeading3Style;
			CharacterStyle		fHeading4Style;
			CharacterStyle		fHeading5Style;
			CharacterStyle		fHeading6Style;

			ParagraphStyle		fParagraphStyle;
			ParagraphStyle		fHeading1ParagraphStyle;
			ParagraphStyle		fHeading2ParagraphStyle;
			ParagraphStyle		fHeading3ParagraphStyle;
			ParagraphStyle		fHeading4ParagraphStyle;
			ParagraphStyle		fHeading5ParagraphStyle;
			ParagraphStyle		fHeading6ParagraphStyle;
			ParagraphStyle		fCodeBlockStyle;
			ParagraphStyle		fBulletStyle;
			ParagraphStyle		fOrderedListStyle;
			ParagraphStyle		fBlockQuoteStyle;
			ParagraphStyle		fHRLineStyle;

			// Parsing state
			std::stack<CharacterStyle> fStyleStack;
			TextDocumentRef		fTextDocument;
			Paragraph			fCurrentParagraph;
			int32				fListLevel;
			int32				fListItemNumber;
			bool				fInCodeBlock;
			int32				fInBlockQuote;
			bool				fNeedsParagraphStart;
			bool				fSuppressNewline;
			BString				fCurrentLink;

			// Image support
			BString				fCurrentImageUrl;
			BString				fCurrentImageAlt;
};


#endif // MARKDOWN_PARSER_H
