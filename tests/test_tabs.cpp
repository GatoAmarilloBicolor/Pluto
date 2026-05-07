// tests/test_tabs.cpp — Multi-tab state isolation tests
//
// Verifies that each ConversationView (= one tab) maintains fully
// independent state: messages, tokens, session metadata, busy flag,
// tool blocks, and rendering offsets.  Two ConversationView instances
// are created with the same provider/config/tools to simulate the
// multi-tab scenario.

#include "gui/ConversationView.h"
#include "gui/ChatColors.h"
#include "gui/textview/TextDocument.h"

#include <Application.h>
#include <TextView.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using pluto::Config;
using pluto::ContentBlock;
using pluto::Message;
using pluto::Provider;
using pluto::CompletionRequest;
using pluto::StreamCallback;
using pluto::ToolRegistry;
using pluto::gui::ConversationDelegate;
using pluto::gui::ConversationView;

// ---- test macros (same pattern as the rest of the suite) ----

static int g_pass = 0;
static int g_total = 0;

#define TEST(name) \
	do { ++g_total; std::printf("  %d. %s ... ", g_total, name); } while(0)
#define PASS() do { ++g_pass; std::printf("ok\n"); } while(0)
#define CHECK(cond) do { \
	if (!(cond)) { \
		std::printf("FAIL (%s:%d: %s)\n", __FILE__, __LINE__, #cond); \
		return; \
	} \
} while(0)

// ---- stubs ----

class NullProvider : public Provider {
public:
	void stream(const CompletionRequest&, StreamCallback) override {}
	std::string name() const override { return "null"; }
};

static std::unique_ptr<Provider> make_null() {
	return std::make_unique<NullProvider>();
}

struct MockDelegate : public ConversationDelegate {
	int status_count   = 0;
	int title_count    = 0;
	int turn_start_count = 0;
	int turn_done_count  = 0;
	int tool_output_count = 0;
	int image_count    = 0;
	int rag_count      = 0;
	int provider_status_count = 0;
	std::string last_title;
	std::string last_preview;
	std::string last_activity;

	void OnStatusChanged() override { ++status_count; }
	void OnTitleChanged(const std::string& t) override {
		++title_count; last_title = t;
	}
	void OnTurnStarted() override { ++turn_start_count; }
	void OnTitleActivity(const std::string& a) override { last_activity = a; }
	void OnToolOutput(const std::string&, const std::string&,
	                  const std::string&, bool) override { ++tool_output_count; }
	void OnImage(const std::string&, const std::string&) override { ++image_count; }
	void OnTurnDone(const std::string& p) override {
		++turn_done_count; last_preview = p;
	}
	int RequestToolApproval(const std::string&, const std::string&) override {
		return 1;  // always allow
	}
	void OnRagInject(const std::vector<std::string>&) override { ++rag_count; }
	void OnProviderStatus(const std::string&) override { ++provider_status_count; }
};

// ---- tests ----

// Helper: create a TEXT content block.
static ContentBlock text_block(const std::string& text) {
	ContentBlock b;
	b.type = ContentBlock::TEXT;
	b.text = text;
	return b;
}

void test_initial_state() {
	TEST("initial state is clean");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del;

	ConversationView cv(make_null(), cfg, &tools, &del);

	CHECK(cv.Messages().empty());
	CHECK(!cv.IsBusy());
	CHECK(cv.GetTokensIn() == 0);
	CHECK(cv.GetTokensOut() == 0);
	CHECK(cv.GetTurnCount() == 0);
	CHECK(cv.GetSessionTitle().empty());
	CHECK(cv.GetSessionPath().empty());
	PASS();
}

void test_independent_messages() {
	TEST("tabs have independent message histories");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// Push a message into tab 1 only.
	std::vector<ContentBlock> blocks;
	blocks.push_back(text_block("hello from tab 1"));
	cv1.PrepareTurn(std::move(blocks));

	CHECK(cv1.Messages().size() == 1);
	CHECK(cv2.Messages().empty());

	// Push a different message into tab 2.
	std::vector<ContentBlock> blocks2;
	blocks2.push_back(text_block("hello from tab 2"));
	cv2.PrepareTurn(std::move(blocks2));

	CHECK(cv1.Messages().size() == 1);
	CHECK(cv2.Messages().size() == 1);
	CHECK(cv1.Messages()[0].content[0].text == "hello from tab 1");
	CHECK(cv2.Messages()[0].content[0].text == "hello from tab 2");
	PASS();
}

void test_independent_tokens() {
	TEST("tabs have independent token accounting");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.HandleUsage(100, 50, 10, 5);
	cv2.HandleUsage(200, 80, 20, 10);

	CHECK(cv1.GetTokensIn() == 100);
	CHECK(cv1.GetTokensOut() == 50);
	CHECK(cv1.GetLastCacheRead() == 10);
	CHECK(cv1.GetLastCacheCreate() == 5);
	CHECK(cv1.GetTurnCount() == 1);

	CHECK(cv2.GetTokensIn() == 200);
	CHECK(cv2.GetTokensOut() == 80);
	CHECK(cv2.GetLastCacheRead() == 20);
	CHECK(cv2.GetLastCacheCreate() == 10);
	CHECK(cv2.GetTurnCount() == 1);

	// Accumulate more on tab 1.
	cv1.HandleUsage(50, 25, 0, 0);
	CHECK(cv1.GetTokensIn() == 150);
	CHECK(cv1.GetTurnCount() == 2);
	CHECK(cv2.GetTokensIn() == 200);  // unchanged
	CHECK(cv2.GetTurnCount() == 1);
	PASS();
}

void test_independent_session_title() {
	TEST("tabs have independent session titles");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.SetSessionTitle("Tab 1 title");
	cv2.SetSessionTitle("Tab 2 title");

	CHECK(cv1.GetSessionTitle() == "Tab 1 title");
	CHECK(cv2.GetSessionTitle() == "Tab 2 title");

	// Delegates notified independently.
	CHECK(del1.last_title == "Tab 1 title");
	CHECK(del2.last_title == "Tab 2 title");
	PASS();
}

void test_independent_session_path() {
	TEST("tabs have independent session paths");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.SetSessionPath("/path/to/session1.jsonl");
	cv2.SetSessionPath("/path/to/session2.jsonl");

	CHECK(cv1.GetSessionPath() == "/path/to/session1.jsonl");
	CHECK(cv2.GetSessionPath() == "/path/to/session2.jsonl");
	PASS();
}

void test_independent_busy_flag() {
	TEST("tabs have independent busy flags");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// PrepareTurn sets busy=true.
	std::vector<ContentBlock> blocks;
	blocks.push_back(text_block("query"));
	cv1.PrepareTurn(std::move(blocks));

	CHECK(cv1.IsBusy() == true);
	CHECK(cv2.IsBusy() == false);

	// HandleTurnDone clears busy.
	cv1.HandleTurnDone();
	CHECK(cv1.IsBusy() == false);
	CHECK(cv2.IsBusy() == false);
	PASS();
}

void test_reset_isolates() {
	TEST("reset on one tab does not affect another");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// Populate both tabs.
	std::vector<ContentBlock> b1;
	b1.push_back(text_block("msg 1"));
	cv1.PrepareTurn(std::move(b1));
	cv1.HandleUsage(100, 50, 0, 0);
	cv1.HandleTurnDone();
	cv1.SetSessionTitle("Title 1");

	std::vector<ContentBlock> b2;
	b2.push_back(text_block("msg 2"));
	cv2.PrepareTurn(std::move(b2));
	cv2.HandleUsage(200, 80, 0, 0);
	cv2.HandleTurnDone();
	cv2.SetSessionTitle("Title 2");

	// Reset tab 1.
	cv1.Reset();

	CHECK(cv1.Messages().empty());
	CHECK(cv1.GetTokensIn() == 0);
	CHECK(cv1.GetSessionTitle().empty());

	// Tab 2 is untouched.
	CHECK(cv2.Messages().size() == 1);
	CHECK(cv2.GetTokensIn() == 200);
	CHECK(cv2.GetSessionTitle() == "Title 2");
	PASS();
}

void test_handle_text_delta_independent() {
	TEST("HandleTextDelta produces output in the correct tab");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// First call returns true (new assistant block).
	bool first1 = cv1.HandleTextDelta("Hello ");
	CHECK(first1 == true);

	// Tab 2 hasn't received any text.
	bool first2 = cv2.HandleTextDelta("World ");
	CHECK(first2 == true);

	// Second call on tab 1 returns false (continuing).
	bool second1 = cv1.HandleTextDelta("more text");
	CHECK(second1 == false);
	PASS();
}

void test_handle_tool_start_independent() {
	TEST("HandleToolStart tracks current tool per tab");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.HandleToolStart("read_file", "read_file(path=/etc/hosts)");
	cv2.HandleToolStart("bash", "bash(command=ls)");

	CHECK(cv1.GetCurrentTool() == "read_file");
	CHECK(cv2.GetCurrentTool() == "bash");
	PASS();
}

void test_font_zoom_per_tab() {
	TEST("font zoom applies independently to each tab");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// ApplyFontDelta on an empty text view should not crash.
	cv1.ApplyFontDelta(2);
	cv2.ApplyFontDelta(2);

	// Both output views should exist.
	CHECK(cv1.DocView() != nullptr);
	CHECK(cv2.DocView() != nullptr);
	PASS();
}

void test_estimate_cost_independent() {
	TEST("cost estimate is per-tab");
	Config cfg;
	cfg.model = "claude-opus-4-5";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.HandleUsage(1000, 500, 0, 0);
	cv2.HandleUsage(2000, 1000, 0, 0);

	double cost1 = cv1.EstimateCost();
	double cost2 = cv2.EstimateCost();

	// cost2 should be 2x cost1 since tokens are 2x.
	CHECK(cost2 > cost1 * 1.99);
	CHECK(cost2 < cost1 * 2.01);
	PASS();
}

void test_delegate_callbacks_routed_correctly() {
	TEST("delegate callbacks route to the correct tab's delegate");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.SetSessionTitle("T1");
	CHECK(del1.title_count == 1);
	CHECK(del2.title_count == 0);

	cv2.SetSessionTitle("T2");
	CHECK(del1.title_count == 1);
	CHECK(del2.title_count == 1);
	PASS();
}

void test_handle_error_independent() {
	TEST("HandleError appends to the correct tab only");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.HandleError("connection failed");

	// Tab 1 document should contain the error.
	TextDocumentRef doc1 = cv1.GetTextDocument();
	BString doc1text = doc1.IsSet() ? doc1->Text() : BString();
	std::string text1(doc1text.String(), doc1text.Length());
	CHECK(text1.find("connection failed") != std::string::npos);

	// Tab 2 document should be empty or minimal.
	TextDocumentRef doc2 = cv2.GetTextDocument();
	BString doc2text = doc2.IsSet() ? doc2->Text() : BString();
	CHECK(doc2text.Length() == 0);
	PASS();
}

void test_handle_status_message() {
	TEST("HandleStatusMessage appends to correct tab");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.HandleStatusMessage("HTTP 429, retrying in 2s", false);

	TextDocumentRef doc1 = cv1.GetTextDocument();
	BString doc1text = doc1.IsSet() ? doc1->Text() : BString();
	std::string text1(doc1text.String(), doc1text.Length());
	CHECK(text1.find("retry") != std::string::npos);
	TextDocumentRef doc2 = cv2.GetTextDocument();
	BString doc2text = doc2.IsSet() ? doc2->Text() : BString();
	CHECK(doc2text.Length() == 0);
	PASS();
}

void test_mutable_messages_independent() {
	TEST("MutableMessages returns per-tab reference");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	auto& msgs1 = cv1.MutableMessages();
	auto& msgs2 = cv2.MutableMessages();

	// They should be different vectors.
	CHECK(&msgs1 != &msgs2);

	Message m;
	m.role = "assistant";
	ContentBlock b;
	b.type = ContentBlock::TEXT;
	b.text = "response";
	m.content.push_back(b);
	msgs1.push_back(m);

	CHECK(cv1.Messages().size() == 1);
	CHECK(cv2.Messages().empty());
	PASS();
}

void test_turn_done_clears_state() {
	TEST("HandleTurnDone clears busy and returns preview");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del;

	ConversationView cv(make_null(), cfg, &tools, &del);

	std::vector<ContentBlock> blocks;
	blocks.push_back(text_block("query"));
	cv.PrepareTurn(std::move(blocks));
	CHECK(cv.IsBusy() == true);

	cv.HandleTextDelta("The answer is 42.");
	std::string preview = cv.HandleTurnDone();

	CHECK(cv.IsBusy() == false);
	CHECK(preview == "The answer is 42.");
	CHECK(cv.GetCurrentTool().empty());
	PASS();
}

void test_build_markdown_per_tab() {
	TEST("BuildMarkdown exports from the correct tab");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	std::vector<ContentBlock> b1;
	b1.push_back(text_block("question one"));
	cv1.PrepareTurn(std::move(b1));

	std::vector<ContentBlock> b2;
	b2.push_back(text_block("question two"));
	cv2.PrepareTurn(std::move(b2));

	std::string md1 = cv1.BuildMarkdown();
	std::string md2 = cv2.BuildMarkdown();

	CHECK(md1.find("question one") != std::string::npos);
	CHECK(md1.find("question two") == std::string::npos);
	CHECK(md2.find("question two") != std::string::npos);
	CHECK(md2.find("question one") == std::string::npos);
	PASS();
}

void test_per_tab_model() {
	TEST("each tab has its own model config");
	Config cfg;
	cfg.model = "claude-opus-4-5";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// Both start with the same model.
	CHECK(cv1.GetModel() == "claude-opus-4-5");
	CHECK(cv2.GetModel() == "claude-opus-4-5");

	// Change model on tab 1 only.
	cv1.MutableConfig().model = "claude-sonnet-4-6";
	CHECK(cv1.GetModel() == "claude-sonnet-4-6");
	CHECK(cv2.GetModel() == "claude-opus-4-5");  // unchanged

	// Change model on tab 2 only.
	cv2.MutableConfig().model = "claude-haiku-4-5";
	CHECK(cv1.GetModel() == "claude-sonnet-4-6");  // still
	CHECK(cv2.GetModel() == "claude-haiku-4-5");
	PASS();
}

void test_per_tab_provider() {
	TEST("each tab has its own provider instance");
	Config cfg;
	cfg.model = "test-model";
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	// Providers are different instances.
	CHECK(cv1.GetProvider() != cv2.GetProvider());

	// Replace tab 1's provider.
	auto old_p1 = cv1.GetProvider();
	cv1.SetProvider(make_null());
	CHECK(cv1.GetProvider() != old_p1);
	CHECK(cv2.GetProvider() != cv1.GetProvider());
	PASS();
}

void test_per_tab_context_window() {
	TEST("each tab has its own context_window");
	Config cfg;
	cfg.model = "test-model";
	cfg.context_window = 200000;
	ToolRegistry tools;
	MockDelegate del1, del2;

	ConversationView cv1(make_null(), cfg, &tools, &del1);
	ConversationView cv2(make_null(), cfg, &tools, &del2);

	cv1.MutableConfig().context_window = 100000;
	CHECK(cv1.GetConfig().context_window == 100000);
	CHECK(cv2.GetConfig().context_window == 200000);
	PASS();
}

// ---- main ----

int main() {
	BApplication app("application/x-vnd.pluto-test-tabs");

	std::printf("test_tabs\n");
	test_initial_state();
	test_independent_messages();
	test_independent_tokens();
	test_independent_session_title();
	test_independent_session_path();
	test_independent_busy_flag();
	test_reset_isolates();
	test_handle_text_delta_independent();
	test_handle_tool_start_independent();
	test_font_zoom_per_tab();
	test_estimate_cost_independent();
	test_delegate_callbacks_routed_correctly();
	test_handle_error_independent();
	test_handle_status_message();
	test_mutable_messages_independent();
	test_turn_done_clears_state();
	test_build_markdown_per_tab();
	test_per_tab_model();
	test_per_tab_provider();
	test_per_tab_context_window();
	std::printf("%d/%d passed\n", g_pass, g_total);

	return g_pass == g_total ? 0 : 1;
}
