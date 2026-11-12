#include "ConversationView.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <Clipboard.h>
#include <DataIO.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <Notification.h>
#include <OS.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <Storage.h>
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
	  fTools(tools), fDelegate(delegate),
	  fControlTextColor(ui_color(B_CONTROL_TEXT_COLOR)),
	  fDocTextColor(ui_color(B_DOCUMENT_TEXT_COLOR)),
	  fPanelBgColor(ui_color(B_PANEL_BACKGROUND_COLOR)),
	  fErrorColor({245, 101, 101, 255}),
	  fWarningColor({255, 180, 0, 255})
{
	// Create the markdown-native document view.
	fDocView = new TextDocumentView("output");
	fDocView->SetEditingEnabled(false);
	fDocView->SetInsets(10, 10, 10, 10);
	fTextDoc.SetTo(new TextDocument(), true);
	fDocView->SetTextDocument(fTextDoc);

	// Set background color from config color scheme.
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

	fOutputScroll = new BScrollView("outputScroll", fDocView,
	                                 0, false, true, B_FANCY_BORDER);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fOutputScroll)
		.End();

	fStreamingMd.on_line = [this](const std::string& line) {
		_RenderAssistantLine(line);
	};
	fStreamingMd.on_code_block = [this](const std::string& code,
	                                     const std::string& lang) {
		_FlushMdCodeBlockImpl(code, lang);
	};
	fStreamingMd.on_partial = [this](const std::string& partial) {
		if (!partial.empty())
			_Append(partial, kColAsstBody);
	};
}

void ConversationView::AttachedToWindow() {
	BView::AttachedToWindow();
	if (!fCursorRunner) {
		BMessage tick(kMsgCursorTick);
		fCursorRunner = new BMessageRunner(BMessenger(this), &tick, 500000);
	}
	if (fDocView)
		fDocView->SetTarget(this);
}

void ConversationView::FrameResized(float w, float h) {
	BView::FrameResized(w, h);
	_RepositionInlineImages();
}

ConversationView::~ConversationView() {
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

	size_t pos = 0;
	while (pos < text.size()) {
		size_t nl = text.find('\n', pos);
		if (nl == std::string::npos) {
			BString chunk(text.c_str() + pos, text.size() - pos);
			fCurrentPara.Append(TextSpan(chunk, style));
			break;
		}
		if (nl > pos) {
			BString chunk(text.c_str() + pos, nl - pos);
			fCurrentPara.Append(TextSpan(chunk, style));
		}
		if (fCurrentPara.IsEmpty()) {
			CharacterStyle dimStyle;
			dimStyle.SetFont(*be_plain_font);
			fCurrentPara.Append(TextSpan(" ", dimStyle));
		}
		fTextDoc->Append(fCurrentPara);
		ParagraphStyle pstyle;
		pstyle.SetSpacingTop(0);
		pstyle.SetSpacingBottom(0);
		fCurrentPara = Paragraph(pstyle);
		pos = nl + 1;
	}

	fDocView->SetTextDocument(fTextDoc);
	fDocView->Relayout();

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
	if (fTextDoc.IsSet() && fTextDoc->CountParagraphs() > 2) {
		CharacterStyle hrStyle;
		rgb_color sepColor = fDocTextColor;
		sepColor.alpha = 100;
		hrStyle.SetForegroundColor(sepColor);
		BFont hrFont = *be_plain_font;
		hrFont.SetSize(4);
		hrStyle.SetFont(hrFont);
		_Append("\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 "
		        "\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 "
		        "\xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7 \xC2\xB7\n",
		        fDocTextColor, 0, false);
		_Append("\n", fDocTextColor, 0, false);
	}
	_Append("\xF0\x9F\x91\xA4 You\n", fControlTextColor, B_BOLD_FACE);
}

void ConversationView::_EmitAssistantHeader() {
	fAsstHeaderOff = 0;

	_Append("\n", fDocTextColor, 0, false);

	fAsstHeaderPara = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

	ToolBlock block;
	block.collapsed = false;
	block.is_error = false;
	block.header_para = fAsstHeaderPara;
	fPendingAsstBlock = fToolBlocks.add(block);

	BFont font = *be_plain_font;
	font.SetFace(B_BOLD_FACE);
	if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
	CharacterStyle style;
	style.SetFont(font);
	style.SetForegroundColor(fControlTextColor);

	BMessage* clickMsg = new BMessage(kMsgToolToggle);
	clickMsg->AddInt32("block_idx", (int32)fPendingAsstBlock);

	std::string arrow_text;
	arrow_text += kArrowOpen;
	arrow_text += " \xE2\x9C\xA8 Pluto";

	TextSpan span(BString(arrow_text.c_str()), style);
	span.SetClickMessage(clickMsg);
	fCurrentPara.Append(span);
	_Append("\n", fControlTextColor);

	fAsstBodyOff = 0;
}

// ---- finalize assistant block ----

void ConversationView::_FinalizeAssistantBlock() {
	if (fAsstHeaderOff < 0 || fAsstBodyOff < 0) return;

	if (fPendingAsstBlock >= 0 && fTextDoc.IsSet()) {
		auto* b = fToolBlocks.at(fPendingAsstBlock);
		if (b) {
			b->header_para_count = fAsstDocParaStart - b->header_para;
			if (b->header_para_count <= 0)
				b->header_para_count = 1;
			b->body_para = fAsstDocParaStart;
			b->body_para_count = fTextDoc->CountParagraphs() - fAsstDocParaStart;
			b->body_text = fAsstMarkdownBuf;

			if (fAsstMarkdownBuf.size() <= 400)
				b->body_para_count = 0;
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
		if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
			size_t end = line.find("**", i + 2);
			if (end != std::string::npos) {
				_Append(line.substr(i + 2, end - i - 2),
					kColAsstBody, B_BOLD_FACE, false);
				i = end + 2;
				continue;
			}
		}
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
		if (line[i] == '`') {
			size_t end = line.find('`', i + 1);
			if (end != std::string::npos) {
				_Append(line.substr(i + 1, end - i - 1),
					fPanelBgColor, 0, true);
				i = end + 1;
				continue;
			}
		}
		if (line[i] == '[') {
			size_t close = line.find(']', i + 1);
			if (close != std::string::npos && close + 1 < line.size()
			    && line[close + 1] == '(') {
				size_t pclose = line.find(')', close + 2);
				if (pclose != std::string::npos) {
					std::string label = line.substr(i + 1, close - i - 1);
					_Append(label, fControlTextColor, B_UNDERSCORE_FACE, false);
					i = pclose + 1;
					continue;
				}
			}
		}
		if (i + 1 < line.size() && line[i] == '~' && line[i + 1] == '~') {
			size_t end = line.find("~~", i + 2);
			if (end != std::string::npos) {
				_Append(line.substr(i + 2, end - i - 2),
					fDocTextColor, B_STRIKEOUT_FACE, false);
				i = end + 2;
				continue;
			}
		}
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
		fDocTextColor, 0, true);
	auto syn_tokens = highlight(code, lang);
	for (const auto& tok : syn_tokens) {
		rgb_color tc = fPanelBgColor;
		switch (tok.type) {
			case TokenType::Keyword:      tc = fControlTextColor; break;
			case TokenType::String:       tc = fControlTextColor; break;
			case TokenType::Comment:      tc = fDocTextColor; break;
			case TokenType::Number:       tc = fControlTextColor; break;
			case TokenType::Preprocessor: tc = fControlTextColor; break;
			case TokenType::Type:         tc = fControlTextColor; break;
			default: break;
		}
		_Append(tok.text, tc, 0, true);
	}
	_Append("\n", kColAsstBody);
}

// ---- single line renderer ----

void ConversationView::_RenderAssistantLine(const std::string& line) {
	if (!line.empty() && line[0] == '#') {
		int level = 0;
		while (level < (int)line.size() && line[level] == '#') ++level;
		if (level <= 6 && (level >= (int)line.size() || line[level] == ' ')) {
			std::string heading = line.substr(level);
			while (!heading.empty() && heading.front() == ' ')
				heading.erase(0, 1);
			_Append("\n", kColAsstBody);
			_Append(heading + "\n", fControlTextColor, B_BOLD_FACE);
			if (level <= 2) {
				size_t ul = heading.size();
				if (ul > 40) ul = 40;
				std::string underline;
				underline.reserve(ul);
				char ch = (level == 1) ? '\xe2\x94\x81' : '\xe2\x94\x80';
				for (size_t j = 0; j < ul; ++j)
					underline += ch;
				_Append(underline + "\n", fDocTextColor, 0, true);
			}
			_Append("\n", kColAsstBody);
			return;
		}
	}

	if (line.size() >= 3 && (line.find("---") == 0 || line.find("***") == 0)) {
		bool is_rule = true;
		for (char c : line)
			if (c != '-' && c != '*' && c != ' ')
				{ is_rule = false; break; }
		if (is_rule) {
			_Append("\n  \xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
			        "\xE2\x94\x80\n\n", fDocTextColor, 0, true);
			return;
		}
	}

	if (!line.empty() && line[0] == '>') {
		std::string q = line.substr(1);
		while (!q.empty() && q.front() == ' ') q.erase(0, 1);
		_Append("   \xE2\x94\x83 ", fControlTextColor, 0, false);
		_Append(q, fDocTextColor, B_ITALIC_FACE, false);
		_Append("\n", fDocTextColor);
		return;
	}

	if (line.size() >= 5 && line[0] == '-' && line[1] == ' '
	    && line[2] == '[') {
		bool checked = (line[3] == 'x' || line[3] == 'X');
		std::string item = line.substr(5);
		while (!item.empty() && item.front() == ' ') item.erase(0, 1);
		const char* box = checked
			? "  \xe2\x98\x91 "    // ☑
			: "  \xe2\x98\x90 ";   // ☐
		_Append(box, checked ? fControlTextColor : fDocTextColor);
		if (checked)
			_Append(item, fDocTextColor, B_STRIKEOUT_FACE);
		else
			_RenderAssistantInline(item);
		_Append("\n", kColAsstBody);
		return;
	}

	if (line.size() >= 2 && (line[0] == '-' || line[0] == '*')
	    && line[1] == ' ') {
		_Append("   \xE2\x80\xA2 ", fDocTextColor);
		_RenderAssistantInline(line.substr(2));
		_Append("\n", kColAsstBody);
		return;
	}

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

	if (line.empty()) {
		_Append("\n", kColAsstBody);
		return;
	}

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

void ConversationView::_ShowCursor() {
	if (fCursorShown) return;
	if (!fCurrentPara.IsEmpty() && fTextDoc.IsSet()) {
		fTextDoc->Append(fCurrentPara);
		ParagraphStyle pstyle;
		pstyle.SetSpacingTop(0);
		pstyle.SetSpacingBottom(0);
		fCurrentPara = Paragraph(pstyle);
	}
	BFont font = *be_plain_font;
	font.SetFace(B_BOLD_FACE);
	if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
	CharacterStyle style;
	style.SetFont(font);
	style.SetForegroundColor(fControlTextColor);
	Paragraph cursorPara;
	cursorPara.Append(TextSpan(kCursorChar, style));
	fTextDoc->Append(cursorPara);
	fDocView->SetTextDocument(fTextDoc);
	fDocView->Relayout();
	if (fOutputScroll) {
		BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
		if (sb) { float mn, mx; sb->GetRange(&mn, &mx); sb->SetValue(mx); }
	}
	fCursorShown = true;
}

void ConversationView::_HideCursor() {
	if (!fCursorShown) return;
	if (fTextDoc.IsSet()) {
		int32 count = fTextDoc->CountParagraphs();
		if (count > 0) {
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
	if (fThinking && fBusy) {
		static const char* kFrames[] = {
			"\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8",
			"\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7",
			"\xE2\xA0\x87", "\xE2\xA0\x8F"
		};

		BFont font = *be_plain_font;
		font.SetFace(B_ITALIC_FACE);
		if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
		CharacterStyle style;
		style.SetFont(font);
		style.SetForegroundColor(fDocTextColor);

		BString text;
		text.SetToFormat("%s thinking\xE2\x80\xA6", kFrames[fSpinnerFrame % 10]);

		if (fCursorShown && fTextDoc.IsSet()) {
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

	if (!fTextOngoing) return;
	if (fCursorShown)
		_HideCursor();
	else
		_ShowCursor();
}

// ---- render all messages (used after load) ----

void ConversationView::_RenderMessages() {
	_ClearInlineImages();
	fTextDoc.SetTo(new TextDocument(), true);
	fDocView->SetTextDocument(fTextDoc);

	BString header;
	header.SetToFormat("(loaded %ld turn%s)\n\n",
		(long)fMessages.size(), fMessages.size() == 1 ? "" : "s");
	_Append(header.String(), fDocTextColor);

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
					_InsertInlineImage(b.image_data, b.image_media_type, "");
				else {
					std::string label = "\xf0\x9f\x96\xbc [";
					label += b.image_media_type.empty() ? "image" : b.image_media_type;
					label += "]\n";
					_Append(label, fDocTextColor, B_ITALIC_FACE);
				}
			} else if (b.type == ContentBlock::AUDIO) {
				std::string label = "\xf0\x9f\x94\x8a [audio: ";
				label += b.audio_format.empty() ? "?" : b.audio_format;
				label += ", ";
				label += std::to_string(b.audio_data.size() / 1370);
				label += " KB]\n";
				_Append(label, fDocTextColor, B_ITALIC_FACE);
			} else if (b.type == ContentBlock::DOCUMENT) {
				if (!b.doc_data.empty())
					_InsertInlineDocument(b.doc_data, b.doc_media_type, b.filename);
				else {
					std::string label = "\xf0\x9f\x93\x84 [PDF: ";
					label += std::to_string(b.doc_data.size() / 1370);
					label += " KB]\n";
					_Append(label, fDocTextColor, B_ITALIC_FACE);
				}
			} else if (b.type == ContentBlock::TOOL_RESULT) {
					std::string body = b.text;
					if (body.size() > 600)
						body = body.substr(0, 600) + "\n\xe2\x80\xa6";
					body += "\n";
					_Append(body, b.is_error ? fErrorColor : fPanelBgColor, 0, true);
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
					_InsertInlineImage(b.image_data, b.image_media_type, "");
				else {
					std::string label = "\xf0\x9f\x96\xbc [";
					label += b.image_media_type.empty() ? "image" : b.image_media_type;
					label += "]\n";
					_Append(label, fDocTextColor, B_ITALIC_FACE);
				}
			} else if (b.type == ContentBlock::AUDIO) {
				_Append("\xf0\x9f\x94\x8a [audio attached]\n", fDocTextColor, B_ITALIC_FACE);
			} else if (b.type == ContentBlock::DOCUMENT) {
				if (!b.doc_data.empty())
					_InsertInlineDocument(b.doc_data, b.doc_media_type, b.filename);
				else
					_Append("\xf0\x9f\x93\x84 [PDF attached]\n", fDocTextColor, B_ITALIC_FACE);
			} else if (b.type == ContentBlock::TOOL_USE) {
				std::string head = "\n\xE2\x97\x86 ";
				head += b.tool_name;
				head += "\n";
				_Append(head, fControlTextColor, B_BOLD_FACE);
			}
		}
		_Append("\n", fDocTextColor);
	}
}
}

// ---- worker event handlers ----

bool ConversationView::HandleTextDelta(const std::string& text) {
	bool first = false;
	if (!fTextOngoing) {
		if (fThinking) {
			fThinking = false;
			_HideCursor();
		} else {
			_HideCursor();
		}
		_Append("\n", kColAsstBody);
		_EmitAssistantHeader();
		fTextOngoing = true;
		fAsstMarkdownBuf.clear();
		fAsstDocParaStart = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : 0;
		first = true;
	}
	_HideCursor();

	fAsstMarkdownBuf += text;

	if (fTextDoc.IsSet()) {
		int32 paraCount = fTextDoc->CountParagraphs();
		if (paraCount > fAsstDocParaStart) {
			int32 offset = 0;
			for (int32 i = 0; i < fAsstDocParaStart; ++i)
				offset += fTextDoc->ParagraphAtIndex(i).Length();
			int32 totalLen = fTextDoc->Length();
			if (totalLen > offset)
				fTextDoc->Remove(offset, totalLen - offset);
		}

		BString mdText(fAsstMarkdownBuf.c_str());
		fMdParser.AppendMarkdown(fTextDoc, mdText);
		fDocView->SetTextDocument(fTextDoc);
		fDocView->Relayout();

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

	block.header_para = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

	_Append("\n", fControlTextColor);

	BFont font = *be_plain_font;
	font.SetFace(B_BOLD_FACE);
	if (fFontDelta != 0) font.SetSize(font.Size() + fFontDelta);
	CharacterStyle style;
	style.SetFont(font);
	style.SetForegroundColor(fControlTextColor);

	BMessage* clickMsg = new BMessage(kMsgToolToggle);
	clickMsg->AddInt32("block_idx", (int32)fToolBlocks.count());

	std::string arrow_text;
	arrow_text += kArrowOpen;
	arrow_text += " \xF0\x9F\x94\xA7 ";
	arrow_text += preview;

	TextSpan span(BString(arrow_text.c_str()), style);
	span.SetClickMessage(clickMsg);
	fCurrentPara.Append(span);
	_Append("\n", fControlTextColor);

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

		b.body_para = fTextDoc.IsSet() ? fTextDoc->CountParagraphs() : -1;

		_Append(display, is_error ? fErrorColor : fPanelBgColor, 0, true);

		if (fTextDoc.IsSet() && b.body_para >= 0)
			b.body_para_count = fTextDoc->CountParagraphs() - b.body_para;

		if (display.size() > 400 && fTextDoc.IsSet()) {
			fToolBlocks.collapse(fPendingTool, fTextDoc);
			fDocView->SetTextDocument(fTextDoc);
			fDocView->Relayout();
		}

		fPendingTool = -1;
	} else {
		_Append(display, is_error ? fErrorColor : fPanelBgColor, 0, true);
	}
	std::string tool = fCurrentTool;
	fCurrentTool.clear();
	return tool;
}

void ConversationView::HandleError(const std::string& error) {
	fTextOngoing = false;
	fShimmerStart = -1;
	_HideCursor();
	std::string line = "\n\xE2\x9A\xA0 ";
	line += error;
	line += "\n";
	_Append(line, fErrorColor, B_BOLD_FACE);
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
	_Append("\n", fDocTextColor);
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
		_Append("\xe2\x9a\xa0 ", fWarningColor, B_BOLD_FACE);
		_Append(status + "\n", fWarningColor);
	} else {
		std::string line = "[retry] ";
		line += status;
		line += "\n";
		_Append(line, fDocTextColor);
	}
}

void ConversationView::HandleRagHint(const std::string& hint) {
	_Append(hint, fDocTextColor);
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

	auto gui_permission = [target](const Tool& tool,
	                               const std::string& preview) -> Permission {
		char sem_name[64];
		snprintf(sem_name, sizeof(sem_name), "pluto-approval-%d",
			(int)getpid());
		sem_id sem = create_sem(0, sem_name);
		if (sem < 0) {
			return Permission::Deny;
		}
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
			return Permission::Deny;
		}
		constexpr bigtime_t kApprovalTimeout = 5 * 60 * 1000000LL;
		status_t err = acquire_sem_etc(sem, 1, B_TIMEOUT, kApprovalTimeout);
		if (err == B_TIMED_OUT || err != B_OK) {
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
	ui.on_document = [target](const std::string& base64_data,
	                       const std::string& media_type,
	                       const std::string& filename) {
		BMessage m(kMsgWorkerText);
		std::string label = "\xf0\x9f\x93\x84 [" + filename + "]\n";
		m.AddString("text", label.c_str());
		target.SendMessage(&m);
		BMessage dm(kMsgWorkerDocument);
		dm.AddString("data", base64_data.c_str());
		dm.AddString("media_type", media_type.c_str());
		dm.AddString("filename", filename.c_str());
		target.SendMessage(&dm);
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
				_InsertInlineImage(data, mt ? mt : "");
				if (fDelegate)
					fDelegate->OnImage(data, mt ? mt : "");
			}
			break;
		}

		case kMsgWorkerDocument: {
			const char* data = "";
			const char* mt = "";
			const char* fn = "";
			msg->FindString("data", &data);
			msg->FindString("media_type", &mt);
			msg->FindString("filename", &fn);
			if (data && *data) {
				_InsertInlineDocument(data, mt ? mt : "", fn ? fn : "");
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
				} else if (u.find("://") != std::string::npos) {
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
		_Append("[error] " + error + "\n", fErrorColor);
		return;
	}

	fMessages = std::move(msgs);
	fToolBlocks.clear();
	fSessionPath = path;
	fSessionTitle = meta.title;
	fTokensIn = meta.tokens_in;
	fTokensOut = meta.tokens_out;
	fTurnCount = meta.turns;

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
		_Append("[error] save failed: " + error + "\n", fErrorColor);
		return;
	}

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

	if (!fMessages.empty()) {
		_RenderMessages();
	}
}

void ConversationView::UpdateParserColors(bool useSystem,
                                           rgb_color codeFg, rgb_color codeBg,
                                           rgb_color linkColor) {
	fMdParser.UpdateColors(useSystem, codeFg, codeBg, linkColor);
}

void ConversationView::RestoreScrollPosition(float position) {
	if (!fOutputScroll) return;
	BScrollBar* sb = fOutputScroll->ScrollBar(B_VERTICAL);
	if (sb) {
		// Clamp to valid range
		if (position < 0) position = 0;
		sb->SetValue(position);
	}
}

std::string ConversationView::BuildMarkdown() const {
	std::string out;
	out.reserve(1024);
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

	std::string q(query);
	for (auto& c : q) c = static_cast<char>(
		std::tolower(static_cast<unsigned char>(c)));

	std::string text(docText.String(), len);
	for (auto& c : text) c = static_cast<char>(
		std::tolower(static_cast<unsigned char>(c)));

	size_t pos = text.find(q, fFindOffset);
	if (pos == std::string::npos && fFindOffset > 0)
		pos = text.find(q);
	if (pos == std::string::npos) return;

	if (fDocView) {
		fDocView->SetCaret(BPoint(0, 0), false);
	}
	fFindOffset = static_cast<int32>(pos + 1);
}

// ---- inline image support ----

namespace {

class InlineImageView : public BView {
public:
	InlineImageView(BBitmap* bitmap, float maxWidth,
	                const std::string& pngData,
	                const std::string& mimeType = "image/png",
	                const std::string& originalName = "")
		: BView("inlineImage", B_WILL_DRAW | B_NAVIGABLE), fBitmap(bitmap),
		  fPngData(pngData), fMimeType(mimeType), fOriginalName(originalName),
		  fZoom(1.0f), fDragOffset(0, 0), fDragging(false)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		BRect br = fBitmap->Bounds();
		float iw = br.Width() + 1;
		float ih = br.Height() + 1;
		float scale = (iw > maxWidth && maxWidth > 10) ? maxWidth / iw : 1.0f;
		fDrawWidth = iw * scale;
		fDrawHeight = ih * scale;
		fOriginalWidth = iw;
		fOriginalHeight = ih;
		if (fDrawHeight > 400) {
			fDrawHeight = 400;
			fDrawWidth = iw * (400.0f / ih);
		}
		fZoom = std::min(fDrawWidth / iw, fDrawHeight / ih);
	}

	~InlineImageView() override { delete fBitmap; }

	void Draw(BRect /*updateRect*/) override {
		if (!fBitmap) return;
		BRect src = fBitmap->Bounds();
		BRect dst(0, 0, fDrawWidth * fZoom - 1, fDrawHeight * fZoom - 1);
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fBitmap, src, dst);
		
		// Draw zoom indicator in corner
		if (fZoom != 1.0f) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%.0f%%", fZoom * 100);
			BFont font = *be_plain_font;
			font.SetSize(9);
			SetFont(&font);
			SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
			DrawString(buf, BPoint(4, 14));
		}
		
		// Draw filename if available
		if (!fOriginalName.empty() && fZoom >= 1.0f) {
			BFont font = *be_plain_font;
			font.SetSize(9);
			SetFont(&font);
			SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
			DrawString(fOriginalName.c_str(), BPoint(4, fDrawHeight * fZoom - 4));
		}
	}

	void MouseDown(BPoint where) override {
		if (!fBitmap) return;
		uint32 buttons = 0;
		GetMouse(&where, &buttons);
		
		if (buttons & B_SECONDARY_MOUSE_BUTTON) {
			_ShowContextMenu(where);
			return;
		}
		
		if (!(buttons & B_PRIMARY_MOUSE_BUTTON)) return;

		// Check for double-click to zoom
		if (Window() && Window()->CurrentMessage()) {
			bigtime_t clickTime = Window()->CurrentMessage()->GetInt64("when", 0);
			bigtime_t now = system_time();
			if (now - fLastClickTime < 500000 && fZoom < 2.0f) {  // Double-click
				_SetZoom(std::min(fZoom * 2.0f, 3.0f));
				fLastClickTime = now;
				return;
			}
			fLastClickTime = now;
		}

		// Start drag for pan (when zoomed)
		if (fZoom > 1.0f) {
			fDragOffset = where;
			fDragging = true;
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS | B_NO_POINTER_HISTORY);
			return;
		}

		// Normal drag for copy
		BMessage drag(B_SIMPLE_DATA);
		drag.AddInt32("be:actions", B_COPY_TARGET);
		drag.AddString("be:types", "application/octet-stream");
		drag.AddString("be:filetypes", fMimeType.c_str());
		drag.AddString("be:type_descriptions", "Image file");
		drag.AddString("be:clip_name", fOriginalName.empty() ? "pluto_image.png" : fOriginalName.c_str());

		float thumbW = std::min(fDrawWidth * fZoom, 100.0f);
		float thumbH = fDrawHeight * fZoom * (thumbW / (fDrawWidth * fZoom));
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
		this->DragMessage(&drag, thumb, B_OP_ALPHA, BPoint(thumbW / 2, thumbH / 2));
	}

	void MouseUp(BPoint where) override {
		fDragging = false;
	}

	void MouseMoved(BPoint where, uint32 transit, const BMessage* msg) override {
		if (fDragging && fZoom > 1.0f) {
			BPoint delta = where - fDragOffset;
			fDragOffset = where;
			// Pan the image (would need scroll view integration)
			// For now, just update and redraw
			Invalidate();
		}
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == B_COPY_TARGET) {
			_HandleCopyTarget(msg);
		} else if (msg->what == 'imgZ') {  // Zoom in
			_SetZoom(std::min(fZoom * 1.5f, 4.0f));
		} else if (msg->what == 'imgZo') {  // Zoom out
			_SetZoom(std::max(fZoom / 1.5f, 0.25f));
		} else if (msg->what == 'imgR') {  // Reset zoom
			_SetZoom(1.0f);
		} else if (msg->what == 'imgS') {  // Save
			_HandleSaveAs();
		} else if (msg->what == 'imgC') {  // Copy
			_HandleCopy();
		} else if (msg->what == 'imgO') {  // Open externally
			_HandleOpenExternally();
		} else if (msg->what == 'imgF') {  // Full screen
			_HandleFullScreen();
		} else {
			BView::MessageReceived(msg);
		}
	}

	void KeyDown(const char* bytes, int32 numBytes) override {
		if (numBytes == 1 && bytes[0] == '+') {
			_SetZoom(std::min(fZoom * 1.25f, 4.0f));
		} else if (numBytes == 1 && bytes[0] == '-') {
			_SetZoom(std::max(fZoom / 1.25f, 0.25f));
		} else if (numBytes == 1 && bytes[0] == '0') {
			_SetZoom(1.0f);
		} else {
			BView::KeyDown(bytes, numBytes);
		}
	}

	BSize MinSize() override { return BSize(fDrawWidth, fDrawHeight); }
	BSize MaxSize() override { return BSize(fDrawWidth * 4, fDrawHeight * 4); }
	BSize PreferredSize() override { return BSize(fDrawWidth * fZoom, fDrawHeight * fZoom); }

private:
	void _ShowContextMenu(BPoint where) {
		BPopUpMenu* menu = new BPopUpMenu("image_ctx", false, false);
		
		// Zoom submenu
		BMenu* zoomMenu = new BMenu("Zoom");
		zoomMenu->AddItem(new BMenuItem("Zoom in (+)", new BMessage('imgZ'), '+'));
		zoomMenu->AddItem(new BMenuItem("Zoom out (-)", new BMessage('imgZo'), '-'));
		zoomMenu->AddItem(new BMenuItem("Reset (0)", new BMessage('imgR'), '0'));
		zoomMenu->AddSeparatorItem();
		zoomMenu->AddItem(new BMenuItem("Fit to window", new BMessage('imgF')));
		menu->AddItem(zoomMenu);
		
		menu->AddSeparatorItem();
		menu->AddItem(new BMenuItem("Save as" B_UTF8_ELLIPSIS, new BMessage('imgS')));
		menu->AddItem(new BMenuItem("Copy", new BMessage('imgC'), 'C'));
		menu->AddItem(new BMenuItem("Open externally", new BMessage('imgO')));
		menu->AddSeparatorItem();
		
		// Image info
		char info[128];
		snprintf(info, sizeof(info), "%dx%d %s", 
		         (int)fOriginalWidth, (int)fOriginalHeight, fMimeType.c_str());
		BMenuItem* infoItem = new BMenuItem(info, nullptr);
		infoItem->SetEnabled(false);
		menu->AddItem(infoItem);
		
		if (!fOriginalName.empty()) {
			BMenuItem* nameItem = new BMenuItem(fOriginalName.c_str(), nullptr);
			nameItem->SetEnabled(false);
			menu->AddItem(nameItem);
		}
		
		menu->SetTargetForItems(this);
		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
	}

	void _SetZoom(float zoom) {
		if (zoom < 0.25f) zoom = 0.25f;
		if (zoom > 4.0f) zoom = 4.0f;
		if (fabs(zoom - fZoom) < 0.01f) return;
		
		fZoom = zoom;
		InvalidateLayout();
		Invalidate();
		
		// Notify parent to relayout
		if (Parent()) Parent()->InvalidateLayout();
	}

	void _HandleCopyTarget(BMessage* msg) {
		entry_ref dir;
		const char* name = nullptr;
		if (msg->FindRef("directory", &dir) != B_OK
		    || msg->FindString("name", &name) != B_OK)
			return;

		BDirectory destDir(&dir);
		BFile file(&destDir, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK) return;

		file.Write(fPngData.data(), fPngData.size());
		file.SetSize(fPngData.size());

		BNodeInfo info(&file);
		info.SetType(fMimeType.c_str());
	}

	void _HandleSaveAs() {
		if (!Window()) return;
		
		BFilePanel* panel = new BFilePanel(B_SAVE_PANEL,
		                                   nullptr,  // target
		                                   nullptr,  // directory
		                                   B_FILE_NODE,
		                                   false,    // allow multiple
		                                   nullptr,  // message
		                                   nullptr,  // filter
		                                   true,     // modal
		                                   Window());
		
		// Set default filename
		std::string filename = fOriginalName.empty() ? "pluto_image.png" : fOriginalName;
		// We can't easily set the filename in BFilePanel, so we'll use a custom approach
		
		// For now, save to desktop
		BDirectory homeDir;
		BPath desktopPath;
		if (find_directory(B_DESKTOP_DIRECTORY, &desktopPath) == B_OK) {
			BPath filePath(desktopPath.Path());
			filePath.Append(filename.c_str());
			
			BFile file(filePath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
			if (file.InitCheck() == B_OK) {
				file.Write(fPngData.data(), fPngData.size());
				file.SetSize(fPngData.size());
				BNodeInfo info(&file);
				info.SetType(fMimeType.c_str());
				
				// Show notification
				if (Window()) {
					BNotification n(B_INFORMATION_NOTIFICATION);
					n.SetGroup("Pluto");
					n.SetTitle("Image saved");
					n.SetContent(filePath.Path());
					n.Send(5000000LL);
				}
			}
		}
	}

	void _HandleCopy() {
		if (!Window()) return;
		
		BMessage clip(B_SIMPLE_DATA);
		clip.AddInt32("be:actions", B_COPY_TARGET);
		clip.AddString("be:types", "application/octet-stream");
		clip.AddString("be:filetypes", fMimeType.c_str());
		clip.AddString("be:type_descriptions", "Image file");
		clip.AddString("be:clip_name", fOriginalName.empty() ? "pluto_image.png" : fOriginalName.c_str());
		
		if (be_clipboard && be_clipboard->Lock()) {
			be_clipboard->Clear();
			be_clipboard->Data()->AddData("application/octet-stream", B_MIME_TYPE, 
			                              fPngData.data(), fPngData.size());
			be_clipboard->Commit();
			be_clipboard->Unlock();
		}
	}

	void _HandleOpenExternally() {
		// Save to temp file and open with default handler
		BPath tempPath;
		if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath) != B_OK) return;
		
		std::string filename = fOriginalName.empty() ? "pluto_image.png" : fOriginalName;
		tempPath.Append(filename.c_str());
		
		BFile file(tempPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK) return;
		file.Write(fPngData.data(), fPngData.size());
		file.SetSize(fPngData.size());
		BNodeInfo info(&file);
		info.SetType(fMimeType.c_str());
		
		entry_ref ref;
		if (get_ref_for_path(tempPath.Path(), &ref) == B_OK) {
			be_roster->Launch(&ref);
		}
	}

	void _HandleFullScreen() {
		// Create a full-screen window with the image
		if (!Window()) return;
		
		BRect screenFrame;
		if (BScreen* screen = new BScreen(Window())) {
			screenFrame = screen->Frame();
			delete screen;
		}
		
		BRect winFrame(0, 0, 
		               std::min(fOriginalWidth, (float)(screenFrame.Width() * 0.9)),
		               std::min(fOriginalHeight, (float)(screenFrame.Height() * 0.9)));
		winFrame.OffsetTo((screenFrame.Width() - winFrame.Width()) / 2,
		                  (screenFrame.Height() - winFrame.Height()) / 2);
		
		BWindow* fullWin = new BWindow(winFrame, "Image Viewer", B_TITLED_WINDOW,
		                               B_ASYNCHRONOUS_CONTROLS | B_NOT_RESIZABLE);
		
		// Create a scroll view with the full-size image
		BBitmap* fullBitmap = new BBitmap(fBitmap->Bounds(), fBitmap->ColorSpace(), true);
		if (fullBitmap->Lock()) {
			fullBitmap->ImportBits(fBitmap);
			fullBitmap->Unlock();
		}
		
		// Create image view with full resolution
		BView* imgView = new BView("fullImage", B_WILL_DRAW);
		imgView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		
		class FullImageView : public BView {
		public:
			FullImageView(BBitmap* bmp) : BView("fullImg", B_WILL_DRAW), fBitmap(bmp) {}
			~FullImageView() override { delete fBitmap; }
			void Draw(BRect) override {
				if (fBitmap) DrawBitmap(fBitmap, BPoint(0, 0));
			}
			BSize MinSize() override { 
				return fBitmap ? BSize(fBitmap->Bounds().Width()+1, fBitmap->Bounds().Height()+1) : BSize(100, 100); 
			}
			BSize MaxSize() override { return MinSize(); }
			BSize PreferredSize() override { return MinSize(); }
		private:
			BBitmap* fBitmap;
		};
		
		FullImageView* fullImg = new FullImageView(fullBitmap);
		BScrollView* scroll = new BScrollView("fullScroll", fullImg, 0, true, true, B_FANCY_BORDER);
		
		fullWin->AddChild(scroll);
		fullWin->CenterOnScreen();
		fullWin->Show();
	}

	BBitmap*    fBitmap;
	std::string fPngData;
	std::string fMimeType;
	std::string fOriginalName;
	float       fDrawWidth;
	float       fDrawHeight;
	float       fOriginalWidth;
	float       fOriginalHeight;
	float       fZoom;
	BPoint      fDragOffset;
	bool        fDragging;
	bigtime_t   fLastClickTime = 0;
};

}  // anon namespace

// ---- inline PDF/document support ----

namespace {

class InlineDocumentView : public BView {
public:
	InlineDocumentView(const std::string& docData, const std::string& mimeType,
	                   const std::string& filename, float maxWidth)
		: BView("inlineDocument", B_WILL_DRAW | B_NAVIGABLE),
		  fDocData(docData), fMimeType(mimeType), fFilename(filename),
		  fMaxWidth(maxWidth), fPageCount(0), fCurrentPage(0)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		_ParseDocument();
	}

	~InlineDocumentView() override {
		delete fFirstPageBitmap;
	}

	void Draw(BRect /*updateRect*/) override {
		if (fFirstPageBitmap) {
			BRect src = fFirstPageBitmap->Bounds();
			BRect dst(0, 0, fDrawWidth - 1, fDrawHeight - 1);
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(fFirstPageBitmap, src, dst);
			
			// Draw page indicator
			if (fPageCount > 1) {
				char buf[64];
				snprintf(buf, sizeof(buf), "Page %d of %d", fCurrentPage + 1, fPageCount);
				BFont font = *be_plain_font;
				font.SetSize(10);
				font.SetFace(B_BOLD_FACE);
				SetFont(&font);
				SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
				DrawString(buf, BPoint(8, 20));
			}
		} else {
			// Fallback: draw document icon and info
			_DrawDocumentPlaceholder();
		}
		
		// Draw filename
		if (!fFilename.empty()) {
			BFont font = *be_plain_font;
			font.SetSize(10);
			SetFont(&font);
			SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
			float y = fFirstPageBitmap ? fDrawHeight + 20 : 60;
			DrawString(fFilename.c_str(), BPoint(8, y));
		}
	}

	void MouseDown(BPoint where) override {
		uint32 buttons = 0;
		GetMouse(&where, &buttons);
		
		if (buttons & B_SECONDARY_MOUSE_BUTTON) {
			_ShowContextMenu(where);
			return;
		}
		
		if (!(buttons & B_PRIMARY_MOUSE_BUTTON)) return;

		// Double-click to open externally
		if (Window() && Window()->CurrentMessage()) {
			bigtime_t clickTime = Window()->CurrentMessage()->GetInt64("when", 0);
			bigtime_t now = system_time();
			if (now - fLastClickTime < 500000) {
				_HandleOpenExternally();
				fLastClickTime = now;
				return;
			}
			fLastClickTime = now;
		}

		// Single click - cycle pages if multi-page
		if (fPageCount > 1) {
			fCurrentPage = (fCurrentPage + 1) % fPageCount;
			_RenderPage(fCurrentPage);
			Invalidate();
		}
	}

	void KeyDown(const char* bytes, int32 numBytes) override {
		if (numBytes == 1 && bytes[0] == B_RIGHT_ARROW && fPageCount > 1) {
			fCurrentPage = (fCurrentPage + 1) % fPageCount;
			_RenderPage(fCurrentPage);
			Invalidate();
		} else if (numBytes == 1 && bytes[0] == B_LEFT_ARROW && fPageCount > 1) {
			fCurrentPage = (fCurrentPage - 1 + fPageCount) % fPageCount;
			_RenderPage(fCurrentPage);
			Invalidate();
		} else if (numBytes == 1 && bytes[0] == B_ENTER) {
			_HandleOpenExternally();
		} else {
			BView::KeyDown(bytes, numBytes);
		}
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == B_COPY_TARGET) {
			_HandleCopyTarget(msg);
		} else if (msg->what == 'docS') {  // Save
			_HandleSaveAs();
		} else if (msg->what == 'docC') {  // Copy
			_HandleCopy();
		} else if (msg->what == 'docO') {  // Open externally
			_HandleOpenExternally();
		} else if (msg->what == 'docE') {  // Extract text
			_HandleExtractText();
		} else {
			BView::MessageReceived(msg);
		}
	}

	BSize MinSize() override { return BSize(fDrawWidth, fDrawHeight + 40); }
	BSize MaxSize() override { return BSize(fMaxWidth, fDrawHeight + 40); }
	BSize PreferredSize() override { return BSize(fDrawWidth, fDrawHeight + 40); }

private:
	void _ParseDocument() {
		if (fDocData.empty()) return;
		
		// For PDF, we'd use a PDF renderer. For now, create a placeholder.
		// In a real implementation, we'd use PDFium, Poppler, or Haiku's PDF translator.
		
		// Try to use Haiku's translator to get first page as bitmap
		BMemoryIO memIO(fDocData.data(), fDocData.size());
		BTranslatorRoster* roster = BTranslatorRoster::Default();
		BBitmapStream stream;
		
		// Try to translate as image (some PDF translators support this)
		if (roster->Translate(&memIO, NULL, NULL, &stream, B_TRANSLATOR_BITMAP) == B_OK) {
			stream.DetachBitmap(&fFirstPageBitmap);
			if (fFirstPageBitmap) {
				BRect bounds = fFirstPageBitmap->Bounds();
				float iw = bounds.Width() + 1;
				float ih = bounds.Height() + 1;
				float scale = (iw > fMaxWidth && fMaxWidth > 10) ? fMaxWidth / iw : 1.0f;
				fDrawWidth = iw * scale;
				fDrawHeight = ih * scale;
				if (fDrawHeight > 500) {
					fDrawHeight = 500;
					fDrawWidth = iw * (500.0f / ih);
				}
			}
		}
		
		// If no bitmap, use default placeholder size
		if (!fFirstPageBitmap) {
			fDrawWidth = std::min(400.0f, fMaxWidth);
			fDrawHeight = 300;
		}
		
		fPageCount = 1;  // Would be parsed from PDF in real implementation
	}

	void _RenderPage(int page) {
		// In real implementation, re-render the specific page
		fCurrentPage = page;
		Invalidate();
	}

	void _DrawDocumentPlaceholder() {
		// Draw a document icon with info
		BRect bounds = Bounds();
		float cx = bounds.Width() / 2;
		float cy = bounds.Height() / 2;
		
		// Document icon background
		SetHighColor({200, 220, 255, 255});
		SetDrawingMode(B_OP_ALPHA);
		FillRoundRect(BRect(cx - 60, cy - 80, cx + 60, cy + 20), 8, 8);
		
		// Document icon lines
		SetHighColor({60, 100, 180, 255});
		SetPenSize(2);
		StrokeLine(BPoint(cx - 40, cy - 60), BPoint(cx + 40, cy - 60));
		StrokeLine(BPoint(cx - 40, cy - 30), BPoint(cx + 40, cy - 30));
		StrokeLine(BPoint(cx - 40, cy), BPoint(cx + 10, cy));
		StrokeLine(BPoint(cx - 40, cy + 10), BPoint(cx - 10, cy + 10));
		
		// Document type label
		BFont font = *be_plain_font;
		font.SetSize(12);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);
		SetHighColor({40, 60, 120, 255});
		
		std::string type = _GetDocumentTypeLabel();
		float tw = font.StringWidth(type.c_str());
		DrawString(type.c_str(), BPoint(cx - tw / 2, cy + 45));
		
		// Size info
		font.SetSize(10);
		font.SetFace(0);
		SetFont(&font);
		char sizeInfo[64];
		snprintf(sizeInfo, sizeof(sizeInfo), "%.1f KB", fDocData.size() / 1024.0);
		tw = font.StringWidth(sizeInfo);
		DrawString(sizeInfo, BPoint(cx - tw / 2, cy + 65));
	}

	std::string _GetDocumentTypeLabel() const {
		if (fMimeType.find("pdf") != std::string::npos) return "PDF Document";
		if (fMimeType.find("word") != std::string::npos || 
		    fMimeType.find("docx") != std::string::npos) return "Word Document";
		if (fMimeType.find("spreadsheet") != std::string::npos || 
		    fMimeType.find("xlsx") != std::string::npos) return "Spreadsheet";
		if (fMimeType.find("text") != std::string::npos) return "Text Document";
		return "Document";
	}

	void _ShowContextMenu(BPoint where) {
		BPopUpMenu* menu = new BPopUpMenu("doc_ctx", false, false);
		
		if (fPageCount > 1) {
			menu->AddItem(new BMenuItem("Next page (Right)", nullptr, B_RIGHT_ARROW));
			menu->AddItem(new BMenuItem("Previous page (Left)", nullptr, B_LEFT_ARROW));
			menu->AddSeparatorItem();
		}
		
		menu->AddItem(new BMenuItem("Save as" B_UTF8_ELLIPSIS, new BMessage('docS')));
		menu->AddItem(new BMenuItem("Copy", new BMessage('docC'), 'C'));
		menu->AddItem(new BMenuItem("Open externally", new BMessage('docO')));
		menu->AddSeparatorItem();
		menu->AddItem(new BMenuItem("Extract text" B_UTF8_ELLIPSIS, new BMessage('docE')));
		
		menu->AddSeparatorItem();
		
		// Document info
		char info[128];
		snprintf(info, sizeof(info), "%s  |  %d page(s)  |  %.1f KB",
		         _GetDocumentTypeLabel().c_str(), fPageCount, fDocData.size() / 1024.0);
		BMenuItem* infoItem = new BMenuItem(info, nullptr);
		infoItem->SetEnabled(false);
		menu->AddItem(infoItem);
		
		if (!fFilename.empty()) {
			BMenuItem* nameItem = new BMenuItem(fFilename.c_str(), nullptr);
			nameItem->SetEnabled(false);
			menu->AddItem(nameItem);
		}
		
		menu->SetTargetForItems(this);
		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
	}

	void _HandleCopyTarget(BMessage* msg) {
		entry_ref dir;
		const char* name = nullptr;
		if (msg->FindRef("directory", &dir) != B_OK
		    || msg->FindString("name", &name) != B_OK)
			return;

		BDirectory destDir(&dir);
		BFile file(&destDir, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK) return;

		file.Write(fDocData.data(), fDocData.size());
		file.SetSize(fDocData.size());

		BNodeInfo info(&file);
		info.SetType(fMimeType.c_str());
	}

	void _HandleSaveAs() {
		if (!Window()) return;
		
		BPath desktopPath;
		if (find_directory(B_DESKTOP_DIRECTORY, &desktopPath) != B_OK) return;
		
		std::string filename = fFilename.empty() ? "pluto_document.pdf" : fFilename;
		BPath filePath(desktopPath.Path());
		filePath.Append(filename.c_str());
		
		BFile file(filePath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() == B_OK) {
			file.Write(fDocData.data(), fDocData.size());
			file.SetSize(fDocData.size());
			BNodeInfo info(&file);
			info.SetType(fMimeType.c_str());
			
			if (Window()) {
				BNotification n(B_INFORMATION_NOTIFICATION);
				n.SetGroup("Pluto");
				n.SetTitle("Document saved");
				n.SetContent(filePath.Path());
				n.Send(5000000LL);
			}
		}
	}

	void _HandleCopy() {
		if (!Window()) return;
		
		if (be_clipboard && be_clipboard->Lock()) {
			be_clipboard->Clear();
			be_clipboard->Data()->AddData(fMimeType.c_str(), B_MIME_TYPE, 
			                              fDocData.data(), fDocData.size());
			be_clipboard->Commit();
			be_clipboard->Unlock();
		}
	}

	void _HandleOpenExternally() {
		BPath tempPath;
		if (find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath) != B_OK) return;
		
		std::string filename = fFilename.empty() ? "pluto_document.pdf" : fFilename;
		tempPath.Append(filename.c_str());
		
		BFile file(tempPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK) return;
		file.Write(fDocData.data(), fDocData.size());
		file.SetSize(fDocData.size());
		BNodeInfo info(&file);
		info.SetType(fMimeType.c_str());
		
		entry_ref ref;
		if (get_ref_for_path(tempPath.Path(), &ref) == B_OK) {
			be_roster->Launch(&ref);
		}
	}

	void _HandleExtractText() {
		// In a real implementation, extract text from PDF
		// For now, show a notification
		if (Window()) {
			BNotification n(B_INFORMATION_NOTIFICATION);
			n.SetGroup("Pluto");
			n.SetTitle("Text extraction");
			n.SetContent("Text extraction requires PDF parsing library (not yet implemented)");
			n.Send(5000000LL);
		}
	}

	std::string fDocData;
	std::string fMimeType;
	std::string fFilename;
	float       fMaxWidth;
	float       fDrawWidth;
	float       fDrawHeight;
	int         fPageCount;
	int         fCurrentPage;
	BBitmap*    fFirstPageBitmap = nullptr;
	bigtime_t   fLastClickTime = 0;
};

}  // anon namespace

void ConversationView::_ClearInlineImages() {
	fInlineImages.clear();
	fImageOffsets.clear();
}

void ConversationView::_InsertInlineImage(const std::string& base64_data,
                                         const std::string& mimeType,
                                         const std::string& filename) {
	std::string raw = base64_decode(base64_data);
	if (raw.empty()) {
		_Append("\xf0\x9f\x96\xbc [image decode failed]\n", fDocTextColor);
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
		BRect bounds = bitmap->Bounds();
		float iw = bounds.Width() + 1;
		float ih = bounds.Height() + 1;
		float maxW = 500.0f;  // Increased max width
		if (fDocView) {
			float viewW = fDocView->Bounds().Width() - 20;
			if (viewW > 50 && viewW < maxW) maxW = viewW;
		}
		float scale = (iw > maxW) ? maxW / iw : 1.0f;
		float dw = iw * scale;
		float dh = ih * scale;
		if (dh > 400) { dh = 400; dw = iw * (400.0f / ih); }

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

		if (fTextDoc) {
			Paragraph imgPara;
			TextSpan imgSpan;
			imgSpan.SetImage(scaled, BString("image"));
			imgPara.Append(imgSpan);
			fTextDoc->Append(imgPara);

			Paragraph spacer;
			spacer.Append(TextSpan(" ", CharacterStyle()));
			fTextDoc->Append(spacer);

			if (fDocView) fDocView->Relayout();
		} else {
			delete scaled;
		}
		delete bitmap;
	} else {
		_Append("\xf0\x9f\x96\xbc [image \xe2\x80\x94 could not render]\n", fDocTextColor);
	}

	if (fDelegate)
		fDelegate->OnImage(base64_data, mimeType);
}

void ConversationView::_InsertInlineDocument(const std::string& base64_data, 
                                            const std::string& mimeType,
                                            const std::string& filename) {
	std::string raw = base64_decode(base64_data);
	if (raw.empty()) {
		_Append("\xf0\x9f\x93\x84 [document decode failed]\n", fDocTextColor);
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
		BRect bounds = bitmap->Bounds();
		float iw = bounds.Width() + 1;
		float ih = bounds.Height() + 1;
		float maxW = 500.0f;
		if (fDocView) {
			float viewW = fDocView->Bounds().Width() - 20;
			if (viewW > 50 && viewW < maxW) maxW = viewW;
		}
		float scale = (iw > maxW) ? maxW / iw : 1.0f;
		float dw = iw * scale;
		float dh = ih * scale;
		if (dh > 400) { dh = 400; dw = iw * (400.0f / ih); }

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

		if (fTextDoc) {
			Paragraph docPara;
			TextSpan docSpan;
			docSpan.SetImage(scaled, BString(filename.empty() ? "document" : filename.c_str()));
			docPara.Append(docSpan);
			fTextDoc->Append(docPara);

			Paragraph spacer;
			spacer.Append(TextSpan(" ", CharacterStyle()));
			fTextDoc->Append(spacer);

			if (fDocView) fDocView->Relayout();
		} else {
			delete scaled;
		}
		delete bitmap;
	} else {
		_Append("\xf0\x9f\x93\x84 [document \xe2\x80\x94 could not render]\n", fDocTextColor);
	}
}

void ConversationView::_RepositionInlineImages() {
}

}  // namespace gui
}  // namespace pluto