#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "textview/TextDocument.h"

class BTextView;

namespace pluto {
namespace gui {

struct ToolBlock {
	// Legacy BTextView offsets (kept for backward compat with tests).
	int32_t     header_offset = 0;
	int32_t     header_len    = 0;
	int32_t     body_offset   = 0;
	int32_t     body_len      = 0;
	std::string body_text;
	bool        is_error      = false;
	bool        collapsed     = false;

	// TextDocument paragraph indices.
	int32       header_para   = -1;  // first paragraph of the header
	int32       header_para_count = 0;
	int32       body_para     = -1;  // first paragraph of the body
	int32       body_para_count = 0;

	// Saved body paragraphs (stored on collapse, restored on expand).
	std::vector<Paragraph> saved_body;
};

// Manages a list of collapsible ToolBlocks.
// Supports both BTextView (legacy) and TextDocument (new) modes.
class ToolBlockManager {
public:
	void clear() { fBlocks.clear(); }
	int  count() const { return (int)fBlocks.size(); }

	// Add a new block. Returns its index.
	int add(const ToolBlock& block);

	// Access a block by index (nullptr if out of range).
	ToolBlock*       at(int idx);
	const ToolBlock* at(int idx) const;

	// --- Legacy BTextView mode ---
	using ArrowFn = void(*)(BTextView* tv, int32_t offset,
	                        const char* arrow, void* ctx);
	void collapse(int idx, BTextView* tv, ArrowFn fn, void* ctx);
	void expand(int idx, BTextView* tv, ArrowFn fn, void* ctx);
	void toggle(int idx, BTextView* tv, ArrowFn fn, void* ctx);
	int hit_test(int32_t offset) const;

	// --- TextDocument paragraph mode ---

	// Collapse: remove body paragraphs from the document, save them.
	// Updates the arrow glyph in the header paragraph.
	void collapse(int idx, TextDocumentRef doc);

	// Expand: re-insert saved body paragraphs into the document.
	void expand(int idx, TextDocumentRef doc);

	// Toggle collapse/expand.
	void toggle(int idx, TextDocumentRef doc);

	// Hit-test by paragraph index. Returns block index or -1.
	int hit_test_para(int32 para_index) const;

	// Direct access to the underlying vector (for iteration).
	std::vector<ToolBlock>&       blocks()       { return fBlocks; }
	const std::vector<ToolBlock>& blocks() const { return fBlocks; }

private:
	// Adjust paragraph indices of all blocks after a shift.
	void _AdjustParaIndices(int skip_idx, int32 from_para, int32 delta);

	std::vector<ToolBlock> fBlocks;
};

}  // namespace gui
}  // namespace pluto
