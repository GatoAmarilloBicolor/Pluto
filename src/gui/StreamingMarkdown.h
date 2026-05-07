#pragma once

#include <functional>
#include <string>

namespace pluto {
namespace gui {

// Buffers streaming text chunks and emits complete lines.
// Manages fenced code block state (``` open/close).
//
// Usage:
//   StreamingMarkdown sm;
//   sm.on_line = [](const string& line) { render(line); };
//   sm.on_code_block = [](const string& code, const string& lang) { highlight(code, lang); };
//   sm.feed("partial ");
//   sm.feed("line\nnext line\n");
//   sm.flush();  // at end of turn
//   sm.reset();  // before next turn
//
class StreamingMarkdown {
public:
	// Called for each complete non-code line (without trailing \n).
	std::function<void(const std::string& line)> on_line;

	// Called when a fenced code block closes (or is force-flushed).
	// `code` contains all lines (each prefixed with "  ", newline-terminated).
	// `lang` is the language hint from the opening fence (may be empty).
	std::function<void(const std::string& code, const std::string& lang)> on_code_block;

	// Called for a partial line at flush (no trailing \n).
	// If in a code block, on_code_block is called instead.
	std::function<void(const std::string& partial)> on_partial;

	// Feed a streaming chunk. Complete lines are emitted immediately.
	void feed(const std::string& chunk);

	// Flush remaining buffered content (call at end of turn).
	void flush();

	// Reset all state for a new turn.
	void reset();

	// State queries.
	bool in_code_block() const { return fInCodeBlock; }
	const std::string& line_buffer() const { return fLineBuf; }
	const std::string& code_lang() const { return fCodeLang; }
	const std::string& code_accum() const { return fCodeAccum; }

private:
	void _process_line(const std::string& line);

	std::string fLineBuf;
	bool        fInCodeBlock = false;
	std::string fCodeLang;
	std::string fCodeAccum;
};

}  // namespace gui
}  // namespace pluto
