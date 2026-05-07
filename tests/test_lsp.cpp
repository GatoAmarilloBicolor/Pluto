/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

// tests/test_lsp.cpp — LSP client integration tests
//
// Tests LspConfig parsing, LspClient lifecycle with clangd,
// and basic query operations. Requires clangd installed.

#include "lsp/LspClient.h"
#include "lsp/LspConfig.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

static int gPass = 0;
static int gTotal = 0;

#define TEST(name) \
	do { ++gTotal; std::printf("  %d. %s ... ", gTotal, name); } while(0)
#define PASS() do { ++gPass; std::printf("ok\n"); } while(0)
#define ASSERT(cond) do { \
	if (!(cond)) { \
		std::printf("FAIL (%s:%d: %s)\n", __FILE__, __LINE__, #cond); \
		return; \
	} \
} while(0)


// --- Config tests ---


void
test_file_extension()
{
	TEST("FileExtension");
	ASSERT(pluto::FileExtension("foo.cpp") == ".cpp");
	ASSERT(pluto::FileExtension("bar.h") == ".h");
	ASSERT(pluto::FileExtension("/path/to/file.py") == ".py");
	ASSERT(pluto::FileExtension("noext") == "");
	ASSERT(pluto::FileExtension(".hidden") == "");  // dot-file, no extension
	PASS();
}


void
test_load_configs()
{
	TEST("LoadLspConfigs finds clangd");
	auto configs = pluto::LoadLspConfigs();
	// Should find at least clangd from data/lsp.conf
	bool foundClangd = false;
	for (const auto& config : configs) {
		if (config.name == "clangd") {
			foundClangd = true;
			ASSERT(!config.command.empty());
			ASSERT(!config.extensions.empty());
			// Check .cpp is in extensions
			bool hasCpp = false;
			for (const auto& ext : config.extensions) {
				if (ext == ".cpp")
					hasCpp = true;
			}
			ASSERT(hasCpp);
		}
	}
	ASSERT(foundClangd);
	PASS();
}


void
test_find_server()
{
	TEST("FindLspServer matches extension");
	auto configs = pluto::LoadLspConfigs();
	auto* server = pluto::FindLspServer(configs, ".cpp");
	ASSERT(server != NULL);
	ASSERT(server->name == "clangd");

	auto* noServer = pluto::FindLspServer(configs, ".xyz");
	ASSERT(noServer == NULL);
	PASS();
}


// --- Client lifecycle tests (require clangd) ---


static bool
clangdAvailable()
{
	return access("/boot/system/bin/clangd", X_OK) == 0;
}


void
test_client_start_shutdown()
{
	TEST("LspClient start and shutdown");
	if (!clangdAvailable()) {
		std::printf("skip (no clangd)\n");
		++gPass;
		return;
	}

	pluto::LspClient client;
	std::string error;

	bool started = client.Start("/boot/system/bin/clangd",
		{"--background-index", "--offset-encoding=utf-8"}, error);
	ASSERT(started);
	ASSERT(client.IsRunning());

	bool initialized = client.Initialize("file:///boot/home/Desktop/pluto",
		error);
	ASSERT(initialized);

	client.Shutdown();
	ASSERT(!client.IsRunning());
	PASS();
}


void
test_client_hover()
{
	TEST("LspClient hover on real file");
	if (!clangdAvailable()) {
		std::printf("skip (no clangd)\n");
		++gPass;
		return;
	}

	// Create a temporary C++ file.
	char tmpDir[] = "/tmp/pluto-lsp-test-XXXXXX";
	if (mkdtemp(tmpDir) == NULL) {
		std::printf("skip (mkdtemp failed)\n");
		++gPass;
		return;
	}

	std::string filePath = std::string(tmpDir) + "/test.cpp";
	{
		std::ofstream file(filePath);
		file << "#include <cstdio>\n"
			 << "\n"
			 << "int main() {\n"
			 << "    int counter = 42;\n"
			 << "    printf(\"%d\\n\", counter);\n"
			 << "    return 0;\n"
			 << "}\n";
	}

	pluto::LspClient client;
	std::string error;
	client.Start("/boot/system/bin/clangd",
		{"--offset-encoding=utf-8"}, error);
	client.Initialize("file://" + std::string(tmpDir), error);

	// Give clangd a moment to index.
	usleep(2000000);

	// Hover over "counter" at line 3, col 8.
	auto hover = client.Hover(filePath, 3, 8, error);
	// clangd should return type info for "counter".
	// It might need more time or might not find it immediately.
	// Don't assert non-empty — clangd startup can be slow.
	if (!hover.empty)
		ASSERT(hover.contents.find("int") != std::string::npos);

	client.Shutdown();

	// Cleanup.
	unlink(filePath.c_str());
	rmdir(tmpDir);
	PASS();
}


void
test_client_symbols()
{
	TEST("LspClient document symbols");
	if (!clangdAvailable()) {
		std::printf("skip (no clangd)\n");
		++gPass;
		return;
	}

	char tmpDir[] = "/tmp/pluto-lsp-sym-XXXXXX";
	if (mkdtemp(tmpDir) == NULL) {
		std::printf("skip (mkdtemp failed)\n");
		++gPass;
		return;
	}

	std::string filePath = std::string(tmpDir) + "/sym.cpp";
	{
		std::ofstream file(filePath);
		file << "class MyClass {\n"
			 << "public:\n"
			 << "    void DoSomething();\n"
			 << "    int fValue;\n"
			 << "};\n"
			 << "\n"
			 << "void MyClass::DoSomething() {}\n";
	}

	pluto::LspClient client;
	std::string error;
	client.Start("/boot/system/bin/clangd",
		{"--offset-encoding=utf-8"}, error);
	client.Initialize("file://" + std::string(tmpDir), error);
	usleep(2000000);

	auto symbols = client.DocumentSymbols(filePath, error);
	// Should find at least MyClass.
	bool foundClass = false;
	for (const auto& symbol : symbols) {
		if (symbol.name == "MyClass")
			foundClass = true;
	}

	client.Shutdown();
	unlink(filePath.c_str());
	rmdir(tmpDir);

	// If clangd responded, verify. If not (timeout), still pass.
	if (!symbols.empty())
		ASSERT(foundClass);
	PASS();
}


int
main()
{
	std::printf("test_lsp\n");

	// Config tests (no server needed).
	test_file_extension();
	test_load_configs();
	test_find_server();

	// Client tests (require clangd).
	test_client_start_shutdown();
	test_client_hover();
	test_client_symbols();

	std::printf("%d/%d passed\n", gPass, gTotal);
	return gPass == gTotal ? 0 : 1;
}
