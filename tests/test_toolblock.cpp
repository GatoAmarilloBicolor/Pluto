// tests/test_toolblock.cpp — ToolBlockManager unit tests
//
// Tests offset management for collapsible tool blocks.
// Uses a real BTextView to verify that collapse/expand
// produce correct text content and offset adjustments.

#include "gui/ToolBlockManager.h"

#include <Application.h>
#include <TextView.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using pluto::gui::ToolBlock;
using pluto::gui::ToolBlockManager;

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

// Dummy arrow callback (does nothing — we test offsets, not styling).
static void noop_arrow(BTextView*, int32_t, const char*, void*) {}

void test_empty() {
	TEST("empty manager");
	ToolBlockManager mgr;
	CHECK(mgr.count() == 0);
	CHECK(mgr.at(0) == nullptr);
	CHECK(mgr.at(-1) == nullptr);
	CHECK(mgr.hit_test(0) == -1);
	PASS();
}

void test_add_and_access() {
	TEST("add and access blocks");
	ToolBlockManager mgr;
	ToolBlock b;
	b.header_offset = 0;
	b.header_len = 10;
	b.body_offset = 10;
	b.body_len = 20;
	b.body_text = "01234567890123456789";

	int idx = mgr.add(b);
	CHECK(idx == 0);
	CHECK(mgr.count() == 1);
	CHECK(mgr.at(0) != nullptr);
	CHECK(mgr.at(0)->header_offset == 0);
	CHECK(mgr.at(0)->body_len == 20);
	CHECK(mgr.at(1) == nullptr);
	PASS();
}

void test_hit_test() {
	TEST("hit_test finds correct block");
	ToolBlockManager mgr;

	ToolBlock b1;
	b1.header_offset = 0;
	b1.header_len = 10;
	b1.body_offset = 10;
	b1.body_len = 20;
	mgr.add(b1);

	ToolBlock b2;
	b2.header_offset = 30;
	b2.header_len = 10;
	b2.body_offset = 40;
	b2.body_len = 15;
	mgr.add(b2);

	CHECK(mgr.hit_test(0) == 0);
	CHECK(mgr.hit_test(5) == 0);
	CHECK(mgr.hit_test(9) == 0);
	CHECK(mgr.hit_test(10) == -1);   // body of block 0
	CHECK(mgr.hit_test(30) == 1);
	CHECK(mgr.hit_test(39) == 1);
	CHECK(mgr.hit_test(40) == -1);   // body of block 1
	CHECK(mgr.hit_test(100) == -1);
	PASS();
}

void test_collapse_single(BTextView* tv) {
	TEST("collapse single block adjusts offsets");
	tv->SetText("");
	// Simulate: "\nHEADER____" (10) + "BODY_TEXT__BODY_TEXT_" (20) = 30 chars
	tv->Insert(0, "\nHEADER____BODY_TEXT__BODY_TEXT_", 30);
	CHECK(tv->TextLength() == 30);

	ToolBlockManager mgr;
	ToolBlock b;
	b.header_offset = 0;
	b.header_len = 10;
	b.body_offset = 10;
	b.body_len = 20;
	b.body_text = "BODY_TEXT__BODY_TEXT_";
	mgr.add(b);

	mgr.collapse(0, tv, noop_arrow, nullptr);
	CHECK(tv->TextLength() == 10);   // body deleted
	CHECK(mgr.at(0)->collapsed == true);
	CHECK(mgr.at(0)->body_len == 0);
	CHECK(mgr.at(0)->body_text == "BODY_TEXT__BODY_TEXT_");  // preserved
	PASS();
}

void test_expand_single(BTextView* tv) {
	TEST("expand restores body text");
	tv->SetText("");
	tv->Insert(0, "\nHEADER____", 10);

	ToolBlockManager mgr;
	ToolBlock b;
	b.header_offset = 0;
	b.header_len = 10;
	b.body_offset = 10;
	b.body_len = 0;
	b.body_text = "BODY_RESTORED_!";
	b.collapsed = true;
	mgr.add(b);

	mgr.expand(0, tv, noop_arrow, nullptr);
	CHECK(tv->TextLength() == 10 + 15);  // header + body
	CHECK(mgr.at(0)->collapsed == false);
	CHECK(mgr.at(0)->body_len == 15);

	// Verify text content.
	std::string text(tv->Text(), tv->TextLength());
	CHECK(text.find("BODY_RESTORED_!") != std::string::npos);
	PASS();
}

void test_toggle(BTextView* tv) {
	TEST("toggle flips collapsed state");
	tv->SetText("");
	tv->Insert(0, "\nHEADER____BODY12345", 19);

	ToolBlockManager mgr;
	ToolBlock b;
	b.header_offset = 0;
	b.header_len = 10;
	b.body_offset = 10;
	b.body_len = 9;
	b.body_text = "BODY12345";
	mgr.add(b);

	CHECK(mgr.at(0)->collapsed == false);
	mgr.toggle(0, tv, noop_arrow, nullptr);
	CHECK(mgr.at(0)->collapsed == true);
	CHECK(tv->TextLength() == 10);
	mgr.toggle(0, tv, noop_arrow, nullptr);
	CHECK(mgr.at(0)->collapsed == false);
	CHECK(tv->TextLength() == 19);
	PASS();
}

void test_two_blocks_collapse_first(BTextView* tv) {
	TEST("collapse first block adjusts second block offsets");
	tv->SetText("");
	// Block 0: header at 0 (len 5), body at 5 (len 10)
	// Block 1: header at 15 (len 5), body at 20 (len 8)
	tv->Insert(0, "HDR0_BODY0_____HDR1_BODY1___", 28);

	ToolBlockManager mgr;
	{
		ToolBlock b;
		b.header_offset = 0; b.header_len = 5;
		b.body_offset = 5;   b.body_len = 10;
		b.body_text = "BODY0_____";
		mgr.add(b);
	}
	{
		ToolBlock b;
		b.header_offset = 15; b.header_len = 5;
		b.body_offset = 20;   b.body_len = 8;
		b.body_text = "BODY1___";
		mgr.add(b);
	}

	// Collapse block 0 — deletes 10 bytes at offset 5.
	// Block 1 should shift: header 15→5, body 20→10.
	mgr.collapse(0, tv, noop_arrow, nullptr);
	CHECK(tv->TextLength() == 18);  // 28 - 10
	CHECK(mgr.at(0)->collapsed == true);
	CHECK(mgr.at(1)->header_offset == 5);
	CHECK(mgr.at(1)->body_offset == 10);
	PASS();
}

void test_two_blocks_expand_first(BTextView* tv) {
	TEST("expand first block adjusts second block offsets");
	tv->SetText("");
	// Start with block 0 collapsed: header at 0 (len 5), body empty
	// Block 1: header at 5 (len 5), body at 10 (len 8)
	tv->Insert(0, "HDR0_HDR1_BODY1___", 18);

	ToolBlockManager mgr;
	{
		ToolBlock b;
		b.header_offset = 0; b.header_len = 5;
		b.body_offset = 5;   b.body_len = 0;
		b.body_text = "BODY0_____";  // 10 chars
		b.collapsed = true;
		mgr.add(b);
	}
	{
		ToolBlock b;
		b.header_offset = 5; b.header_len = 5;
		b.body_offset = 10;  b.body_len = 8;
		b.body_text = "BODY1___";
		mgr.add(b);
	}

	// Expand block 0 — inserts 10 bytes at offset 5.
	// Block 1 should shift: header 5→15, body 10→20.
	mgr.expand(0, tv, noop_arrow, nullptr);
	CHECK(tv->TextLength() == 28);  // 18 + 10
	CHECK(mgr.at(0)->collapsed == false);
	CHECK(mgr.at(0)->body_len == 10);
	CHECK(mgr.at(1)->header_offset == 15);
	CHECK(mgr.at(1)->body_offset == 20);
	PASS();
}

void test_collapse_invalid() {
	TEST("collapse/expand/toggle out of range is safe");
	ToolBlockManager mgr;
	// Should not crash.
	mgr.collapse(-1, nullptr, nullptr, nullptr);
	mgr.collapse(0, nullptr, nullptr, nullptr);
	mgr.expand(-1, nullptr, nullptr, nullptr);
	mgr.toggle(99, nullptr, nullptr, nullptr);
	PASS();
}

int main() {
	BApplication app("application/x-vnd.pluto-test-toolblock");

	// Create a BTextView for tests that need one.
	BRect r(0, 0, 500, 500);
	BTextView* tv = new BTextView(r, "test", r, B_FOLLOW_ALL, B_WILL_DRAW);

	std::printf("test_toolblock\n");
	test_empty();
	test_add_and_access();
	test_hit_test();
	test_collapse_single(tv);
	test_expand_single(tv);
	test_toggle(tv);
	test_two_blocks_collapse_first(tv);
	test_two_blocks_expand_first(tv);
	test_collapse_invalid();
	std::printf("%d/%d passed\n", g_pass, g_total);

	delete tv;
	return g_pass == g_total ? 0 : 1;
}
