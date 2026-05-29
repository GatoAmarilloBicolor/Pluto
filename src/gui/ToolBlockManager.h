#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "textview/TextDocument.h"

namespace pluto {
namespace gui {

struct ToolBlock {
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

// Manages a list of collapsible ToolBlocks using TextDocument paragraphs.
class ToolBlockManager {
public:
	void clear() { fBlocks.clear(); }
	int  count() const { return (int)fBlocks.size(); }

	// Add a new block. Returns its index.
	int add(const ToolBlock& block);

	// Access a block by index (nullptr if out of range).
	ToolBlock*       at(int idx);
	const ToolBlock* at(int idx) const;

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
