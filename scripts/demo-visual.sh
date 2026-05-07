#!/bin/sh
#
# demo-visual.sh — theatrical visual demo of Pluto on Haiku.
#
# Opens windows, creates files with BFS attributes, runs live BQuery
# in Tracker, scripts apps via BMessage, pops alerts, launches the GUI.
# All automatic, all cleanup at the end. Zero API tokens.
#
# Run:  ./scripts/demo-visual.sh

set -e

DEMO_DIR="/boot/home/Desktop/pluto-demo"
SESS_DIR="$DEMO_DIR/sessions"

cleanup() {
    rm -rf "$DEMO_DIR"
    # Kill Pluto-gui if we started it
    [ -n "$GUI_PID" ] && kill "$GUI_PID" 2>/dev/null || true
}
trap cleanup EXIT

pause() {
    sleep "${1:-1}"
}

# ─── Act 1: Setup ────────────────────────────────────────────────

alert --idea "Pluto — Visual Demo

This demo will:

• Create files with BFS attributes on your Desktop
• Open Tracker windows and run live queries
• Script running apps via BMessage
• Launch the Pluto-gui native interface
• Clean up everything automatically

No API tokens consumed." "Let's go!"

rm -rf "$DEMO_DIR"
mkdir -p "$DEMO_DIR" "$SESS_DIR"

# ─── Act 2: Create project files with rich BFS attributes ────────

alert --info "Act 1/5: Creating files with BFS extended attributes..." "OK" &
ALERT_PID=$!

# Source files
for f in main.cpp config.cpp agent.cpp tools.cpp gui.cpp; do
    echo "// pluto — $f" > "$DEMO_DIR/$f"
    echo "// Haiku-native Claude client" >> "$DEMO_DIR/$f"
done

# Set BFS attributes like pluto would
addattr -t string "pluto:status" "complete"  "$DEMO_DIR/main.cpp"
addattr -t string "pluto:status" "complete"  "$DEMO_DIR/config.cpp"
addattr -t string "pluto:status" "in-progress" "$DEMO_DIR/agent.cpp"
addattr -t string "pluto:status" "in-progress" "$DEMO_DIR/tools.cpp"
addattr -t string "pluto:status" "new"       "$DEMO_DIR/gui.cpp"

addattr -t string "pluto:author" "claude"    "$DEMO_DIR/main.cpp"
addattr -t string "pluto:author" "claude"    "$DEMO_DIR/config.cpp"
addattr -t string "pluto:author" "claude"    "$DEMO_DIR/agent.cpp"
addattr -t string "pluto:author" "human"     "$DEMO_DIR/tools.cpp"
addattr -t string "pluto:author" "claude"    "$DEMO_DIR/gui.cpp"

addattr -t int32  "pluto:priority" 1         "$DEMO_DIR/main.cpp"
addattr -t int32  "pluto:priority" 2         "$DEMO_DIR/config.cpp"
addattr -t int32  "pluto:priority" 1         "$DEMO_DIR/agent.cpp"
addattr -t int32  "pluto:priority" 3         "$DEMO_DIR/tools.cpp"
addattr -t int32  "pluto:priority" 1         "$DEMO_DIR/gui.cpp"

# Session files with metadata
cat > "$SESS_DIR/refactoring.jsonl" << 'EOF'
{"role":"user","content":[{"type":"text","text":"Refactor the MCP client into a Manager class"}]}
{"role":"assistant","content":[{"type":"text","text":"I'll create src/mcp/Manager.h with a clean API..."}]}
EOF
addattr -t string "pluto:title"   "MCP refactoring"     "$SESS_DIR/refactoring.jsonl"
addattr -t string "pluto:model"   "claude-opus-4-5"     "$SESS_DIR/refactoring.jsonl"
addattr -t int64  "pluto:tokens_in"  4500               "$SESS_DIR/refactoring.jsonl"
addattr -t int64  "pluto:tokens_out" 1200               "$SESS_DIR/refactoring.jsonl"
addattr -t string "pluto:tags"    "refactor,mcp"        "$SESS_DIR/refactoring.jsonl"

cat > "$SESS_DIR/gui-design.jsonl" << 'EOF'
{"role":"user","content":[{"type":"text","text":"Design a native Haiku GUI with BSplitView"}]}
{"role":"assistant","content":[{"type":"text","text":"I'll create a MainWindow with chat + detail split..."}]}
EOF
addattr -t string "pluto:title"   "GUI design session"  "$SESS_DIR/gui-design.jsonl"
addattr -t string "pluto:model"   "claude-opus-4-6"     "$SESS_DIR/gui-design.jsonl"
addattr -t int64  "pluto:tokens_in"  8900               "$SESS_DIR/gui-design.jsonl"
addattr -t int64  "pluto:tokens_out" 3400               "$SESS_DIR/gui-design.jsonl"
addattr -t string "pluto:tags"    "gui,haiku,design"    "$SESS_DIR/gui-design.jsonl"

wait $ALERT_PID 2>/dev/null || true
pause 0.5

# ─── Act 3: Open in Tracker and show attributes ──────────────────

alert --info "Act 2/5: Opening in Tracker — look at the BFS attributes!" "OK" &
ALERT_PID=$!

# Open the demo folder
open "$DEMO_DIR"
pause 1.5

# Move the most recently opened Tracker window to a nice position.
# Tracker names windows by folder leaf, so use the last window index.
LAST_WIN=$(hey application/x-vnd.Be-TRAK count Window 2>/dev/null | grep -o '[0-9]*' | tail -1 || echo "0")
LAST_IDX=$((LAST_WIN - 1))
[ "$LAST_IDX" -lt 0 ] && LAST_IDX=0
hey application/x-vnd.Be-TRAK set Frame of Window "$LAST_IDX" to "BRect(80, 80, 680, 400)" 2>/dev/null || true
pause 0.5

wait $ALERT_PID 2>/dev/null || true

# ─── Act 4: Live BQuery — find files by attribute ────────────────

alert --info "Act 3/5: Running a BQuery to find files by pluto:status...

Query: pluto:status == \"in-progress\"

This searches the ENTIRE filesystem via BFS indexes!" "Run Query" &
ALERT_PID=$!
wait $ALERT_PID 2>/dev/null || true

# Run query and show results
QUERY_RESULTS=$(query 'pluto:status == "in-progress"' 2>/dev/null || echo "(query not indexed)")

# Create a visual query file that Tracker can open
QUERY_FILE="$DEMO_DIR/find-in-progress"
touch "$QUERY_FILE"
addattr -t string "_trk/qrystr" 'pluto:status == "in-progress"' "$QUERY_FILE" 2>/dev/null
addattr -t string "BEOS:TYPE" "application/x-vnd.Be-query" "$QUERY_FILE" 2>/dev/null
addattr -t string "_trk/qryinitstr" 'pluto:status == "in-progress"' "$QUERY_FILE" 2>/dev/null
addattr -t int32 "_trk/qryinitmode" 1 "$QUERY_FILE" 2>/dev/null

# Also create a "find completed" query
QUERY_FILE2="$DEMO_DIR/find-complete"
touch "$QUERY_FILE2"
addattr -t string "_trk/qrystr" 'pluto:status == "complete"' "$QUERY_FILE2" 2>/dev/null
addattr -t string "BEOS:TYPE" "application/x-vnd.Be-query" "$QUERY_FILE2" 2>/dev/null

pause 0.5

# ─── Act 5: Desktop scripting via BMessage ────────────────────────

alert --info "Act 4/5: Scripting the desktop via BMessage!

Pluto can send scripting commands to ANY running Haiku app.
Watch as we query Tracker programmatically..." "Go"

# Count windows
WIN_COUNT=$(hey application/x-vnd.Be-TRAK count Window 2>/dev/null | grep -o 'int32([0-9]*)' | grep -o '[0-9]*' || echo "?")

# Get window titles
TITLES=""
i=0
while [ $i -lt 5 ]; do
    T=$(hey application/x-vnd.Be-TRAK get Title of Window $i 2>/dev/null | grep -o '"[^"]*"' | head -1 || true)
    if [ -n "$T" ]; then
        TITLES="$TITLES
  Window $i: $T"
    fi
    i=$((i + 1))
done

alert --info "Desktop scripting results:

Tracker has $WIN_COUNT window(s):
$TITLES

All queried via BMessage — the same protocol
Pluto's desktop_script tool uses!" "Cool!"

pause 0.5

# ─── Act 6: Launch Pluto-gui ─────────────────────────────────────

alert --info "Act 5/5: Launching Pluto-gui!

The native Haiku GUI with:
• BSplitView (chat + detail panel)
• Collapsible tool blocks
• Streaming cursor animation
• Token budget gauge
• MCP server menu
• Dark/light theme support
• Session save/load

Window will appear momentarily..." "Launch!"

# Launch GUI in background (will show API key dialog if no key)
./Pluto-gui &
GUI_PID=$!
pause 3

# ─── Finale ───────────────────────────────────────────────────────

LOC=$(find src -name '*.cpp' -o -name '*.h' | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
TOOLS=$(grep -c 'tools.add.*make_' src/main.cpp 2>/dev/null || echo "?")
TESTS=$(ls tests/test_*.cpp 2>/dev/null | wc -l)

alert --idea "Demo complete!

Pluto — Haiku-native Claude client

• $LOC lines of C++17
• $TOOLS tools (6 built-in + 9 Haiku-native)
• $TESTS test suites
• 2 binaries (CLI + native GUI)
• HVIF icon, .hpkg packaging
• Session storage with BFS attributes
• BQuery search from Tracker's Find dialog
• Desktop scripting via BMessage
• MCP client with auto-discovery
• Prompt caching + retry + cancel

No other OS can do this.

github.com/atomozero/pluto" "Close demo"

# Cleanup happens via trap
