#pragma once

#include <string>
#include <vector>

namespace pluto {

// Streaming terminal markdown renderer.
//
// Feed() accepts incremental text chunks (as received from the LLM
// streaming callback) and returns ANSI-formatted output ready to be
// written to stdout.  Complete lines are formatted immediately; a
// partial trailing line is held in an internal buffer until the next
// newline arrives or Flush() is called.
//
// Supported markdown:
//   - Fenced code blocks (``` with optional language tag)
//   - ATX headers (# through ######)
//   - Inline: **bold**, *italic*, `code`, ~~strikethrough~~, [text](url)
//   - Unordered lists (- / * ), nested by indent
//   - Ordered lists (1. )
//   - Blockquotes (> )
//   - Horizontal rules (---, ***, ___)
//   - Tables (| a | b |, with a --- separator row)

class TerminalMarkdown {
public:
	// Feed a streaming chunk.  Returns formatted text to print.
	std::string Feed(const std::string& chunk);

	// Flush any remaining buffered text (call at end of turn).
	std::string Flush();

	// Reset internal state for a new turn.
	void Reset();

private:
	std::string fLineBuf;
	bool fInCodeBlock = false;

	// Raw lines that look like table rows, held until the table ends
	// (a non-table line arrives, or the stream is flushed) so column
	// widths can be computed across the whole table.
	std::vector<std::string> fTableBuf;

	// Process one complete line: either render it directly into `out`,
	// or buffer it as a candidate table row. `add_newline` is false for
	// the trailing partial line handed to Flush(), which never had a
	// terminating '\n' in the source text.
	void _ProcessLine(const std::string& line, std::string& out,
	                   bool add_newline = true);

	// Render a single complete line (without trailing \n).
	std::string _RenderLine(const std::string& line);

	// Render a buffered run of table rows as an aligned box table.
	std::string _RenderTable(const std::vector<std::string>& rows);

	// Apply inline formatting (**bold**, *italic*, `code`, ~~strike~~, links).
	std::string _RenderInline(const std::string& text);
};

}  // namespace pluto
