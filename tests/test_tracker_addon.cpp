// Smoke test for the Tracker add-on: verify the shared library loads
// and exports the required process_refs symbol.

#include <image.h>
#include <OS.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

// Get the directory of the running test binary.
static std::string exe_dir() {
	image_info info;
	int32 cookie = 0;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		if (info.type == B_APP_IMAGE) {
			std::string path = info.name;
			size_t slash = path.rfind('/');
			if (slash != std::string::npos)
				return path.substr(0, slash);
			return ".";
		}
	}
	return ".";
}

static void test_load_addon() {
	std::string addon_path = exe_dir() + "/Ask pluto";
	image_id img = load_add_on(addon_path.c_str());
	CHECK(img >= 0);
	if (img < 0) {
		std::fprintf(stderr, "  cannot load '%s': image_id=%d\n",
			addon_path.c_str(), (int)img);
		return;
	}

	// Check for the required process_refs symbol.
	void* sym = nullptr;
	status_t rc = get_image_symbol(img, "process_refs",
		B_SYMBOL_TYPE_TEXT, &sym);
	CHECK(rc == B_OK);
	CHECK(sym != nullptr);

	unload_add_on(img);
}

int main() {
	test_load_addon();

	if (g_failures == 0) {
		std::fprintf(stderr, "test_tracker_addon: OK\n");
		return 0;
	}
	std::fprintf(stderr, "test_tracker_addon: %d failure(s)\n", g_failures);
	return 1;
}
