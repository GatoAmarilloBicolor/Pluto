#pragma once

#include <string>

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
//   - Inline: **bold**, *italic*, `code`
//   - Unordered lists (- / * )
//   - Ordered lists (1. )
//   - Blockquotes (> )
//   - Horizontal rules (---, ***, ___)

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

	// Render a single complete line (without trailing \n).
	std::string _RenderLine(const std::string& line);

	// Apply inline formatting (**bold**, *italic*, `code`).
	std::string _RenderInline(const std::string& text);
};

}  // namespace pluto
