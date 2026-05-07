#include "StreamingMarkdown.h"

namespace pluto {
namespace gui {

void StreamingMarkdown::feed(const std::string& chunk) {
	fLineBuf += chunk;

	// Process all complete lines (ending with \n).
	size_t pos = 0;
	while (pos < fLineBuf.size()) {
		size_t nl = fLineBuf.find('\n', pos);
		if (nl == std::string::npos) break;

		std::string line = fLineBuf.substr(pos, nl - pos);
		while (!line.empty() && line.back() == '\r') line.pop_back();
		_process_line(line);
		pos = nl + 1;
	}

	// Keep any remaining partial line in the buffer.
	fLineBuf = fLineBuf.substr(pos);
}

void StreamingMarkdown::flush() {
	// Render any partial last line.
	if (!fLineBuf.empty()) {
		if (fInCodeBlock) {
			fCodeAccum += "  " + fLineBuf;
			fLineBuf.clear();
			// Flush unclosed code block.
			if (on_code_block) on_code_block(fCodeAccum, fCodeLang);
			fCodeAccum.clear();
			fCodeLang.clear();
			fInCodeBlock = false;
		} else {
			if (on_partial) on_partial(fLineBuf);
			fLineBuf.clear();
		}
	} else if (fInCodeBlock) {
		// Unclosed code block — flush what we have.
		if (on_code_block) on_code_block(fCodeAccum, fCodeLang);
		fCodeAccum.clear();
		fCodeLang.clear();
		fInCodeBlock = false;
	}
}

void StreamingMarkdown::reset() {
	fLineBuf.clear();
	fInCodeBlock = false;
	fCodeLang.clear();
	fCodeAccum.clear();
}

void StreamingMarkdown::_process_line(const std::string& line) {
	// Fenced code block toggle.
	if (line.size() >= 3 && line.substr(0, 3) == "```") {
		if (fInCodeBlock) {
			// Closing fence — emit accumulated code.
			if (on_code_block) on_code_block(fCodeAccum, fCodeLang);
			fCodeAccum.clear();
			fCodeLang.clear();
			fInCodeBlock = false;
		} else {
			// Opening fence — extract language hint.
			fCodeLang = line.size() > 3 ? line.substr(3) : "";
			while (!fCodeLang.empty() && fCodeLang.front() == ' ')
				fCodeLang.erase(0, 1);
			fInCodeBlock = true;
		}
		return;
	}

	if (fInCodeBlock) {
		fCodeAccum += "  " + line + "\n";
		return;
	}

	// Normal line — emit via callback.
	if (on_line) on_line(line);
}

}  // namespace gui
}  // namespace pluto
