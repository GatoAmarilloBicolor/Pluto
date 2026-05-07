#!/bin/sh
#
# gen-test.sh — scaffold a new Pluto test without consuming API tokens.
#
# Usage:
#   ./scripts/gen-test.sh <test_name> [obj1.o obj2.o ...] [--lbe]
#
# Examples:
#   ./scripts/gen-test.sh test_config src/config.o src/json.o
#   ./scripts/gen-test.sh test_bfs src/haiku/BfsTools.o src/tools/Tool.o src/json.o --lbe
#   ./scripts/gen-test.sh test_mcp src/mcp/Client.o src/json.o --lbe
#
# What it does:
#   1. Creates tests/<name>.cpp with standard Pluto test scaffolding
#   2. Prints the Makefile lines to add (you paste them in)
#
# Zero API tokens needed. Edit the .cpp, paste the Makefile rule, make check.

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <test_name> [obj1.o obj2.o ...] [--lbe]"
    echo ""
    echo "Examples:"
    echo "  $0 test_config src/config.o src/json.o"
    echo "  $0 test_bfs src/haiku/BfsTools.o src/tools/Tool.o src/json.o --lbe"
    exit 1
fi

NAME="$1"
shift

OBJS=""
LIBS=""
for arg in "$@"; do
    case "$arg" in
        --lbe) LIBS=" -lbe" ;;
        *)     OBJS="$OBJS $arg" ;;
    esac
done

CPP="tests/${NAME}.cpp"

if [ -f "$CPP" ]; then
    echo "ERROR: $CPP already exists."
    exit 1
fi

# --- Generate test source ---

cat > "$CPP" << 'EOF'
// Auto-generated test scaffold for TESTNAME.
// Edit to add test cases, then: make TESTNAME && ./TESTNAME
//
// Pattern: static void test_xxx() with CHECK/CHECK_EQ macros.
// make_tmpdir() for isolated temp dirs. No API tokens needed.

// TODO: adjust includes for the module under test.
#include "../src/json.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	auto _a = (a); auto _b = (b); \
	if (!(_a == _b)) { \
		std::fprintf(stderr, "FAIL %s:%d: " #a " == " #b "\n", \
			__FILE__, __LINE__); \
		++g_failures; \
	} \
} while (0)

static std::string g_tmpdir;

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-TESTNAME-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) { std::fprintf(stderr, "mkdtemp failed\n"); std::exit(2); }
	g_tmpdir = d;
	return g_tmpdir;
}

static void cleanup() {
	if (!g_tmpdir.empty()) {
		std::string cmd = "rm -rf '" + g_tmpdir + "'";
		std::system(cmd.c_str());
	}
}

// --- Test cases ---

static void test_example() {
	std::fprintf(stderr, "  example...");
	CHECK(1 + 1 == 2);
	std::fprintf(stderr, " ok\n");
}

// TODO: add more tests here

int main() {
	std::fprintf(stderr, "TESTNAME\n");
	make_tmpdir();

	test_example();

	cleanup();
	if (g_failures == 0) {
		std::fprintf(stderr, "TESTNAME: OK\n");
		return 0;
	}
	std::fprintf(stderr, "TESTNAME: %d failure(s)\n", g_failures);
	return 1;
}
EOF

# Replace placeholder
sed -i "s/TESTNAME/${NAME}/g" "$CPP"

echo "Created $CPP"
echo ""

# --- Print Makefile instructions ---

echo "=== Add to Makefile ==="
echo ""
echo "1. In TEST_BINS, add:  ${NAME}"
echo ""
echo "2. Add this build rule (before the 'check:' target):"
echo ""
if [ -n "$OBJS" ]; then
    echo "${NAME}: tests/${NAME}.cpp \\"
    echo "            ${OBJS}"
else
    echo "${NAME}: tests/${NAME}.cpp"
fi
echo "	\$(CXX) \$(CXXFLAGS) -o \$@ \$^${LIBS}"
echo ""
echo "=== Then ==="
echo ""
echo "  make ${NAME} && ./${NAME}   # run just this test"
echo "  make check                  # run all tests"
