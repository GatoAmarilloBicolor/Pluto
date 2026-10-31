#include "ConversationView.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <Directory.h>
#include <File.h>
#include <LayoutBuilder.h>
#include <DataIO.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <ScrollView.h>
#include <String.h>
#include <TextView.h>
#include <TranslatorRoster.h>

#include "../base64.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "ChatColors.h"
#include "SyntaxHighlight.h"
#include "../agent/Loop.h"
#include "../cancel.h"
#include "../storage/SessionStore.h"
#include "../telemetry.h"

namespace pluto {
namespace gui {

// ---- constructor / destructor ----

ConversationView::ConversationView(std::unique_ptr<Provider> provider,
                                   const Config& cfg,
                                   ToolRegistry* tools,
                                   ConversationDelegate* delegate)
	: BView("conversation", B_WILL_DRAW | B_FRAME_EVENTS),
	  fTabConfig(cfg),
	  fOwnedProvider(std::move(provider)),
	  fProvider(fOwnedProvider.get()),
	  fConfig(&fTabConfig),
	  fTools(tools), fDelegate(delegate)
{
	// Create the markdown-native document view.
	fDocView = new TextDocumentView("output");
	fDocView->SetEditingEnabled(false);
	fDocView->SetInsets(10, 10, 10, 10);
	fTextDoc.SetTo(new TextDocument(), true);
	fDocView->SetTextDocument(fTextDoc);

	// Set background color from config color scheme.
	// Note: cfg.color_scheme may be "dark", "light", or "auto".
	if (cfg.color_scheme == "dark") {
		rgb_color bg = { 30, 30, 30, 255 };
		fDocView->SetViewUIColor(B_NO_COLOR);
		fDocView->SetViewColor(bg);
		fDocView->SetLowColor(bg);
	} else if (cfg.color_scheme == "light") {
		rgb_color bg = { 255, 255, 255, 255 };
		fDocView->SetViewUIColor(B_NO_COLOR);
		fDocView->SetViewColor(bg);
		fDocView->SetLowColor(bg);
	} else {
		fDocView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		fDocView->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	}

	// Keep legacy BTextView for compatibility (hidden, used for text queries).
	// Legacy BTextView removed — rendering uses TextDocumentView (fDocView).

	fOutputScroll = new BScrollView("outputScroll", fDocView,
	                                 0, false, true, B_FANCY_BORDER);

	// Layout: output scroll fills the view.
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fOutputScroll)
		.End();

	// Wire streaming markdown callbacks to the real rendering methods.
	fStreamingMd.on_line = [this](const std::string& line) {
		_RenderAssistantLine(line);
	};
	fStreamingMd.on_code_block = [this](const std::string& code,
	                                     const std::string& lang) {
		_FlushMdCodeBlockImpl(code, lang);
	};
	fStreamingMd.on_partial = [this](const std::string& partial) {
		// With TextDocumentView, partial lines are rendered directly.
		if (!partial.empty())
			_Append(partial, kColAsstBody);
	};
}

void ConversationView::AttachedToWindow() {
	BView::AttachedToWindow();
	// Create cursor runner now that we have a Looper.
	if (!fCursorRunner) {
		BMessage tick(kMsgCursorTick);
		fCursorRunner = new BMessageRunner(BMessenger(this), &tick, 500000);
	}
	// Set this view as the invoker target for link clicks in TextDocumentView.
	if (fDocView)
		fDocView->SetTarget(this);
}

void ConversationView::FrameResized(float w, float h) {
	BView::FrameResized(w, h);
	_RepositionInlineImages();
}

ConversationView::~ConversationView() {
	// Signal worker to stop and wait for it to finish before
	// invalidating any pointers it holds (provider, config, tools).
	fCancelFlag.store(true, std::memory_order_release);
	if (fWorker >= 0) {
		status_t dummy;
		wait_for_thread(fWorker, &dummy);
	}
	delete fPreviewBitmap;
	delete fCursorRunner;
}

// ---- low-level text append ----

void ConversationView::_Append(const std::string& text, rgb_color color,
                               uint16 face, bool mono) {
	if (text.empty()) return;

	BFont font = mono ? *be_fixed_font : *be_plain_font;
	if (face != 0) font.SetFace(face);
	if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);

	CharacterStyle style;
	style.SetFont(font);
	style.SetForegroundColor(color);

	// Process text: accumulate spans in fCurrentPara.
	// On '\n', flush fCurrentPara to the document and start a new one.
	size_t pos = 0;
	while (pos < text.size()) {
		size_t nl = text.find('\n', pos);
		if (nl == std::string::npos) {
			// No more newlines — add remaining text to current paragraph.
			BString chunk(text.c_str() + pos, text.size() - pos);
			fCurrentPara.Append(TextSpan(chunk, style));
			break;
		}
		// Text before the newline — add to current paragraph.
		if (nl > pos) {
			BString chunk(text.c_str() + pos, nl - pos);
			fCurrentPara.Append(TextSpan(chunk, style));
		}
		// Flush current paragraph to document.
		if (fCurrentPara.IsEmpty()) {
			// Empty paragraph — add a space so it takes up a line.
			CharacterStyle dimStyle;
			dimStyle.SetFont(*be_plain_font);
			fCurrentPara.Append(TextSpan(" ", dimStyle));
		}
		fTextDoc->Append(fCurrentPara);
		// Start a new paragraph with zero spacing.
		ParagraphStyle pstyle;
		pstyle.SetSpacingTop(0);
		pstyle.SetSpacingBottom(0);
		fCurrentPara = Paragraph(pstyle);
		pos = nl + 1;
	}

	// Update the view.
	fDocView->SetTextDocument(fTextDoc);
	fDocView->Relayout();

	// Scroll to bottom.
	if (fOutputScroll) {
		BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
		if (sb) {
			float min, max;
			sb->GetRange(&min, &max);
			sb->SetValue(max);
		}
	}
}

// ---- header emission ----

void ConversationView::_EmitUserHeader() {
	// Spacer between turns (skip on first message after banner).
	if (fTextDoc.IsSet() && fTextDoc->CountParagraphs() > 2) {
		// Thin dotted separator — lighter than the old solid bar.
		CharacterStyle hrStyle;
		rgb_color sepColor = ui_color(B_DOCUMENT_TEXT_COLOR);
		sepColor.alpha = 100;  // semi-transparent
		hrStyle.SetForegroundColor(sepColor);
		BFont hrFont = *be_plain_font;
		hrFont.SetSize(4);
		hrStyle.SetFont(hrFont);
		_Append("\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 "
		        "\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 "
		        "\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 "
		        "\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7\n",
		        ui_color(B_DOCUMENT_TEXT_COLOR), 0, false);
		_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR), 0, false);
	}
	_Append("\xF0\x9F\x91\xA4 You\n", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
}

void ConversationView::_EmitAssistantHeader() {
	fAsstHeaderOff = 0;  // legacy offset, unused by paragraph-based ToolBlockManager

	// Breathing space before assistant response.
	_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR), 0, false);

	fAsstHeaderPara = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

	// Pre-allocate a ToolBlock so we know its index for the click message.
	ToolBlock block;
	block.collapsed = false;
	block.is_error = false;
	block.header_para = fAsstHeaderPara;
	fPendingAsstBlock = fToolBlocks.add(block);

	// Create clickable arrow span for collapse/expand.
	{
		BFont font = *be_plain_font;
		font.SetFace(B_BOLD_FACE);
		if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
		CharacterStyle style;
		style.SetFont(font);
		style.SetForegroundColor(ui_color(B_CONTROL_TEXT_COLOR));

		BMessage* clickMsg = new BMessage(kMsgToolToggle);
		clickMsg->AddInt32("block_idx", (int32)fPendingAsstBlock);

		std::string arrow_text;
		arrow_text += kArrowOpen;  // ▾
		arrow_text += " \xE2\x9C\xA8 Pluto";  // ✨

		TextSpan span(BString(arrow_text.c_str()), style);
		span.SetClickMessage(clickMsg);
		fCurrentPara.Append(span);
	}
	_Append("\n", ui_color(B_CONTROL_TEXT_COLOR));

	fAsstBodyOff = 0;  // legacy offset, unused
}

// ---- finalize assistant block ----

void ConversationView::_FinalizeAssistantBlock() {
	if (fAsstHeaderOff < 0 || fAsstBodyOff < 0) return;

	// Fill in the pre-allocated assistant block with body paragraph info.
	if (fPendingAsstBlock >= 0 && fTextDoc.IsSet()) {
		auto* b = fToolBlocks.at(fPendingAsstBlock);
		if (b) {
			b->header_para_count = fAsstDocParaStart - b->header_para;
			if (b->header_para_count <= 0)
				b->header_para_count = 1;
			b->body_para = fAsstDocParaStart;
			b->body_para_count = fTextDoc->CountParagraphs() - fAsstDocParaStart;
			b->body_text = fAsstMarkdownBuf;

			// If the body is short, the block doesn't need collapse.
			// We leave it registered but it'll be a no-op on toggle.
			if (fAsstMarkdownBuf.size() <= 400)
				b->body_para_count = 0;  // not collapsible
		}
	}

	fAsstMarkdownBuf.clear();
	fAsstHeaderOff = -1;
	fAsstBodyOff   = -1;
	fAsstHeaderPara = -1;
	fPendingAsstBlock = -1;
}

// ---- inline markdown renderer ----

void ConversationView::_RenderAssistantInline(const std::string& line) {
	size_t i = 0;
	while (i < line.size()) {
		// **bold**
		if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
			size_t end = line.find("**", i + 2);
			if (end != std::string::npos) {
				_Append(line.substr(i + 2, end - i - 2),
					kColAsstBody, B_BOLD_FACE, false);
				i = end + 2;
				continue;
			}
		}
		// *italic*
		if (line[i] == '*' && (i == 0 || line[i - 1] != '*')
		    && i + 1 < line.size() && line[i + 1] != '*') {
			size_t end = line.find('*', i + 1);
			if (end != std::string::npos && (end + 1 >= line.size()
			    || line[end + 1] != '*')) {
				_Append(line.substr(i + 1, end - i - 1),
					kColAsstBody, B_ITALIC_FACE, false);
				i = end + 1;
				continue;
			}
		}
		// `inline code`
		if (line[i] == '`') {
			size_t end = line.find('`', i + 1);
			if (end != std::string::npos) {
				_Append(line.substr(i + 1, end - i - 1),
					ui_color(B_PANEL_BACKGROUND_COLOR), 0, true);
				i = end + 1;
				continue;
			}
		}
		// [text](url)
		if (line[i] == '[') {
			size_t close = line.find(']', i + 1);
			if (close != std::string::npos && close + 1 < line.size()
			    && line[close + 1] == '(') {
				size_t pclose = line.find(')', close + 2);
				if (pclose != std::string::npos) {
					std::string label = line.substr(i + 1, close - i - 1);
					_Append(label, ui_color(B_CONTROL_TEXT_COLOR), B_UNDERSCORE_FACE, false);
					i = pclose + 1;
					continue;
				}
			}
		}
		// ~~strikethrough~~
		if (i + 1 < line.size() && line[i] == '~' && line[i + 1] == '~') {
			size_t end = line.find("~~", i + 2);
			if (end != std::string::npos) {
				_Append(line.substr(i + 2, end - i - 2),
					ui_color(B_DOCUMENT_TEXT_COLOR), B_STRIKEOUT_FACE, false);
				i = end + 2;
				continue;
			}
		}
		// Plain text — accumulate until next special char
		size_t next = i + 1;
		while (next < line.size()
		    && line[next] != '*' && line[next] != '`'
		    && line[next] != '[' && line[next] != '~')
			++next;
		_Append(line.substr(i, next - i), kColAsstBody, 0, false);
		i = next;
	}
}

// ---- code block flush ----

void ConversationView::_FlushMdCodeBlockImpl(const std::string& code,
                                              const std::string& lang) {
	if (!lang.empty())
		_Append("  \xE2\x94\x8C\xE2\x94\x80 " + lang + " \xE2\x94\x80\xE2\x94\x80\n",
		ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);  // ┌─ lang ──
	auto syn_tokens = highlight(code, lang);
	for (const auto& tok : syn_tokens) {
		rgb_color tc = ui_color(B_PANEL_BACKGROUND_COLOR);
		switch (tok.type) {
			case TokenType::Keyword:      tc = ui_color(B_CONTROL_TEXT_COLOR); break;
			case TokenType::String:       tc = ui_color(B_CONTROL_TEXT_COLOR); break;
			case TokenType::Comment:      tc = ui_color(B_DOCUMENT_TEXT_COLOR); break;
			case TokenType::Number:       tc = ui_color(B_CONTROL_TEXT_COLOR); break;
			case TokenType::Preprocessor: tc = ui_color(B_CONTROL_TEXT_COLOR); break;
			case TokenType::Type:         tc = ui_color(B_CONTROL_TEXT_COLOR); break;
			default: break;
		}
		_Append(tok.text, tc, 0, true);
	}
	_Append("\n", kColAsstBody);
}

// ---- single line renderer ----

void ConversationView::_RenderAssistantLine(const std::string& line) {
	// Headings
	if (!line.empty() && line[0] == '#') {
		int level = 0;
		while (level < (int)line.size() && line[level] == '#') ++level;
		if (level <= 6 && (level >= (int)line.size() || line[level] == ' ')) {
			std::string heading = line.substr(level);
			while (!heading.empty() && heading.front() == ' ')
				heading.erase(0, 1);
			_Append("\n", kColAsstBody);
			_Append(heading + "\n", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
			if (level <= 2) {
				size_t ul = heading.size();
				if (ul > 40) ul = 40;
				std::string underline;
				for (size_t j = 0; j < ul; ++j)
					underline += (level == 1)
						? "\xe2\x94\x81"   // ━
						: "\xe2\x94\x80";   // ─
				_Append(underline + "\n", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			}
			_Append("\n", kColAsstBody);
			return;
		}
	}

	// Horizontal rule
	if (line.size() >= 3 && (line.find("---") == 0 || line.find("***") == 0)) {
		bool is_rule = true;
		for (char c : line)
			if (c != '-' && c != '*' && c != ' ')
				{ is_rule = false; break; }
		if (is_rule) {
			_Append("\n  \xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80\n\n", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			return;
		}
	}

	// Blockquote — left bar + italic text
	if (!line.empty() && line[0] == '>') {
		std::string q = line.substr(1);
		while (!q.empty() && q.front() == ' ') q.erase(0, 1);
		_Append("   \xE2\x94\x83 ", ui_color(B_CONTROL_TEXT_COLOR), 0, false);  // ┃ (thick bar)
		_Append(q, ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE, false);
		_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		return;
	}

	// Task list  - [ ] or - [x]
	if (line.size() >= 5 && line[0] == '-' && line[1] == ' '
	    && line[2] == '[') {
		bool checked = (line[3] == 'x' || line[3] == 'X');
		std::string item = line.substr(5);
		while (!item.empty() && item.front() == ' ') item.erase(0, 1);
		const char* box = checked
			? "  \xe2\x98\x91 "    // ☑
			: "  \xe2\x98\x90 ";   // ☐
		_Append(box, checked ? ui_color(B_CONTROL_TEXT_COLOR) : ui_color(B_DOCUMENT_TEXT_COLOR));
		if (checked)
			_Append(item, ui_color(B_DOCUMENT_TEXT_COLOR), B_STRIKEOUT_FACE);
		else
			_RenderAssistantInline(item);
		_Append("\n", kColAsstBody);
		return;
	}

	// Bullet list — wider indent, softer bullet
	if (line.size() >= 2 && (line[0] == '-' || line[0] == '*')
	    && line[1] == ' ') {
		_Append("   \xE2\x80\xA2 ", ui_color(B_DOCUMENT_TEXT_COLOR));  // •  (dimmed bullet)
		_RenderAssistantInline(line.substr(2));
		_Append("\n", kColAsstBody);
		return;
	}

	// Numbered list
	if (line.size() >= 3 && std::isdigit((unsigned char)line[0])) {
		size_t dot = line.find(". ");
		if (dot != std::string::npos && dot <= 3) {
			_Append("  " + line.substr(0, dot + 2),
				kColAsstBody, B_BOLD_FACE);
			_RenderAssistantInline(line.substr(dot + 2));
			_Append("\n", kColAsstBody);
			return;
		}
	}

	// Empty line
	if (line.empty()) {
		_Append("\n", kColAsstBody);
		return;
	}

	// Normal paragraph with inline formatting
	_RenderAssistantInline(line);
	_Append("\n", kColAsstBody);
}

// ---- streaming markdown API ----

void ConversationView::_FeedStreamingMarkdown(const std::string& chunk) {
	fStreamingMd.feed(chunk);
}

void ConversationView::_FlushStreamingMarkdown() {
	fStreamingMd.flush();
}

void ConversationView::_ResetStreamingMarkdown() {
	fStreamingMd.reset();
	fMdPreviewLen = 0;
}

void ConversationView::_RenderAssistantMarkdown(const std::string& text) {
	if (fTextDoc.IsSet()) {
		BString mdText(text.c_str());
		fMdParser.AppendMarkdown(fTextDoc, mdText);
		fDocView->SetTextDocument(fTextDoc);
		fDocView->Relayout();
	}
}

// ---- streaming cursor / thinking spinner ----
//
// Both the blinking cursor (█) and the thinking spinner (⠋ thinking…) are
// implemented as a temporary paragraph appended to the end of the document.
// _ShowCursor/_ShowSpinner append it; _HideCursor removes it by popping
// the last paragraph. This avoids byte-counting issues with the TextDocument
// model which operates on paragraphs, not raw bytes.

void ConversationView::_ShowCursor() {
	if (fCursorShown) return;
	// Flush any pending spans in fCurrentPara first.
	if (!fCurrentPara.IsEmpty() && fTextDoc.IsSet()) {
		fTextDoc->Append(fCurrentPara);
		ParagraphStyle pstyle;
		pstyle.SetSpacingTop(0);
		pstyle.SetSpacingBottom(0);
		fCurrentPara = Paragraph(pstyle);
	}
	// Append cursor as its own paragraph.
	BFont font = *be_plain_font;
	font.SetFace(B_BOLD_FACE);
	if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
	CharacterStyle style;
	style.SetFont(font);
	style.SetForegroundColor(ui_color(B_CONTROL_TEXT_COLOR));
	Paragraph cursorPara;
	cursorPara.Append(TextSpan(kCursorChar, style));
	fTextDoc->Append(cursorPara);
	fDocView->SetTextDocument(fTextDoc);
	fDocView->Relayout();
	// Scroll to bottom.
	if (fOutputScroll) {
		BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
		if (sb) { float mn, mx; sb->GetRange(&mn, &mx); sb->SetValue(mx); }
	}
	fCursorShown = true;
}

void ConversationView::_HideCursor() {
	if (!fCursorShown) return;
	// Remove the last paragraph (cursor or spinner) by computing its
	// text offset and length from the document model.
	if (fTextDoc.IsSet()) {
		int32 count = fTextDoc->CountParagraphs();
		if (count > 0) {
			// Calculate offset of the last paragraph.
			int32 offset = 0;
			for (int32 i = 0; i < count - 1; ++i)
				offset += fTextDoc->ParagraphAtIndex(i).Length();
			int32 lastLen = fTextDoc->ParagraphAtIndex(count - 1).Length();
			if (lastLen > 0)
				fTextDoc->Remove(offset, lastLen);
		}
		fDocView->SetTextDocument(fTextDoc);
	}
	fCursorShown = false;
}

void ConversationView::OnCursorTick() {
	// Thinking spinner: braille dots animation before first text arrives.
	if (fThinking && fBusy) {
		static const char* kFrames[] = {
			"\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8",
			"\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7",
			"\xE2\xA0\x87", "\xE2\xA0\x8F"
		};  // ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏

		BFont font = *be_plain_font;
		font.SetFace(B_ITALIC_FACE);
		if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
		CharacterStyle style;
		style.SetFont(font);
		style.SetForegroundColor(ui_color(B_DOCUMENT_TEXT_COLOR));

		BString text;
		text.SetToFormat("%s thinking\xE2\x80\xA6", kFrames[fSpinnerFrame % 10]);

		if (fCursorShown && fTextDoc.IsSet()) {
			// Update spinner text in-place: replace the last paragraph's
			// content without removing/re-adding it to avoid layout flicker.
			int32 count = fTextDoc->CountParagraphs();
			if (count > 0) {
				int32 offset = 0;
				for (int32 i = 0; i < count - 1; ++i)
					offset += fTextDoc->ParagraphAtIndex(i).Length();
				int32 lastLen = fTextDoc->ParagraphAtIndex(count - 1).Length();
				if (lastLen > 0)
					fTextDoc->Replace(offset, lastLen, BString(text), style);
				fDocView->Relayout();
			}
		} else {
			// First tick: flush pending content and append spinner paragraph.
			if (!fCurrentPara.IsEmpty() && fTextDoc.IsSet()) {
				fTextDoc->Append(fCurrentPara);
				ParagraphStyle pstyle;
				pstyle.SetSpacingTop(0);
				pstyle.SetSpacingBottom(0);
				fCurrentPara = Paragraph(pstyle);
			}
			Paragraph spinPara;
			spinPara.Append(TextSpan(text, style));
			fTextDoc->Append(spinPara);
			fDocView->SetTextDocument(fTextDoc);
			fDocView->Relayout();
			if (fOutputScroll) {
				BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
				if (sb) { float mn, mx; sb->GetRange(&mn, &mx); sb->SetValue(mx); }
			}
			fCursorShown = true;
		}
		fSpinnerFrame++;
		return;
	}

	// Normal streaming cursor: blinking block.
	if (!fTextOngoing) return;
	if (fCursorShown)
		_HideCursor();
	else
		_ShowCursor();
}

// ---- render all messages (used after load) ----

void ConversationView::_RenderMessages() {
	_ClearInlineImages();
	// Legacy BTextView removed — no text to clear.
	// Reset the document model.
	fTextDoc.SetTo(new TextDocument(), true);
	fDocView->SetTextDocument(fTextDoc);

	BString header;
	header.SetToFormat("(loaded %ld turn%s)\n\n",
		(long)fMessages.size(), fMessages.size() == 1 ? "" : "s");
	_Append(header.String(), ui_color(B_DOCUMENT_TEXT_COLOR));

	for (const auto& m : fMessages) {
		if (m.role == "user") {
			bool has_content = false;
			for (const auto& b : m.content)
				if (b.type == ContentBlock::TEXT || b.type == ContentBlock::IMAGE
				    || b.type == ContentBlock::AUDIO || b.type == ContentBlock::DOCUMENT)
					{ has_content = true; break; }
			if (has_content) _EmitUserHeader();
			for (const auto& b : m.content) {
				if (b.type == ContentBlock::TEXT) {
					_Append(b.text + "\n\n", kColUserBody);
				} else if (b.type == ContentBlock::IMAGE) {
					if (!b.image_data.empty())
						_InsertInlineImage(b.image_data);
					else {
						std::string label = "\xf0\x9f\x96\xbc [";
						label += b.image_media_type.empty() ? "image" : b.image_media_type;
						label += "]\n";
						_Append(label, ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
					}
				} else if (b.type == ContentBlock::AUDIO) {
					std::string label = "\xf0\x9f\x94\x8a [audio: ";
					label += b.audio_format.empty() ? "?" : b.audio_format;
					label += ", ";
					label += std::to_string(b.audio_data.size() / 1370);
					label += " KB]\n";
					_Append(label, ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
				} else if (b.type == ContentBlock::DOCUMENT) {
					std::string label = "\xf0\x9f\x93\x84 [PDF: ";
					label += std::to_string(b.doc_data.size() / 1370);
					label += " KB]\n";
					_Append(label, ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
				} else if (b.type == ContentBlock::TOOL_RESULT) {
					std::string body = b.text;
					if (body.size() > 600)
						body = body.substr(0, 600) + "\n\xe2\x80\xa6";
					body += "\n";
					_Append(body, b.is_error ? rgb_color{245, 101, 101, 255} : ui_color(B_PANEL_BACKGROUND_COLOR), 0, true);
				}
			}
		} else if (m.role == "assistant") {
			bool has_text = false;
			for (const auto& b : m.content)
				if (b.type == ContentBlock::TEXT) { has_text = true; break; }
			if (has_text) _EmitAssistantHeader();
			for (const auto& b : m.content) {
				if (b.type == ContentBlock::TEXT) {
					_RenderAssistantMarkdown(b.text + "\n");
				} else if (b.type == ContentBlock::IMAGE) {
					if (!b.image_data.empty())
						_InsertInlineImage(b.image_data);
					else {
						std::string label = "\xf0\x9f\x96\xbc [";
						label += b.image_media_type.empty() ? "image" : b.image_media_type;
						label += "]\n";
						_Append(label, ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
					}
				} else if (b.type == ContentBlock::AUDIO) {
					_Append("\xf0\x9f\x94\x8a [audio attached]\n", ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
				} else if (b.type == ContentBlock::DOCUMENT) {
					_Append("\xf0\x9f\x93\x84 [PDF attached]\n", ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
				} else if (b.type == ContentBlock::TOOL_USE) {
					std::string head = "\n\xE2\x97\x86 ";  // ◆
					head += b.tool_name;
					head += "\n";
					_Append(head, ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
				}
			}
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}
	}
}

// ---- worker event handlers ----

bool ConversationView::HandleTextDelta(const std::string& text) {
	bool first = false;
	if (!fTextOngoing) {
		// Stop thinking spinner — first text has arrived.
		if (fThinking) {
			fThinking = false;
			_HideCursor();  // remove spinner
		} else {
			_HideCursor();
		}
		_Append("\n", kColAsstBody);
		_EmitAssistantHeader();
		fTextOngoing = true;
		fAsstMarkdownBuf.clear();
		// Remember where the assistant content starts in the document.
		fAsstDocParaStart = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : 0;
		first = true;
	}
	_HideCursor();

	// Accumulate markdown text.
	fAsstMarkdownBuf += text;

	// Remove previous assistant content from the document
	// and re-render with MarkdownParser for proper markdown.
	if (fTextDoc.IsSet()) {
		// Remove paragraphs from fAsstDocParaStart to end.
		int32 paraCount = fTextDoc->CountParagraphs();
		if (paraCount > fAsstDocParaStart) {
			// Calculate text offset at the start of assistant content.
			int32 offset = 0;
			for (int32 i = 0; i < fAsstDocParaStart; ++i)
				offset += fTextDoc->ParagraphAtIndex(i).Length();
			int32 totalLen = fTextDoc->Length();
			if (totalLen > offset)
				fTextDoc->Remove(offset, totalLen - offset);
		}

		// Re-render the accumulated markdown.
		BString mdText(fAsstMarkdownBuf.c_str());
		fMdParser.AppendMarkdown(fTextDoc, mdText);
		fDocView->SetTextDocument(fTextDoc);
		fDocView->Relayout();

		// Scroll to bottom.
		if (fOutputScroll) {
			BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
			if (sb) {
				float min, max;
				sb->GetRange(&min, &max);
				sb->SetValue(max);
			}
		}
	}

	_ShowCursor();
	fLastAssistantText += text;
	return first;
}

void ConversationView::HandleToolStart(const std::string& name,
                                        const std::string& preview) {
	_FinalizeAssistantBlock();
	fTextOngoing = false;
	fThinking = false;
	fShimmerStart = -1;
	_HideCursor();
	fCurrentTool = name;

	ToolBlock block;
	block.collapsed = false;
	block.is_error = false;

	// Record paragraph index before appending the header.
	block.header_para = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

	// Build the header with a clickable arrow span carrying a tool-toggle message.
	_Append("\n", ui_color(B_CONTROL_TEXT_COLOR));

	// Create the arrow+tool name as a clickable span with a toggle message.
	{
		BFont font = *be_plain_font;
		font.SetFace(B_BOLD_FACE);
		if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
		CharacterStyle style;
		style.SetFont(font);
		style.SetForegroundColor(ui_color(B_CONTROL_TEXT_COLOR));

		// Attach a click message for tool block toggle.
		BMessage* clickMsg = new BMessage(kMsgToolToggle);
		clickMsg->AddInt32("block_idx", (int32)fToolBlocks.count());

		std::string arrow_text;
		arrow_text += kArrowOpen;
		arrow_text += " \xF0\x9F\x94\xA7 ";  // 🔧
		arrow_text += preview;

		TextSpan span(BString(arrow_text.c_str()), style);
		span.SetClickMessage(clickMsg);
		fCurrentPara.Append(span);
	}
	_Append("\n", ui_color(B_CONTROL_TEXT_COLOR));

	// Record header paragraph count.
	if (fTextDoc.IsSet() && block.header_para >= 0)
		block.header_para_count = fTextDoc->CountParagraphs() - block.header_para;

	fPendingTool = fToolBlocks.add(block);
}

std::string ConversationView::HandleToolResult(const std::string& output,
                                                bool is_error) {
	fTextOngoing = false;
	fShimmerStart = -1;
	_HideCursor();

	std::string display = output;
	if (display.size() > 600)
		display = display.substr(0, 600)
			+ "\n\xE2\x80\xA6 [click \xE2\x96\xB8 to expand or see side panel]";
	display += "\n";

	if (auto* b_ptr = fToolBlocks.at(fPendingTool)) {
		auto& b = *b_ptr;
		b.body_text = display;
		b.is_error = is_error;

		// Record body paragraph start.
		b.body_para = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

		_Append(display, is_error ? rgb_color{245, 101, 101, 255} : ui_color(B_PANEL_BACKGROUND_COLOR), 0, true);

		// Record body paragraph count.
		if (fTextDoc.IsSet() && b.body_para >= 0)
			b.body_para_count = fTextDoc->CountParagraphs() - b.body_para;

		// Auto-collapse long tool output.
		if (display.size() > 400 && fTextDoc.IsSet()) {
			fToolBlocks.collapse(fPendingTool, fTextDoc);
			fDocView->SetTextDocument(fTextDoc);
			fDocView->Relayout();
		}

		fPendingTool = -1;
	} else {
		_Append(display, is_error ? rgb_color{245, 101, 101, 255} : ui_color(B_PANEL_BACKGROUND_COLOR), 0, true);
	}
	std::string tool = fCurrentTool;
	fCurrentTool.clear();
	return tool;
}

void ConversationView::HandleError(const std::string& error) {
	fTextOngoing = false;
	fShimmerStart = -1;
	_HideCursor();
	std::string line = "\n\xE2\x9A\xA0 ";  // ⚠
	line += error;
	line += "\n";
	_Append(line, rgb_color{245, 101, 101, 255}, B_BOLD_FACE);
	if (fDelegate) fDelegate->OnError(error);
}

void ConversationView::HandleUsage(long in, long out,
                                    long cache_read, long cache_create) {
	fTokensIn  += in;
	fTokensOut += out;
	fLastInput  = in;
	fLastOutput = out;
	fLastCacheRead = cache_read;
	fLastCacheCreate = cache_create;
	fTurnCount++;
}

std::string ConversationView::HandleTurnDone() {
	_FinalizeAssistantBlock();
	_HideCursor();
	_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
	fBusy = false;
	fTextOngoing = false;
	fThinking = false;
	fShimmerStart = -1;
	fCurrentTool.clear();
	fWorker = -1;
	std::string preview = fLastAssistantText;
	fLastAssistantText.clear();
	return preview;
}

void ConversationView::HandleStatusMessage(const std::string& status,
                                            bool is_warning) {
	if (is_warning) {
		_Append("\xe2\x9a\xa0 ", {255, 180, 0, 255}, B_BOLD_FACE);
		_Append(status + "\n", {255, 180, 0, 255});
	} else {
		std::string line = "[retry] ";
		line += status;
		line += "\n";
		_Append(line, ui_color(B_DOCUMENT_TEXT_COLOR));
	}
}

void ConversationView::HandleRagHint(const std::string& hint) {
	_Append(hint, ui_color(B_DOCUMENT_TEXT_COLOR));
}

void ConversationView::PrepareTurn(std::vector<ContentBlock> user_blocks) {
	Message user;
	user.role = "user";
	user.content = std::move(user_blocks);
	fMessages.push_back(std::move(user));

	fBusy = true;
	fTextOngoing = false;
	fThinking = true;
	fSpinnerFrame = 0;
	fShimmerStart = -1;
	fLastAssistantText.clear();
	fTurnStartedAt = system_time();
}

void ConversationView::AppendText(const std::string& text, rgb_color color,
                                   uint16 face, bool mono) {
	_Append(text, color, face, mono);
}

// ---- worker thread ----

struct WorkerCtx {
	BMessenger            target;
	Provider*             provider;
	Config*               cfg;
	ToolRegistry*         tools;
	std::vector<Message>* messages;
	std::atomic<bool>*    cancel;
};

int32 ConversationView::_WorkerEntry(void* data) {
	std::unique_ptr<WorkerCtx> ctx(static_cast<WorkerCtx*>(data));
	BMessenger target = ctx->target;

	// Cross-thread permission: post to ConversationView, block on sem with timeout.
	auto gui_permission = [target](const Tool& tool,
	                               const std::string& preview) -> Permission {
		char sem_name[64];
		snprintf(sem_name, sizeof(sem_name), "pluto-approval-%d",
			(int)getpid());
		sem_id sem = create_sem(0, sem_name);
		if (sem < 0) {
			// Fail closed: deny if semaphore creation fails.
			return Permission::Deny;
		}
		// RAII guard: ensure semaphore is always deleted.
		struct SemGuard {
			sem_id id;
			~SemGuard() { if (id >= 0) delete_sem(id); }
		} guard{sem};
		int result = 1;
		BMessage m(kMsgApproval);
		m.AddString("name", tool.name().c_str());
		m.AddString("preview", preview.c_str());
		m.AddInt32("sem", static_cast<int32>(sem));
		m.AddPointer("result", &result);
		if (target.SendMessage(&m) != B_OK) {
			// Target gone (view destroyed) — deny.
			return Permission::Deny;
		}
		// Wait up to 5 minutes for user response.
		// acquire_sem_etc with B_TIMEOUT returns B_TIMED_OUT if no signal.
		constexpr bigtime_t kApprovalTimeout = 5 * 60 * 1000000LL;  // 5 min in microseconds
		status_t err = acquire_sem_etc(sem, B_TIMEOUT, kApprovalTimeout);
		if (err == B_TIMED_OUT || err != B_OK) {
			// Timeout or error — deny.
			return Permission::Deny;
		}
		if (result == 0) return Permission::Deny;
		if (result == 2) return Permission::AllowAlways;
		return Permission::Allow;
	};

	UiCallbacks ui;
	ui.on_text = [target](const std::string& text) {
		BMessage m(kMsgWorkerText);
		m.AddString("text", text.c_str());
		target.SendMessage(&m);
	};
	ui.on_tool_start = [target](const std::string& name,
	                            const std::string& preview) {
		BMessage m(kMsgWorkerTS);
		m.AddString("name", name.c_str());
		m.AddString("preview", preview.c_str());
		target.SendMessage(&m);
	};
	ui.on_tool_result = [target](const std::string& name,
	                             const std::string& input,
	                             const ToolResult& r) {
		BMessage m(kMsgWorkerTR);
		m.AddString("name", name.c_str());
		m.AddString("input", input.c_str());
		m.AddString("output", r.output.c_str());
		m.AddBool("error", r.is_error);
		target.SendMessage(&m);
	};
	ui.on_image = [target](const std::string& base64_data,
	                       const std::string& media_type) {
		BMessage m(kMsgWorkerText);
		std::string label = "\xf0\x9f\x96\xbc [" + media_type + "]\n";
		m.AddString("text", label.c_str());
		target.SendMessage(&m);
		BMessage im(kMsgWorkerImage);
		im.AddString("data", base64_data.c_str());
		im.AddString("media_type", media_type.c_str());
		target.SendMessage(&im);
	};
	ui.on_error = [target](const std::string& err) {
		BMessage m(kMsgWorkerErr);
		m.AddString("error", err.c_str());
		target.SendMessage(&m);
	};
	ui.on_usage = [target](int in_tok, int out_tok,
	                       int cache_read, int cache_create) {
		BMessage m(kMsgWorkerUsage);
		m.AddInt32("in", in_tok);
		m.AddInt32("out", out_tok);
		m.AddInt32("cache_read", cache_read);
		m.AddInt32("cache_create", cache_create);
		target.SendMessage(&m);
	};
	ui.on_status = [target](const std::string& status) {
		BMessage m(kMsgWorkerStatus);
		m.AddString("status", status.c_str());
		target.SendMessage(&m);
	};
	ui.on_rag_inject = [target](const std::vector<std::string>& topics) {
		BMessage m(kMsgWorkerRag);
		for (const auto& t : topics)
			m.AddString("topic", t.c_str());
		target.SendMessage(&m);
	};

	RunTurn(*ctx->provider, *ctx->cfg, *ctx->tools,
	         *ctx->messages, gui_permission, ui, ctx->cancel);

	BMessage done(kMsgWorkerDone);
	target.SendMessage(&done);
	return 0;
}

void ConversationView::_StartTurn() {
	fCancelFlag.store(false, std::memory_order_relaxed);
	auto* ctx = new WorkerCtx{
		BMessenger(this), fProvider, fConfig, fTools, &fMessages, &fCancelFlag
	};
	fWorker = spawn_thread(_WorkerEntry, "pluto-turn",
	                       B_NORMAL_PRIORITY, ctx);
	if (fWorker < 0) {
		delete ctx;
		HandleError("spawn_thread failed");
		std::string preview = HandleTurnDone();
		if (fDelegate) fDelegate->OnTurnDone(preview);
		return;
	}
	resume_thread(fWorker);
}

// ---- MessageReceived ----

void ConversationView::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgCursorTick:
			OnCursorTick();
			if (fBusy && fDelegate)
				fDelegate->OnStatusChanged();
			break;

		case kMsgWorkerText: {
			const char* t = nullptr;
			if (msg->FindString("text", &t) == B_OK && t) {
				bool first = HandleTextDelta(t);
				if (first && fDelegate)
					fDelegate->OnTitleActivity("writing\xe2\x80\xa6");
			}
			break;
		}

		case kMsgWorkerTS: {
			const char* name = "";
			const char* prev = "";
			msg->FindString("name", &name);
			msg->FindString("preview", &prev);
			HandleToolStart(name, prev);
			if (fDelegate) {
				BString act;
				act.SetToFormat("running %s", name);
				fDelegate->OnTitleActivity(act.String());
				fDelegate->OnStatusChanged();
			}
			break;
		}

		case kMsgWorkerTR: {
			const char* out = "";
			const char* in_json = "";
			bool is_err = false;
			msg->FindString("output", &out);
			msg->FindString("input", &in_json);
			msg->FindBool("error", &is_err);
			std::string full(out ? out : "");
			std::string tool = HandleToolResult(full, is_err);
			if (fDelegate) {
				fDelegate->OnToolOutput(tool, full,
					in_json ? in_json : "", is_err);
				fDelegate->OnStatusChanged();
			}
			break;
		}

		case kMsgWorkerErr: {
			const char* e = "";
			msg->FindString("error", &e);
			HandleError(e ? e : "");
			break;
		}

		case kMsgWorkerImage: {
			const char* data = "";
			const char* mt = "";
			msg->FindString("data", &data);
			msg->FindString("media_type", &mt);
			if (data && *data) {
				_InsertInlineImage(data);
				if (fDelegate)
					fDelegate->OnImage(data, mt ? mt : "");
			}
			break;
		}

		case kMsgWorkerStatus: {
			const char* s = "";
			msg->FindString("status", &s);
			std::string status = s ? s : "";
			bool is_no_tools = (status.find("does not support tools")
				!= std::string::npos);
			HandleStatusMessage(
				is_no_tools
					? "This model does not support tools \xe2\x80\x94 running in chat-only mode."
					: status,
				is_no_tools);
			if (fDelegate) fDelegate->OnProviderStatus(status);
			break;
		}

		case kMsgWorkerRag: {
			std::vector<std::string> topics;
			const char* topic = nullptr;
			for (int32 i = 0; msg->FindString("topic", i, &topic) == B_OK; ++i)
				topics.push_back(topic);
			if (!topics.empty()) {
				std::string hint = "[beapi] injected: " + topics[0];
				if (topics.size() > 1)
					hint += ", " + topics[1] + "...";
				hint += "\n";
				HandleRagHint(hint);
				if (fDelegate) fDelegate->OnRagInject(topics);
			}
			break;
		}

		case kMsgWorkerUsage: {
			int32 in_tok = 0, out_tok = 0, c_read = 0, c_create = 0;
			msg->FindInt32("in", &in_tok);
			msg->FindInt32("out", &out_tok);
			msg->FindInt32("cache_read", &c_read);
			msg->FindInt32("cache_create", &c_create);
			HandleUsage(in_tok, out_tok, c_read, c_create);
			TelemetryRecord(fConfig->model, in_tok, out_tok, c_read, c_create);
			if (fDelegate) fDelegate->OnStatusChanged();
			break;
		}

		case kMsgWorkerDone: {
			std::string preview = HandleTurnDone();
			if (fDelegate) fDelegate->OnTurnDone(preview);
			// Auto-save session after each turn.
			if (!fMessages.empty()) SaveSession();
			break;
		}

		case kMsgApproval: {
			const char* name = "";
			const char* prev = "";
			int32 sem_val = -1;
			void* result_ptr = nullptr;
			msg->FindString("name", &name);
			msg->FindString("preview", &prev);
			msg->FindInt32("sem", &sem_val);
			msg->FindPointer("result", &result_ptr);
			int* result = static_cast<int*>(result_ptr);

			if (fAlwaysApproved.count(name)) {
				if (result) *result = 1;
				if (sem_val >= 0) release_sem(static_cast<sem_id>(sem_val));
				break;
			}

			int rc = fDelegate
				? fDelegate->RequestToolApproval(name, prev)
				: 1;
			if (result) *result = rc;
			if (rc == 2) fAlwaysApproved.insert(name);
			if (sem_val >= 0) release_sem(static_cast<sem_id>(sem_val));
			break;
		}

		case kMsgToolToggle: {
			int32 block_idx = -1;
			msg->FindInt32("block_idx", &block_idx);
			if (block_idx >= 0 && fTextDoc.IsSet()) {
				fToolBlocks.toggle(block_idx, fTextDoc);
				fDocView->SetTextDocument(fTextDoc);
				fDocView->Relayout();
			}
			break;
		}

		case kMsgLinkClick: {
			const char* url = nullptr;
			if (msg->FindString("url", &url) == B_OK && url && *url) {
				std::string u(url);
				if (u[0] == '#') {
					// Anchor link — scroll within conversation (no-op for now,
					// could search for heading text).
				} else if (u.find("://") != std::string::npos) {
					// External URL — open in browser via Haiku roster.
					const char* argv[] = { url, nullptr };
					be_roster->Launch("text/html", 1, const_cast<char**>(argv));
				}
			}
			break;
		}

		default:
			BView::MessageReceived(msg);
			break;
	}
}

// ---- public API ----

void ConversationView::SendInput(std::vector<ContentBlock> blocks) {
	if (fBusy) return;
	PrepareTurn(std::move(blocks));
	if (fDelegate) fDelegate->OnTurnStarted();
	_StartTurn();
}

void ConversationView::Cancel() {
	fCancelFlag.store(true, std::memory_order_relaxed);
}

void ConversationView::Reset() {
	fMessages.clear();
	fToolBlocks.clear();
	fPendingTool = -1;
	fStreamingMd.reset();
	fMdPreviewLen = 0;
	fTokensIn = 0;
	fTokensOut = 0;
	fLastInput = 0;
	fLastOutput = 0;
	fLastCacheRead = 0;
	fLastCacheCreate = 0;
	fTurnCount = 0;
	fSessionPath.clear();
	fSessionTitle.clear();
	fTextOngoing = false;
	fCursorShown = false;
	fShimmerStart = -1;
	fAsstHeaderOff = -1;
	fAsstBodyOff = -1;
	fAsstHeaderPara = -1;
	fPendingAsstBlock = -1;
	fCurrentTool.clear();
	fLastAssistantText.clear();

	_ClearInlineImages();
	// Reset document model.
	fTextDoc.SetTo(new TextDocument(), true);
	if (fDocView) fDocView->SetTextDocument(fTextDoc);
}

void ConversationView::LoadMessages(const std::vector<Message>& msgs) {
	Reset();
	fMessages = msgs;
	fSessionTitle = "fork";
	_RenderMessages();
	if (fDelegate) fDelegate->OnTitleChanged("fork");
}

void ConversationView::LoadSession(const std::string& path) {
	std::vector<Message> msgs;
	SessionMetadata meta;
	std::string error;

	if (!SessionStore::load(path, msgs, meta, error)) {
		_Append("[error] " + error + "\n", rgb_color{245, 101, 101, 255});
		return;
	}

	// Reset state before populating.
	fMessages = std::move(msgs);
	fToolBlocks.clear();
	fSessionPath = path;
	fSessionTitle = meta.title;
	fTokensIn = meta.tokens_in;
	fTokensOut = meta.tokens_out;
	fTurnCount = meta.turns;

	// Restore the model the session was created with.
	if (!meta.model.empty())
		fTabConfig.model = meta.model;
	fPendingTool = -1;
	fStreamingMd.reset();
	fMdPreviewLen = 0;
	fTextOngoing = false;
	fCursorShown = false;
	fShimmerStart = -1;
	fAsstHeaderOff = -1;
	fAsstBodyOff = -1;
	fCurrentTool.clear();
	fLastAssistantText.clear();

	_RenderMessages();

	if (fDelegate) {
		if (!fSessionTitle.empty())
			fDelegate->OnTitleChanged(fSessionTitle);
		fDelegate->OnStatusChanged();
	}
}

void ConversationView::SaveSession() {
	std::string error;
	std::string title = fSessionTitle.empty() ? "untitled" : fSessionTitle;
	std::string model = fConfig ? fConfig->model : "unknown";

	std::string saved = SessionStore::save(
		fSessionPath, title, model,
		fTokensIn, fTokensOut, fMessages, error);

	if (saved.empty()) {
		_Append("[error] save failed: " + error + "\n", rgb_color{245, 101, 101, 255});
		return;
	}

	// Update session path if this was a new save.
	if (fSessionPath.empty())
		fSessionPath = saved;
}

double ConversationView::EstimateCost() const {
	const std::string& m = fConfig->model;
	double in_per_mtok = 3.0, out_per_mtok = 15.0;
	std::string ml = m;
	for (auto& c : ml) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	if (ml.find("opus") != std::string::npos) {
		in_per_mtok = 15.0; out_per_mtok = 75.0;
	} else if (ml.find("sonnet") != std::string::npos) {
		in_per_mtok = 3.0; out_per_mtok = 15.0;
	} else if (ml.find("haiku") != std::string::npos) {
		in_per_mtok = 0.80; out_per_mtok = 4.0;
	} else if (ml.find("gpt-4o") != std::string::npos) {
		in_per_mtok = 5.0; out_per_mtok = 15.0;
	} else if (ml.find("gpt-4") != std::string::npos) {
		in_per_mtok = 30.0; out_per_mtok = 60.0;
	} else if (ml.find("gpt-3.5") != std::string::npos) {
		in_per_mtok = 0.50; out_per_mtok = 1.50;
	} else if (ml.find("llama") != std::string::npos
	         || ml.find("mistral") != std::string::npos
	         || ml.find("qwen") != std::string::npos
	         || ml.find("gemma") != std::string::npos
	         || ml.find("phi") != std::string::npos) {
		in_per_mtok = 0.0; out_per_mtok = 0.0;
	} else if (ml.find("deepseek") != std::string::npos) {
		in_per_mtok = 0.14; out_per_mtok = 0.28;
	} else if (ml.find("groq") != std::string::npos) {
		in_per_mtok = 0.0; out_per_mtok = 0.0;
	} else if (ml.find("gemini") != std::string::npos) {
		in_per_mtok = 0.075; out_per_mtok = 0.30;
	} else if (ml.find("command") != std::string::npos) {
		in_per_mtok = 2.50; out_per_mtok = 10.0;
	} else if (ml.find("grok") != std::string::npos) {
		in_per_mtok = 3.0; out_per_mtok = 15.0;
	}
	return (fTokensIn  * in_per_mtok / 1e6)
	     + (fTokensOut * out_per_mtok / 1e6);
}

void ConversationView::SetSessionTitle(const std::string& title) {
	fSessionTitle = title;
	if (fDelegate) fDelegate->OnTitleChanged(title);
}

void ConversationView::RequestCompaction() {
	// Inject a system message asking the model to compact the conversation.
	// This will be processed on the next turn.
	if (fBusy) return;
	
	Message sys;
	sys.role = "user";
	ContentBlock sb;
	sb.type = ContentBlock::TEXT;
	sb.text = "<system-reminder>\n"
		"The user has requested conversation compaction. "
		"Please provide a concise summary of the conversation so far, "
		"preserving key decisions, code changes, file paths, and important context. "
		"Be factual and brief.\n"
		"</system-reminder>";
	sys.content.push_back(std::move(sb));
	fMessages.push_back(std::move(sys));
	
	// Also trigger the built-in MaybeCompact logic by setting a flag
	// that will be checked on the next turn. For now, just inject the
	// request as a system message - the model will respond with a summary
	// and we'll handle it in the next turn.
	
	// Start a turn to process the compaction request.
	PrepareTurn({});
	if (fDelegate) fDelegate->OnTurnStarted();
	_StartTurn();
}

void ConversationView::SetProvider(std::unique_ptr<Provider> provider) {
	fOwnedProvider = std::move(provider);
	fProvider = fOwnedProvider.get();
}

void ConversationView::ApplyFontDelta(int delta) {
	fFontDelta = delta;

	// Re-render the entire conversation with the new font delta.
	// This rebuilds all paragraphs from the message history.
	if (!fMessages.empty()) {
		_RenderMessages();
	}
}

void ConversationView::UpdateParserColors(bool useSystem,
                                           rgb_color codeFg, rgb_color codeBg,
                                           rgb_color linkColor) {
	fMdParser.UpdateColors(useSystem, codeFg, codeBg, linkColor);
}

std::string ConversationView::BuildMarkdown() const {
	std::string out;
	out += "# Pluto conversation\n\n";
	out += "**Model:** " + (fConfig ? fConfig->model : "unknown") + "  \n";
	out += "**Tokens:** " + std::to_string(fTokensIn) + " in / "
	     + std::to_string(fTokensOut) + " out  \n";
	if (!fSessionTitle.empty())
		out += "**Title:** " + fSessionTitle + "  \n";
	out += "\n---\n\n";

	for (const auto& m : fMessages) {
		if (m.role == "user")           out += "## You\n\n";
		else if (m.role == "assistant") out += "## Pluto\n\n";

		for (const auto& b : m.content) {
			if (b.type == ContentBlock::TEXT) {
				out += b.text;
				out += "\n\n";
			} else if (b.type == ContentBlock::TOOL_USE) {
				out += "**Tool call:** `" + b.tool_name + "`\n\n";
				out += "```json\n" + b.tool_input + "\n```\n\n";
			} else if (b.type == ContentBlock::TOOL_RESULT) {
				out += b.is_error ? "**Tool error:**\n\n" : "**Tool result:**\n\n";
				out += "```\n" + b.text + "\n```\n\n";
			}
		}
	}
	return out;
}

void ConversationView::FindInConversation(const std::string& query,
                                           bool /*forward*/) {
	if (query.empty()) return;

	if (!fTextDoc.IsSet()) return;
	BString docText = fTextDoc->Text();
	int32 len = docText.Length();
	if (len <= 0) return;

	// Case-insensitive linear scan from fFindOffset.
	std::string q(query);
	for (auto& c : q) c = static_cast<char>(
		std::tolower(static_cast<unsigned char>(c)));

	std::string text(docText.String(), len);
	for (auto& c : text) c = static_cast<char>(
		std::tolower(static_cast<unsigned char>(c)));

	size_t pos = text.find(q, fFindOffset);
	if (pos == std::string::npos && fFindOffset > 0)
		pos = text.find(q);  // wrap
	if (pos == std::string::npos) return;

	// Select the found text in the TextDocumentView.
	if (fDocView) {
		fDocView->SetCaret(BPoint(0, 0), false);  // deselect
		// TODO: implement proper selection highlighting in TextDocumentView
	}
	fFindOffset = static_cast<int32>(pos + 1);
}

// ---- inline image support ----

namespace {

class InlineImageView : public BView {
public:
	InlineImageView(BBitmap* bitmap, float maxWidth,
	                const std::string& pngData)
		: BView("inlineImage", B_WILL_DRAW), fBitmap(bitmap),
		  fPngData(pngData)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		BRect br = fBitmap->Bounds();
		float iw = br.Width() + 1;
		float ih = br.Height() + 1;
		// Scale to fit maxWidth, cap height at 300.
		float scale = (iw > maxWidth && maxWidth > 10) ? maxWidth / iw : 1.0f;
		fDrawWidth = iw * scale;
		fDrawHeight = ih * scale;
		if (fDrawHeight > 300) {
			fDrawHeight = 300;
			fDrawWidth = iw * (300.0f / ih);
		}
	}

	~InlineImageView() override { delete fBitmap; }

	void Draw(BRect /*updateRect*/) override {
		if (!fBitmap) return;
		BRect src = fBitmap->Bounds();
		BRect dst(0, 0, fDrawWidth - 1, fDrawHeight - 1);
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fBitmap, src, dst);
	}

	void MouseDown(BPoint where) override {
		if (!fBitmap) return;
		uint32 buttons = 0;
		GetMouse(&where, &buttons);
		if (!(buttons & B_PRIMARY_MOUSE_BUTTON)) return;

		// Build a drag message with negotiated type for Tracker.
		BMessage drag(B_SIMPLE_DATA);
		drag.AddInt32("be:actions", B_COPY_TARGET);
		drag.AddString("be:types", "application/octet-stream");
		drag.AddString("be:filetypes", "image/png");
		drag.AddString("be:type_descriptions", "PNG image");
		drag.AddString("be:clip_name", "pluto_image.png");

		// Thumbnail for drag feedback.
		float thumbW = std::min(fDrawWidth, 100.0f);
		float thumbH = fDrawHeight * (thumbW / fDrawWidth);
		BBitmap* thumb = new BBitmap(BRect(0, 0, thumbW - 1, thumbH - 1),
		                             B_RGBA32, true);
		if (thumb->Lock()) {
			BView canvas(thumb->Bounds(), "", 0, 0);
			thumb->AddChild(&canvas);
			canvas.SetDrawingMode(B_OP_ALPHA);
			canvas.DrawBitmap(fBitmap, fBitmap->Bounds(), canvas.Bounds());
			canvas.Sync();
			thumb->RemoveChild(&canvas);
			thumb->Unlock();
		}
		DragMessage(&drag, thumb, B_OP_ALPHA,
		            BPoint(thumbW / 2, thumbH / 2));
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == B_COPY_TARGET) {
			_HandleCopyTarget(msg);
		} else {
			BView::MessageReceived(msg);
		}
	}

	BSize MinSize() override { return BSize(fDrawWidth, fDrawHeight); }
	BSize MaxSize() override { return BSize(fDrawWidth, fDrawHeight); }
	BSize PreferredSize() override { return BSize(fDrawWidth, fDrawHeight); }

private:
	void _HandleCopyTarget(BMessage* msg) {
		// Tracker asks us to write data to a file it created.
		entry_ref dir;
		const char* name = nullptr;
		if (msg->FindRef("directory", &dir) != B_OK
		    || msg->FindString("name", &name) != B_OK)
			return;

		BDirectory destDir(&dir);
		BFile file(&destDir, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK) return;

		// Write the raw PNG data.
		file.Write(fPngData.data(), fPngData.size());
		file.SetSize(fPngData.size());

		// Set the MIME type.
		BNodeInfo info(&file);
		info.SetType("image/png");
	}

	BBitmap*    fBitmap;
	std::string fPngData;  // raw PNG bytes for drag-to-save
	float       fDrawWidth;
	float       fDrawHeight;
};

}  // anon namespace

void ConversationView::_ClearInlineImages() {
	// Legacy inline image views removed — images are now shown in the
	// side panel via OnImage delegate callback.
	fInlineImages.clear();
	fImageOffsets.clear();
}

void ConversationView::_InsertInlineImage(const std::string& base64_data) {
	// Decode base64 → raw image bytes → BBitmap via BTranslatorRoster.
	std::string raw = base64_decode(base64_data);
	if (raw.empty()) {
		_Append("\xf0\x9f\x96\xbc [image decode failed]\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		return;
	}

	BMemoryIO memIO(raw.data(), raw.size());
	BBitmap* bitmap = NULL;
	{
		BTranslatorRoster* roster = BTranslatorRoster::Default();
		BBitmapStream stream;
		if (roster->Translate(&memIO, NULL, NULL, &stream,
				B_TRANSLATOR_BITMAP) == B_OK) {
			stream.DetachBitmap(&bitmap);
		}
	}

	if (bitmap != NULL) {
		// Scale bitmap to fit the view width (max 400px, cap height 300px).
		BRect bounds = bitmap->Bounds();
		float iw = bounds.Width() + 1;
		float ih = bounds.Height() + 1;
		float maxW = 400.0f;
		if (fDocView) {
			float viewW = fDocView->Bounds().Width() - 20;
			if (viewW > 50 && viewW < maxW) maxW = viewW;
		}
		float scale = (iw > maxW) ? maxW / iw : 1.0f;
		float dw = iw * scale;
		float dh = ih * scale;
		if (dh > 300) { dh = 300; dw = iw * (300.0f / ih); }

		// Create a scaled copy for inline display.
		BBitmap* scaled = new BBitmap(BRect(0, 0, dw - 1, dh - 1),
		                              B_RGBA32, true);
		if (scaled->Lock()) {
			BView canvas(scaled->Bounds(), "", 0, 0);
			scaled->AddChild(&canvas);
			canvas.SetDrawingMode(B_OP_ALPHA);
			canvas.DrawBitmap(bitmap, bitmap->Bounds(), canvas.Bounds());
			canvas.Sync();
			scaled->RemoveChild(&canvas);
			scaled->Unlock();
		}

		// Insert as a TextSpan image in the document model — participates
		// in text flow, no overlap with surrounding paragraphs.
		if (fTextDoc) {
			Paragraph imgPara;
			TextSpan imgSpan;
			imgSpan.SetImage(scaled, BString("image"));
			imgPara.Append(imgSpan);
			fTextDoc->Append(imgPara);

			// Add an empty paragraph after for spacing.
			Paragraph spacer;
			spacer.Append(TextSpan(" ", CharacterStyle()));
			fTextDoc->Append(spacer);

			if (fDocView) fDocView->Relayout();
		} else {
			delete scaled;
		}
		delete bitmap;  // original no longer needed, scaled copy is in doc
	} else {
		_Append("\xf0\x9f\x96\xbc [image — could not render]\n", ui_color(B_DOCUMENT_TEXT_COLOR));
	}

	// Also send to side panel for full-size preview.
	if (fDelegate)
		fDelegate->OnImage(base64_data, "image/png");
}

void ConversationView::_RepositionInlineImages() {
	// No-op with TextDocumentView (images in side panel).
}

}  // namespace gui
}  // namespace pluto
