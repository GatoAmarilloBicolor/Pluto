#include "ToolBlockManager.h"

#include <TextView.h>

namespace pluto {
namespace gui {

int ToolBlockManager::add(const ToolBlock& block) {
	fBlocks.push_back(block);
	return (int)fBlocks.size() - 1;
}

ToolBlock* ToolBlockManager::at(int idx) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return nullptr;
	return &fBlocks[idx];
}

const ToolBlock* ToolBlockManager::at(int idx) const {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return nullptr;
	return &fBlocks[idx];
}

// --- Legacy BTextView mode ---

void ToolBlockManager::collapse(int idx, BTextView* tv,
                                ArrowFn fn, void* ctx) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	auto& b = fBlocks[idx];
	if (b.collapsed) return;

	if (b.body_len > 0) {
		int32_t body_end = b.body_offset + b.body_len;
		int32_t deleted = b.body_len;
		tv->Delete(b.body_offset, body_end);

		for (size_t i = 0; i < fBlocks.size(); ++i) {
			if ((int)i == idx) continue;
			auto& o = fBlocks[i];
			if (o.header_offset >= body_end) o.header_offset -= deleted;
			if (o.body_offset   >= body_end) o.body_offset   -= deleted;
		}
		b.body_len = 0;
	}

	if (fn) fn(tv, b.header_offset + 1, "\xE2\x96\xB8", ctx);  // ▸
	b.collapsed = true;
}

void ToolBlockManager::expand(int idx, BTextView* tv,
                              ArrowFn fn, void* ctx) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	auto& b = fBlocks[idx];
	if (!b.collapsed) return;

	int32_t ins_pos = b.body_offset;
	int32_t ins_len = (int32_t)b.body_text.size();
	if (ins_len > 0) {
		tv->Insert(ins_pos, b.body_text.data(), ins_len);

		for (size_t i = 0; i < fBlocks.size(); ++i) {
			if ((int)i == idx) continue;
			auto& o = fBlocks[i];
			if (o.header_offset >= ins_pos) o.header_offset += ins_len;
			if (o.body_offset   >= ins_pos) o.body_offset   += ins_len;
		}
		b.body_len = ins_len;
	}

	if (fn) fn(tv, b.header_offset + 1, "\xE2\x96\xBE", ctx);  // ▾
	b.collapsed = false;
}

void ToolBlockManager::toggle(int idx, BTextView* tv,
                              ArrowFn fn, void* ctx) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	if (fBlocks[idx].collapsed) expand(idx, tv, fn, ctx);
	else                        collapse(idx, tv, fn, ctx);
}

int ToolBlockManager::hit_test(int32_t offset) const {
	for (size_t i = 0; i < fBlocks.size(); ++i) {
		const auto& b = fBlocks[i];
		if (offset >= b.header_offset
		    && offset < b.header_offset + b.header_len) {
			return (int)i;
		}
	}
	return -1;
}

// --- TextDocument paragraph mode ---

void ToolBlockManager::_AdjustParaIndices(int skip_idx, int32 from_para,
                                           int32 delta) {
	for (size_t i = 0; i < fBlocks.size(); ++i) {
		if ((int)i == skip_idx) continue;
		auto& o = fBlocks[i];
		if (o.header_para >= from_para) o.header_para += delta;
		if (o.body_para >= from_para)   o.body_para += delta;
	}
}

void ToolBlockManager::collapse(int idx, TextDocumentRef doc) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	auto& b = fBlocks[idx];
	if (b.collapsed || !doc.IsSet()) return;
	if (b.body_para < 0 || b.body_para_count <= 0) {
		b.collapsed = true;
		return;
	}

	// Save body paragraphs before removing.
	b.saved_body.clear();
	for (int32 i = 0; i < b.body_para_count; ++i) {
		int32 pi = b.body_para + i;
		if (pi < doc->CountParagraphs())
			b.saved_body.push_back(doc->ParagraphAtIndex(pi));
	}

	// Calculate text offset of the body paragraphs to remove.
	int32 offset = 0;
	for (int32 i = 0; i < b.body_para; ++i)
		offset += doc->ParagraphAtIndex(i).Length();
	int32 len = 0;
	for (int32 i = 0; i < b.body_para_count; ++i) {
		int32 pi = b.body_para + i;
		if (pi < doc->CountParagraphs())
			len += doc->ParagraphAtIndex(pi).Length();
	}
	if (len > 0)
		doc->Remove(offset, len);

	// Adjust downstream block paragraph indices.
	_AdjustParaIndices(idx, b.body_para, -b.body_para_count);

	// Swap arrow in header paragraph: ▾ → ▸
	if (b.header_para >= 0 && b.header_para < doc->CountParagraphs()) {
		// Find the arrow glyph in the header paragraph and replace it.
		const Paragraph& hdr = doc->ParagraphAtIndex(b.header_para);
		BString text = hdr.Text();
		int32 arrowPos = text.FindFirst("\xE2\x96\xBE");  // ▾
		if (arrowPos >= 0) {
			int32 hdrOffset = 0;
			for (int32 i = 0; i < b.header_para; ++i)
				hdrOffset += doc->ParagraphAtIndex(i).Length();
			doc->Replace(hdrOffset + arrowPos, 3,
				BString("\xE2\x96\xB8"));  // ▸
		}
	}

	b.collapsed = true;
}

void ToolBlockManager::expand(int idx, TextDocumentRef doc) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	auto& b = fBlocks[idx];
	if (!b.collapsed || !doc.IsSet()) return;
	if (b.saved_body.empty()) {
		b.collapsed = false;
		return;
	}

	// Calculate insertion point (just after header paragraphs).
	int32 ins_para = b.body_para;

	// Re-insert saved body paragraphs at the original position.
	// We insert by calculating the text offset at that paragraph boundary.
	int32 offset = 0;
	int32 paraCount = doc->CountParagraphs();
	for (int32 i = 0; i < ins_para && i < paraCount; ++i)
		offset += doc->ParagraphAtIndex(i).Length();

	// Build the text to insert from saved paragraphs.
	for (int32 i = (int32)b.saved_body.size() - 1; i >= 0; --i) {
		// Insert each paragraph. We insert from back to front so offsets
		// stay valid. Use the paragraph's own text and style.
		const Paragraph& p = b.saved_body[i];
		BString text = p.Text();
		if (text.Length() > 0) {
			// Use first span's style for the insert.
			CharacterStyle cs;
			if (p.CountTextSpans() > 0)
				cs = p.TextSpanAtIndex(0).Style();
			doc->Insert(offset, text, cs, p.Style());
		}
	}

	int32 restored = (int32)b.saved_body.size();
	b.body_para_count = restored;

	// Adjust downstream block paragraph indices.
	_AdjustParaIndices(idx, b.body_para, restored);

	// Swap arrow: ▸ → ▾
	if (b.header_para >= 0 && b.header_para < doc->CountParagraphs()) {
		const Paragraph& hdr = doc->ParagraphAtIndex(b.header_para);
		BString text = hdr.Text();
		int32 arrowPos = text.FindFirst("\xE2\x96\xB8");  // ▸
		if (arrowPos >= 0) {
			int32 hdrOffset = 0;
			for (int32 i = 0; i < b.header_para; ++i)
				hdrOffset += doc->ParagraphAtIndex(i).Length();
			doc->Replace(hdrOffset + arrowPos, 3,
				BString("\xE2\x96\xBE"));  // ▾
		}
	}

	b.saved_body.clear();
	b.collapsed = false;
}

void ToolBlockManager::toggle(int idx, TextDocumentRef doc) {
	if (idx < 0 || (size_t)idx >= fBlocks.size()) return;
	if (fBlocks[idx].collapsed) expand(idx, doc);
	else                        collapse(idx, doc);
}

int ToolBlockManager::hit_test_para(int32 para_index) const {
	for (size_t i = 0; i < fBlocks.size(); ++i) {
		const auto& b = fBlocks[i];
		if (b.header_para >= 0
		    && para_index >= b.header_para
		    && para_index < b.header_para + b.header_para_count) {
			return (int)i;
		}
	}
	return -1;
}

}  // namespace gui
}  // namespace pluto
