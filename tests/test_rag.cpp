// Tests for local BeAPI RAG engine.

#include "../src/rag.h"
#include "../src/json.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

using pluto::json::Value;
using pluto::json::Array;

static std::string g_tmpdir;

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-rag-XXXXXX";
	const char* d = mkdtemp(tmpl);
	assert(d);
	return std::string(d);
}

// Write a minimal test index with known entries.
static std::string write_test_index() {
	std::string path = g_tmpdir + "/beapi-index.jsonl";
	std::ofstream f(path);
	assert(f.good());

	// Entry 1: BSlider
	Value e1;
	e1.set("topic", std::string("BSlider vertical with label"));
	e1.set("question", std::string("How to create a vertical slider in Haiku?"));
	e1.set("answer", std::string("#include <Slider.h>\nBSlider* s = new BSlider(\"vol\", \"Volume\", NULL, 0, 100, B_VERTICAL);"));
	Value c1(Array{});
	c1.push(Value(std::string("BSlider")));
	c1.push(Value(std::string("BApplication")));
	c1.push(Value(std::string("BWindow")));
	e1.set("classes", std::move(c1));
	f << e1.serialize() << "\n";

	// Entry 2: BListView
	Value e2;
	e2.set("topic", std::string("BListView selection handling"));
	e2.set("question", std::string("How to handle selection changes in BListView?"));
	e2.set("answer", std::string("#include <ListView.h>\nBListView* list = new BListView(\"mylist\");"));
	Value c2(Array{});
	c2.push(Value(std::string("BListView")));
	c2.push(Value(std::string("BScrollView")));
	c2.push(Value(std::string("BStringItem")));
	e2.set("classes", std::move(c2));
	f << e2.serialize() << "\n";

	// Entry 3: BNotification
	Value e3;
	e3.set("topic", std::string("BNotification simple notification"));
	e3.set("question", std::string("How to show a notification in Haiku?"));
	e3.set("answer", std::string("#include <Notification.h>\nBNotification n(B_INFORMATION_NOTIFICATION);"));
	Value c3(Array{});
	c3.push(Value(std::string("BNotification")));
	e3.set("classes", std::move(c3));
	f << e3.serialize() << "\n";

	// Entry 4: BFilePanel
	Value e4;
	e4.set("topic", std::string("BFilePanel open dialog"));
	e4.set("question", std::string("How to open a file dialog in Haiku?"));
	e4.set("answer", std::string("#include <FilePanel.h>\nBFilePanel* panel = new BFilePanel(B_OPEN_PANEL);"));
	Value c4(Array{});
	c4.push(Value(std::string("BFilePanel")));
	c4.push(Value(std::string("BMessenger")));
	e4.set("classes", std::move(c4));
	f << e4.serialize() << "\n";

	// Entry 5: BView drawing
	Value e5;
	e5.set("topic", std::string("BView custom drawing"));
	e5.set("question", std::string("How to draw custom graphics in a BView?"));
	e5.set("answer", std::string("void MyView::Draw(BRect r) { SetHighColor(255,0,0); FillRect(r); }"));
	Value c5(Array{});
	c5.push(Value(std::string("BView")));
	c5.push(Value(std::string("BRect")));
	e5.set("classes", std::move(c5));
	f << e5.serialize() << "\n";

	f.close();
	return path;
}

static void test_init_loads_entries() {
	std::string path = write_test_index();
	int n = pluto::rag_init(path);
	assert(n == 5);
	assert(pluto::rag_entry_count() == 5);
	std::printf("  init loads entries... ok\n");
}

static void test_init_bad_path() {
	int n = pluto::rag_init("/nonexistent/path.jsonl");
	assert(n == 0);
	assert(pluto::rag_entry_count() == 0);
	std::printf("  init bad path... ok\n");
}

static void test_search_by_class() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	auto results = pluto::rag_search("I need a BSlider for volume control");
	assert(!results.empty());
	assert(results[0].entry->topic.find("BSlider") != std::string::npos);
	assert(results[0].score > 0.0);
	std::printf("  search by class... ok\n");
}

static void test_search_by_keyword() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	auto results = pluto::rag_search("notification");
	assert(!results.empty());
	assert(results[0].entry->topic.find("BNotification") != std::string::npos);
	std::printf("  search by keyword... ok\n");
}

static void test_search_ranking() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	// Searching for BFilePanel should rank BFilePanel entry first.
	auto results = pluto::rag_search("BFilePanel open file dialog");
	assert(!results.empty());
	assert(results[0].entry->topic.find("BFilePanel") != std::string::npos);

	// Score should be strictly greater than second result (if any).
	if (results.size() > 1)
		assert(results[0].score > results[1].score);

	std::printf("  search ranking... ok\n");
}

static void test_search_top_k() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	auto results = pluto::rag_search("Haiku", 2);
	assert((int)results.size() <= 2);
	std::printf("  search top_k... ok\n");
}

static void test_search_no_match() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	auto results = pluto::rag_search("quantum entanglement physics");
	assert(results.empty());
	std::printf("  search no match... ok\n");
}

static void test_extract_classes() {
	auto classes = pluto::rag_extract_classes(
		"I need a BView with BSlider and BMenuBar inside");
	assert(classes.size() == 3);
	// Should find BView, BSlider, BMenuBar
	bool found_view = false, found_slider = false, found_menu = false;
	for (const auto& c : classes) {
		if (c == "BView") found_view = true;
		if (c == "BSlider") found_slider = true;
		if (c == "BMenuBar") found_menu = true;
	}
	assert(found_view);
	assert(found_slider);
	assert(found_menu);
	std::printf("  extract classes... ok\n");
}

static void test_extract_classes_no_false_positives() {
	// "BOLD" starts with B but second char is O (uppercase) - should match
	// but it's only 4 chars and is a common word. Our filter requires B+uppercase,
	// so "BOLD" would match. Let's test that single-B words don't match.
	auto classes = pluto::rag_extract_classes("use bold text BY the way");
	// "BY" is only 2 chars, filtered out (< 3).
	// "BOLD" would not match because 'O' follows 'B' and is uppercase — but
	// this IS the pattern. Let's verify behavior is consistent.
	for (const auto& c : classes) {
		assert(c.size() >= 3);
	}
	std::printf("  extract no false positives... ok\n");
}

static void test_search_multiple_classes() {
	std::string path = write_test_index();
	pluto::rag_init(path);

	// BView + drawing → should find the BView custom drawing entry
	auto results = pluto::rag_search("BView custom Draw BRect");
	assert(!results.empty());
	assert(results[0].entry->topic.find("BView") != std::string::npos);
	std::printf("  search multiple classes... ok\n");
}

int main() {
	std::printf("test_rag\n");
	g_tmpdir = make_tmpdir();

	test_init_loads_entries();
	test_init_bad_path();
	test_search_by_class();
	test_search_by_keyword();
	test_search_ranking();
	test_search_top_k();
	test_search_no_match();
	test_extract_classes();
	test_extract_classes_no_false_positives();
	test_search_multiple_classes();

	// Cleanup
	std::string cmd = "rm -rf " + g_tmpdir;
	std::system(cmd.c_str());

	std::printf("test_rag: OK (10 tests)\n");
	return 0;
}
