// tests/test_toolblock.cpp — ToolBlockManager unit tests
//
// Tests paragraph-based collapsible tool blocks using TextDocument.

#include "gui/ToolBlockManager.h"

#include <Application.h>

#include <cassert>
#include <cstdio>
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

void test_empty() {
	TEST("empty manager");
	ToolBlockManager mgr;
	CHECK(mgr.count() == 0);
	CHECK(mgr.at(0) == nullptr);
	CHECK(mgr.at(-1) == nullptr);
	PASS();
}

void test_add_and_access() {
	TEST("add and access blocks");
	ToolBlockManager mgr;
	ToolBlock b;
	b.body_text = "some tool output";
	b.is_error = false;
	b.collapsed = false;
	b.header_para = 0;
	b.header_para_count = 1;
	b.body_para = 1;
	b.body_para_count = 2;

	int idx = mgr.add(b);
	CHECK(idx == 0);
	CHECK(mgr.count() == 1);
	CHECK(mgr.at(0) != nullptr);
	CHECK(mgr.at(0)->body_text == "some tool output");
	CHECK(mgr.at(0)->body_para_count == 2);
	CHECK(mgr.at(1) == nullptr);
	PASS();
}

void test_hit_test_para() {
	TEST("hit_test_para finds correct block");
	ToolBlockManager mgr;

	ToolBlock b1;
	b1.header_para = 0;
	b1.header_para_count = 1;
	b1.body_para = 1;
	b1.body_para_count = 3;
	mgr.add(b1);

	ToolBlock b2;
	b2.header_para = 4;
	b2.header_para_count = 1;
	b2.body_para = 5;
	b2.body_para_count = 2;
	mgr.add(b2);

	CHECK(mgr.hit_test_para(0) == 0);   // header of block 0
	CHECK(mgr.hit_test_para(1) == -1);   // body of block 0 (not header)
	CHECK(mgr.hit_test_para(4) == 1);   // header of block 1
	CHECK(mgr.hit_test_para(5) == -1);   // body of block 1
	CHECK(mgr.hit_test_para(10) == -1);  // outside any block
	PASS();
}

void test_clear() {
	TEST("clear removes all blocks");
	ToolBlockManager mgr;
	ToolBlock b;
	b.body_text = "text";
	mgr.add(b);
	mgr.add(b);
	CHECK(mgr.count() == 2);
	mgr.clear();
	CHECK(mgr.count() == 0);
	PASS();
}

void test_blocks_access() {
	TEST("blocks() provides direct vector access");
	ToolBlockManager mgr;
	ToolBlock b1;
	b1.body_text = "first";
	ToolBlock b2;
	b2.body_text = "second";
	mgr.add(b1);
	mgr.add(b2);

	auto& blocks = mgr.blocks();
	CHECK(blocks.size() == 2);
	CHECK(blocks[0].body_text == "first");
	CHECK(blocks[1].body_text == "second");
	PASS();
}

void test_error_flag() {
	TEST("error flag is preserved");
	ToolBlockManager mgr;
	ToolBlock b;
	b.is_error = true;
	b.body_text = "error output";
	mgr.add(b);
	CHECK(mgr.at(0)->is_error == true);
	PASS();
}

int main() {
	BApplication app("application/x-vnd.pluto-test-toolblock");

	std::printf("test_toolblock\n");
	test_empty();
	test_add_and_access();
	test_hit_test_para();
	test_clear();
	test_blocks_access();
	test_error_flag();
	std::printf("%d/%d passed\n", g_pass, g_total);

	return g_pass == g_total ? 0 : 1;
}
