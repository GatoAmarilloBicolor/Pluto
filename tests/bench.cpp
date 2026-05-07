// tests/bench.cpp — Performance benchmarks for Pluto core subsystems
//
// Run: make bench && ./bench
// Reports: iterations, total time, per-operation time, throughput

#include "json.h"
#include "rag.h"
#include "markdown.h"
#include "storage/SessionStore.h"
#include "workspace.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

using Clock = std::chrono::high_resolution_clock;

static double elapsed_ms(Clock::time_point start, Clock::time_point end) {
	return std::chrono::duration<double, std::milli>(end - start).count();
}

// ---------- Benchmark helpers ----------

struct BenchResult {
	const char* name;
	int iterations;
	double total_ms;
	double per_op_us;  // microseconds per operation
};

static std::vector<BenchResult> g_results;

#define BENCH(name, iters, body) do { \
	auto _start = Clock::now(); \
	for (int _i = 0; _i < (iters); ++_i) { body; } \
	auto _end = Clock::now(); \
	double _total = elapsed_ms(_start, _end); \
	double _per = (_total * 1000.0) / (iters); \
	g_results.push_back({name, iters, _total, _per}); \
} while(0)

// ---------- JSON benchmarks ----------

void bench_json_parse_small() {
	std::string json = R"({"model":"claude-opus-4-5","max_tokens":4096,"cache":true})";
	BENCH("json parse small (62B)", 10000, {
		auto v = pluto::json::Value::parse(json);
		(void)v;
	});
}

void bench_json_parse_medium() {
	// Simulate a typical API response (~2KB)
	std::string json = "{\"id\":\"msg_01\",\"type\":\"message\",\"role\":\"assistant\","
		"\"content\":[{\"type\":\"text\",\"text\":\"";
	std::string filler(2000, 'x');
	json += filler;
	json += "\"}],\"model\":\"claude-opus-4-5\",\"stop_reason\":\"end_turn\","
		"\"usage\":{\"input_tokens\":150,\"output_tokens\":500}}";
	BENCH("json parse medium (2KB)", 5000, {
		auto v = pluto::json::Value::parse(json);
		(void)v;
	});
}

void bench_json_parse_large() {
	// Simulate a large tool result (~50KB)
	pluto::json::Object obj;
	pluto::json::Array arr;
	for (int i = 0; i < 100; ++i) {
		pluto::json::Object item;
		item["name"] = std::string("file_") + std::to_string(i) + ".cpp";
		item["size"] = (double)(i * 1024);
		item["content"] = std::string(400, 'a' + (i % 26));
		arr.push_back(pluto::json::Value(std::move(item)));
	}
	obj["results"] = pluto::json::Value(std::move(arr));
	std::string json = pluto::json::Value(std::move(obj)).serialize();

	BENCH("json parse large (50KB)", 500, {
		auto v = pluto::json::Value::parse(json);
		(void)v;
	});
}

void bench_json_serialize() {
	pluto::json::Object obj;
	obj["model"] = "claude-opus-4-5";
	obj["max_tokens"] = 4096;
	obj["stream"] = true;
	pluto::json::Array messages;
	pluto::json::Object msg;
	msg["role"] = "user";
	msg["content"] = std::string(500, 'H');
	messages.push_back(pluto::json::Value(std::move(msg)));
	obj["messages"] = pluto::json::Value(std::move(messages));
	pluto::json::Value val(std::move(obj));

	BENCH("json serialize (1KB)", 10000, {
		std::string s = val.serialize();
		(void)s;
	});
}

// ---------- RAG benchmarks ----------

void bench_rag_search() {
	std::string idx = pluto::rag_default_path();
	if (idx.empty()) {
		std::printf("  [skip] RAG index not found\n");
		return;
	}
	pluto::rag_init(idx);
	int count = pluto::rag_entry_count();

	BENCH("rag search (390 entries)", 10000, {
		auto r = pluto::rag_search("BWindow BView layout", 3);
		(void)r;
	});

	BENCH("rag extract_classes", 50000, {
		auto c = pluto::rag_extract_classes(
			"Create a BWindow with a BView child and use BLayoutBuilder");
		(void)c;
	});

	std::printf("  [info] RAG loaded %d entries\n", count);
}

// ---------- Workspace benchmarks ----------

void bench_WorkspaceAllows() {
	pluto::WorkspaceSetRoot("/boot/home/Desktop/pluto");

	BENCH("WorkspaceAllows (inside)", 100000, {
		bool ok = pluto::WorkspaceAllows("/boot/home/Desktop/pluto/src/main.cpp");
		(void)ok;
	});

	BENCH("WorkspaceAllows (outside)", 100000, {
		bool ok = pluto::WorkspaceAllows("/boot/home/config/settings/secret");
		(void)ok;
	});

	pluto::WorkspaceSetRoot("");
}

// ---------- Session benchmarks ----------

void bench_session_save_load() {
	// Create a temp directory for sessions.
	char tmpdir[] = "/tmp/pluto-bench-XXXXXX";
	if (!mkdtemp(tmpdir)) {
		std::printf("  [skip] cannot create tmpdir\n");
		return;
	}
	pluto::SessionStore::set_dir_override(tmpdir);

	// Build a realistic conversation (20 turns).
	std::vector<pluto::Message> messages;
	for (int i = 0; i < 20; ++i) {
		pluto::Message user;
		user.role = "user";
		pluto::ContentBlock tb;
		tb.type = pluto::ContentBlock::TEXT;
		tb.text = "Explain how BWindow works in Haiku OS with code examples.";
		user.content.push_back(std::move(tb));
		messages.push_back(std::move(user));

		pluto::Message asst;
		asst.role = "assistant";
		pluto::ContentBlock ab;
		ab.type = pluto::ContentBlock::TEXT;
		ab.text = std::string(500, 'A');  // ~500 char response
		asst.content.push_back(std::move(ab));
		messages.push_back(std::move(asst));
	}

	std::string saved_path;
	BENCH("session save (20 turns)", 100, {
		std::string err;
		saved_path = pluto::SessionStore::save(
			"", "bench-session", "claude-opus-4-5",
			10000, 5000, messages, err);
	});

	if (!saved_path.empty()) {
		BENCH("session load (20 turns)", 100, {
			std::vector<pluto::Message> loaded;
			pluto::SessionMetadata meta;
			std::string err;
			pluto::SessionStore::load(saved_path, loaded, meta, err);
		});
	}

	// Cleanup.
	pluto::SessionStore::set_dir_override("");
	std::string rm = std::string("rm -rf ") + tmpdir;
	std::system(rm.c_str());
}

// ---------- Markdown benchmarks ----------

void bench_markdown_render() {
	std::string md =
		"# Heading\n\n"
		"This is **bold** and *italic* and `code`.\n\n"
		"```cpp\n"
		"#include <Window.h>\n"
		"int main() {\n"
		"    BWindow* w = new BWindow(...);\n"
		"    w->Show();\n"
		"}\n"
		"```\n\n"
		"- Item one\n"
		"- Item two\n"
		"- Item three\n\n"
		"Normal paragraph with [link](https://haiku-os.org).\n";

	BENCH("markdown terminal render", 10000, {
		pluto::TerminalMarkdown tm;
		for (size_t i = 0; i < md.size(); ++i) {
			tm.feed(md.substr(i, 1));
		}
		tm.flush();
	});
}

void bench_markdown_render_bulk() {
	// Feed entire blocks at once (more realistic for streaming).
	std::string md =
		"Here is a **bold** statement and a `code` snippet.\n"
		"Another line with *italic* text.\n";

	BENCH("markdown bulk feed (per-line)", 50000, {
		pluto::TerminalMarkdown tm;
		tm.feed(md);
		tm.flush();
	});
}

// ---------- String/Memory benchmarks ----------

void bench_string_alloc() {
	BENCH("std::string alloc 1KB", 100000, {
		std::string s(1024, 'x');
		(void)s;
	});

	BENCH("std::string alloc 64KB", 10000, {
		std::string s(65536, 'x');
		(void)s;
	});
}

// ---------- Main ----------

void print_results() {
	std::printf("\n");
	std::printf("%-40s  %8s  %10s  %12s\n",
		"BENCHMARK", "ITERS", "TOTAL(ms)", "PER-OP(us)");
	std::printf("%-40s  %8s  %10s  %12s\n",
		"----------------------------------------", "--------",
		"----------", "------------");

	for (const auto& r : g_results) {
		std::printf("%-40s  %8d  %10.1f  %12.2f\n",
			r.name, r.iterations, r.total_ms, r.per_op_us);
	}
	std::printf("\n");
}

int main() {
	std::printf("Pluto Benchmark Suite\n");
	std::printf("=====================\n\n");

	// JSON
	std::printf("== JSON ==\n");
	bench_json_parse_small();
	bench_json_parse_medium();
	bench_json_parse_large();
	bench_json_serialize();

	// RAG
	std::printf("== RAG ==\n");
	bench_rag_search();

	// Workspace
	std::printf("== Workspace ==\n");
	bench_WorkspaceAllows();

	// Sessions
	std::printf("== Sessions ==\n");
	bench_session_save_load();

	// Markdown
	std::printf("== Markdown ==\n");
	bench_markdown_render();
	bench_markdown_render_bulk();

	// Memory
	std::printf("== Memory ==\n");
	bench_string_alloc();

	print_results();

	// Memory footprint.
	std::printf("== Memory Footprint ==\n");
	FILE* f = fopen("/proc/self/status", "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, "VmRSS:", 6) == 0 ||
			    strncmp(line, "VmSize:", 7) == 0 ||
			    strncmp(line, "VmPeak:", 7) == 0) {
				std::printf("  %s", line);
			}
		}
		fclose(f);
	}

	// Binary sizes.
	std::printf("\n== Binary Sizes ==\n");
	struct stat st;
	if (stat("Pluto_cli", &st) == 0)
		std::printf("  Pluto_cli:  %lld KB\n", (long long)st.st_size / 1024);
	if (stat("Pluto", &st) == 0)
		std::printf("  Pluto-gui:  %lld KB\n", (long long)st.st_size / 1024);

	return 0;
}
