#include "repl.h"

#include "agent/Loop.h"

#include <cstdio>
#include <string>
#include <vector>

namespace pluto {

namespace {

const char* kReset = "\x1b[0m";
const char* kDim   = "\x1b[2m";
const char* kBold  = "\x1b[1m";
const char* kCyan  = "\x1b[36m";
const char* kGreen = "\x1b[32m";
const char* kRed   = "\x1b[31m";
const char* kMag   = "\x1b[35m";

bool read_line(std::string& out) {
	out.clear();
	char buf[4096];
	if (!std::fgets(buf, sizeof(buf), stdin)) return false;
	out = buf;
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
		out.pop_back();
	return true;
}

std::string first_line(const std::string& s) {
	size_t nl = s.find('\n');
	return nl == std::string::npos ? s : s.substr(0, nl);
}

void cmd_tools(const ToolRegistry& tools) {
	for (const Tool* t : tools.list()) {
		std::string desc = first_line(t->description());
		if (desc.size() > 80) { desc.resize(77); desc += "..."; }
		std::printf("  %s%-24s%s  %s\n",
			kBold, t->name().c_str(), kReset, desc.c_str());
	}
}

}  // namespace

int run_repl(Provider& provider, Config& cfg, ToolRegistry& tools, bool yolo) {
	std::printf("%spluto%s %s(%s / %s%s%s)  %zu tools%s  type /help or /quit\n",
		kBold, kReset, kDim, provider.name().c_str(), kBold, cfg.model.c_str(),
		kDim, tools.list().size(), kReset);
	if (yolo) std::printf("%s(yolo: tool calls auto-approved)%s\n", kRed, kReset);

	std::vector<Message> messages;
	std::string line;

	long long session_input_tokens = 0;
	long long session_output_tokens = 0;
	int turns = 0;

	UiCallbacks ui;
	ui.on_text = [](const std::string& text) {
		std::printf("%s", kGreen);
		std::fwrite(text.data(), 1, text.size(), stdout);
		std::printf("%s", kReset);
		std::fflush(stdout);
	};
	ui.on_tool_start = [](const std::string&, const std::string& preview) {
		std::printf("\n%s◆ %s%s\n", kMag, preview.c_str(), kReset);
	};
	ui.on_tool_result = [](const std::string&, const ToolResult& r) {
		const char* tag = r.is_error ? kRed : kDim;
		std::string out = r.output;
		if (out.size() > 500) out = out.substr(0, 500) + "\n... [truncated]";
		std::printf("%s", tag);
		size_t start = 0;
		while (start < out.size()) {
			size_t nl = out.find('\n', start);
			size_t end = (nl == std::string::npos) ? out.size() : nl;
			std::printf("  %s\n", out.substr(start, end - start).c_str());
			start = (nl == std::string::npos) ? out.size() : nl + 1;
		}
		std::printf("%s", kReset);
	};
	ui.on_error = [](const std::string& err) {
		std::printf("%s[error] %s%s\n", kRed, err.c_str(), kReset);
	};
	ui.on_usage = [&](int in_tok, int out_tok) {
		session_input_tokens  += in_tok;
		session_output_tokens += out_tok;
		++turns;
	};

	PermissionFn perm = yolo ? auto_allow() : interactive_prompt();

	for (;;) {
		std::printf("%s> %s", kCyan, kReset);
		std::fflush(stdout);
		if (!read_line(line)) { std::printf("\n"); break; }
		if (line.empty()) continue;

		if (line == "/quit" || line == "/exit") break;
		if (line == "/help") {
			std::printf("  /quit            exit\n");
			std::printf("  /reset           clear conversation history\n");
			std::printf("  /tools           list registered tools\n");
			std::printf("  /model ID        switch model for subsequent turns\n");
			std::printf("  /cost            show session token totals\n");
			continue;
		}
		if (line == "/reset") {
			messages.clear();
			std::printf("%s(history cleared)%s\n", kDim, kReset);
			continue;
		}
		if (line == "/tools") {
			cmd_tools(tools);
			continue;
		}
		if (line == "/cost") {
			std::printf("  turns:  %d\n", turns);
			std::printf("  input:  %lld tokens\n", session_input_tokens);
			std::printf("  output: %lld tokens\n", session_output_tokens);
			std::printf("  total:  %lld tokens\n",
				session_input_tokens + session_output_tokens);
			continue;
		}
		if (line.rfind("/model", 0) == 0) {
			if (line.size() <= 7) {
				std::printf("  current: %s\n", cfg.model.c_str());
			} else {
				cfg.model = line.substr(7);
				// trim leading space
				while (!cfg.model.empty() && cfg.model.front() == ' ')
					cfg.model.erase(0, 1);
				std::printf("  model: %s\n", cfg.model.c_str());
			}
			continue;
		}

		Message user;
		user.role = "user";
		ContentBlock tb;
		tb.type = ContentBlock::TEXT;
		tb.text = line;
		user.content.push_back(std::move(tb));
		messages.push_back(std::move(user));

		run_turn(provider, cfg, tools, messages, perm, ui);
		std::printf("\n");
	}
	return 0;
}

}  // namespace pluto
