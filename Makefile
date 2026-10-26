CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc -I/boot/system/develop/headers/private/interface -I/boot/system/develop/headers/private/support -I/boot/system/develop/headers/private/locale -I/boot/system/develop/headers/private/shared -I/boot/system/develop/headers/private/kernel -I/boot/system/develop/headers/private/storage -I/boot/system/develop/headers/private/app -MMD -MP
LDLIBS   = -lcurl -lbe -lcrypto -lnetwork -ledit

PKG_NAME    = pluto
PKG_VERSION = 0.7.1
PKG_ARCH    = x86_64

CORE_SRCS = src/lsp/LspClient.cpp \
            src/lsp/LspConfig.cpp \
            src/ipc/PlutoMessenger.cpp \
            src/agent_store.cpp \
            src/cancel.cpp \
            src/markdown.cpp \
            src/oauth.cpp \
            src/config.cpp \
            src/rag.cpp \
            src/wiki.cpp \
            src/routine.cpp \
            src/schedule.cpp \
            src/telemetry.cpp \
            src/frontmatter.cpp \
            src/text_index.cpp \
            src/util.cpp \
            src/workspace.cpp \
            src/json.cpp \
            src/storage/SessionStore.cpp \
            src/agent/Loop.cpp \
            src/tools/Tool.cpp \
            src/tools/ReadFile.cpp \
            src/tools/WriteFile.cpp \
            src/tools/EditFile.cpp \
            src/tools/Bash.cpp \
            src/tools/Grep.cpp \
            src/tools/Glob.cpp \
            src/tools/BeapiLookup.cpp \
            src/tools/WikiLookup.cpp \
            src/tools/WebFetch.cpp \
            src/tools/WebSearch.cpp \
            src/tools/TodoWrite.cpp \
            src/tools/ApplyPatch.cpp \
            src/tools/Lsp.cpp \
            src/tools/GraphQuery.cpp \
            src/map/Graph.cpp \
            src/map/Scanner.cpp \
            src/map/BfsAttrs.cpp \
            src/haiku/AsyncStream.cpp \
            src/haiku/BfsTools.cpp \
            src/haiku/DesktopTools.cpp \
            src/haiku/Notifications.cpp \
            src/haiku/PluginLoader.cpp \
            src/mcp/Client.cpp \
            src/mcp/Config.cpp \
            src/mcp/Manager.cpp \
            src/mcp/Transport.cpp \
            src/mcp/HttpTransport.cpp \
            src/mcp/StdioTransport.cpp \
            src/mcp/SseSubscriber.cpp \
            src/mcp/SseTransport.cpp \
            src/provider/Anthropic.cpp \
            src/provider/OpenAICompat.cpp

CLI_SRCS = src/main.cpp \
           src/repl.cpp \
           src/mcp/Cli.cpp \
           src/cli/CliApp.cpp \
           src/cli/ProviderFactory.cpp \
           src/cli/ToolRegistryBuilder.cpp \
           src/cli/McpBootstrap.cpp

GUI_SRCS = src/gui/main_gui.cpp \
           src/gui/App.cpp \
           src/gui/MainWindow.cpp \
           src/gui/SlashCommands.cpp \
           src/gui/SessionManager.cpp \
           src/gui/ConversationView.cpp \
           src/gui/ChatColors.cpp \
           src/gui/ToolBlockManager.cpp \
           src/gui/StreamingMarkdown.cpp \
           src/gui/SettingsDialog.cpp \
           src/gui/AgentStoreDialog.cpp \
           src/gui/RoutineDialog.cpp \
           src/gui/ScheduleDialog.cpp \
           src/gui/SessionBrowser.cpp \
           src/gui/MiniMapView.cpp \
           src/gui/GraphView.cpp \
           src/gui/GraphWindow.cpp \
           src/gui/TelemetryDialog.cpp \
           src/gui/DeskbarReplicant.cpp \
           src/gui/ColumnListView.cpp \
           src/gui/ColumnTypes.cpp \
           src/gui/textview/Bullet.cpp \
           src/gui/textview/BulletData.cpp \
           src/gui/textview/CharacterStyle.cpp \
           src/gui/textview/CharacterStyleData.cpp \
           src/gui/textview/ImageCache.cpp \
           src/gui/textview/MarkdownParser.cpp \
           src/gui/textview/Paragraph.cpp \
           src/gui/textview/ParagraphLayout.cpp \
           src/gui/textview/ParagraphStyle.cpp \
           src/gui/textview/ParagraphStyleData.cpp \
           src/gui/textview/StyleColoursData.cpp \
           src/gui/textview/TextDocument.cpp \
           src/gui/textview/TextDocumentLayout.cpp \
           src/gui/textview/TextDocumentView.cpp \
           src/gui/textview/TextEditor.cpp \
           src/gui/textview/TextListener.cpp \
           src/gui/textview/TextSelection.cpp \
           src/gui/textview/TextSpan.cpp \
           src/gui/textview/TextView.cpp \
           src/gui/textview/UndoableEditListener.cpp

# md4c is C, not C++
MD4C_OBJ = src/gui/textview/md4c.o

CORE_OBJS = $(CORE_SRCS:.cpp=.o)
CLI_OBJS  = $(CLI_SRCS:.cpp=.o)
GUI_OBJS  = $(GUI_SRCS:.cpp=.o)

all: Pluto_cli Pluto build-addon systemone_server

# Compile resource definition (icon + app metadata).
pluto.rsrc: pluto.rdef pluto.hvif
	rc -o $@ pluto.rdef

Pluto_cli: $(CORE_OBJS) $(CLI_OBJS) pluto.rsrc
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_OBJS) $(CLI_OBJS) $(LDLIBS)
	xres -o $@ pluto.rsrc
	resattr -o $@ pluto.rsrc
	addattr -t icon -f pluto.hvif BEOS:ICON $@

# Compile md4c (C file) separately.
CC ?= gcc
src/gui/textview/md4c.o: src/gui/textview/md4c.c
	$(CC) -Wall -O2 -MMD -MP -c -o $@ $<

Pluto: $(CORE_OBJS) $(GUI_OBJS) $(MD4C_OBJ) pluto.rsrc
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_OBJS) $(GUI_OBJS) $(MD4C_OBJ) $(LDLIBS) -ltracker -ltranslation
	xres -o $@ pluto.rsrc
	resattr -o $@ pluto.rsrc
	addattr -t icon -f pluto.hvif BEOS:ICON $@

# Tracker add-on (shared library).
# The name contains a space — use a simple variable and quote everywhere.
TRACKER_ADDON := Ask pluto

.PHONY: build-addon install-addon uninstall-addon

build-addon: src/haiku/TrackerAddOn.cpp pluto.hvif
	$(CXX) $(CXXFLAGS) -shared -fPIC -o '$(TRACKER_ADDON)' $< -lbe
	addattr -t icon -f pluto.hvif BEOS:ICON '$(TRACKER_ADDON)'

install-addon: build-addon
	mkdir -p '$(HOME)/config/non-packaged/add-ons/Tracker'
	cp '$(TRACKER_ADDON)' '$(HOME)/config/non-packaged/add-ons/Tracker/'
	@echo 'Installed: ~/config/non-packaged/add-ons/Tracker/$(TRACKER_ADDON)'

uninstall-addon:
	rm -f '$(HOME)/config/non-packaged/add-ons/Tracker/$(TRACKER_ADDON)'
	@echo 'Removed Tracker add-on'

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Auto-generated header dependencies (-MMD writes a .d next to each .o).
DEPS = $(CORE_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(GUI_OBJS:.o=.d) $(MD4C_OBJ:.o=.d)
-include $(DEPS)

TEXTVIEW_OBJS = src/gui/textview/Bullet.o src/gui/textview/BulletData.o \
    src/gui/textview/CharacterStyle.o src/gui/textview/CharacterStyleData.o \
    src/gui/textview/ImageCache.o src/gui/textview/MarkdownParser.o \
    src/gui/textview/Paragraph.o src/gui/textview/ParagraphLayout.o \
    src/gui/textview/ParagraphStyle.o src/gui/textview/ParagraphStyleData.o \
    src/gui/textview/StyleColoursData.o src/gui/textview/TextDocument.o \
    src/gui/textview/TextDocumentLayout.o src/gui/textview/TextDocumentView.o \
    src/gui/textview/TextEditor.o src/gui/textview/TextListener.o \
    src/gui/textview/TextSelection.o src/gui/textview/TextSpan.o \
    src/gui/textview/TextView.o src/gui/textview/UndoableEditListener.o \
    $(MD4C_OBJ)

# --- Tests (minimal self-contained runners, no framework) ---

TEST_BINS = test_json test_session test_tools test_sse test_workspace test_expected test_loop \
            test_desktop_script test_image test_schedule test_routine \
            test_tracker_addon test_syntax test_oauth test_agent_store \
            test_skills test_markdown test_rag test_history test_ipc \
            test_toolblock test_streaming_md test_tabs

test_json: tests/test_json.cpp src/json.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_loop: tests/test_loop.cpp \
	src/agent/Loop.o src/json.o src/config.o src/util.o src/workspace.o src/tools/Tool.o \
	src/tools/ReadFile.o src/tools/WriteFile.o src/tools/EditFile.o src/tools/Bash.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_expected: tests/test_expected.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
	$(CXX) $(CXXFLAGS) -o $@ $^

test_sse: tests/test_sse.cpp src/json.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_session: tests/test_session.cpp \
              src/storage/SessionStore.o src/json.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_tools: tests/test_tools.cpp \
            src/tools/Tool.o src/tools/ReadFile.o src/tools/WriteFile.o \
            src/tools/EditFile.o src/tools/Bash.o src/json.o src/workspace.o \
            src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_workspace: tests/test_workspace.cpp src/workspace.o src/json.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_desktop_script: tests/test_desktop_script.cpp \
                     src/haiku/DesktopTools.o src/tools/Tool.o src/json.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_image: tests/test_image.cpp src/json.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_schedule: tests/test_schedule.cpp src/schedule.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_routine: tests/test_routine.cpp src/routine.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_tracker_addon: tests/test_tracker_addon.cpp build-addon
	$(CXX) $(CXXFLAGS) -o $@ tests/test_tracker_addon.cpp -lbe

test_syntax: tests/test_syntax.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test_oauth: tests/test_oauth.cpp src/oauth.o src/json.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lcurl -lbe -lcrypto -lnetwork

test_agent_store: tests/test_agent_store.cpp src/agent_store.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_skills: tests/test_skills.cpp src/config.o src/oauth.o src/rag.o src/json.o \
             src/frontmatter.o src/text_index.o src/util.o src/cancel.o \
             src/provider/Anthropic.o src/provider/OpenAICompat.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lcurl -lbe -lcrypto -lnetwork

test_markdown: tests/test_markdown.cpp src/markdown.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_rag: tests/test_rag.cpp src/rag.o src/json.o src/text_index.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_history: tests/test_history.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -ledit

test_lsp: tests/test_lsp.cpp src/lsp/LspClient.o src/lsp/LspConfig.o src/json.o \
          src/workspace.o src/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_ipc: tests/test_ipc.cpp src/ipc/PlutoMessenger.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe

test_toolblock: tests/test_toolblock.cpp src/gui/ToolBlockManager.o $(TEXTVIEW_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lbe -ltranslation

test_streaming_md: tests/test_streaming_md.cpp src/gui/StreamingMarkdown.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_tabs: tests/test_tabs.cpp src/gui/ConversationView.o \
           src/gui/ChatColors.o src/gui/ToolBlockManager.o \
           src/gui/StreamingMarkdown.o $(TEXTVIEW_OBJS) \
           src/agent/Loop.o src/cancel.o src/storage/SessionStore.o \
           src/telemetry.o src/json.o src/tools/Tool.o \
           src/tools/ReadFile.o src/tools/WriteFile.o src/tools/EditFile.o \
           src/tools/Bash.o src/tools/Grep.o src/tools/Glob.o \
           src/tools/BeapiLookup.o \
           src/haiku/BfsTools.o src/haiku/DesktopTools.o \
           src/config.o src/oauth.o src/rag.o src/workspace.o \
           src/frontmatter.o src/text_index.o src/util.o \
           src/provider/Anthropic.o src/provider/OpenAICompat.o \
           src/mcp/Client.o src/mcp/Config.o src/mcp/Manager.o \
           src/mcp/Transport.o src/mcp/HttpTransport.o \
           src/mcp/StdioTransport.o src/mcp/SseSubscriber.o \
           src/mcp/SseTransport.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS) -ltranslation

# Compile GUI helper modules (used by tests independently of the full GUI).
src/gui/ToolBlockManager.o: src/gui/ToolBlockManager.cpp src/gui/ToolBlockManager.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/gui/StreamingMarkdown.o: src/gui/StreamingMarkdown.cpp src/gui/StreamingMarkdown.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bench: tests/bench.cpp $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "==> $$t"; ./$$t || exit 1; done
	@echo "all tests passed"

# Full CI-like validation: clean build from scratch + all tests.
# Works from any cwd by using $(MAKEFILE_DIR) for paths.
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ci: clean Pluto_cli Pluto check
	@echo ""
	@echo "=============================="
	@echo " CI: build OK, all tests pass"
	@echo "=============================="

# --- Packaging ---

package: Pluto_cli Pluto build-addon systemone_server
	rm -rf pkgroot
	mkdir -p pkgroot/apps pkgroot/bin \
	         pkgroot/add-ons/Tracker \
	         pkgroot/documentation/man/man1 \
	         pkgroot/documentation/packages/$(PKG_NAME)/examples
	cp Pluto_cli  pkgroot/bin/Pluto_cli
	cp Pluto      pkgroot/apps/Pluto
	cp systemone_server pkgroot/bin/systemone_server
	cp pluto.hvif pkgroot/apps/Pluto.hvif
	cp '$(TRACKER_ADDON)' 'pkgroot/add-ons/Tracker/$(TRACKER_ADDON)'
	cp docs/pluto.1 pkgroot/documentation/man/man1/pluto.1
	cp README.md CHANGELOG.md docs/architecture.md docs/user-guide.md \
	   docs/mcp-tutorial.md docs/bquery-cookbook.md \
	   pkgroot/documentation/packages/$(PKG_NAME)/
	cp -r examples/agents examples/routines examples/providers \
	   pkgroot/documentation/packages/$(PKG_NAME)/examples/
	mkdir -p pkgroot/data/pluto/providers
	cp data/beapi-index.jsonl pkgroot/data/pluto/
	cp data/providers/*.conf pkgroot/data/pluto/providers/
	cp data/lsp.conf pkgroot/data/pluto/
	cp .PackageInfo pkgroot/.PackageInfo
	cd pkgroot && package create -C . \
	   ../$(PKG_NAME)-$(PKG_VERSION)-1-$(PKG_ARCH).hpkg
	@echo "built $(PKG_NAME)-$(PKG_VERSION)-1-$(PKG_ARCH).hpkg"

clean:
	rm -f Pluto_cli Pluto systemone_server $(TEST_BINS) bench pluto.rsrc 'Ask pluto' \
	      $(CORE_OBJS) $(CLI_OBJS) $(GUI_OBJS) $(MD4C_OBJ) \
	      $(CORE_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(GUI_OBJS:.o=.d)
	rm -rf pkgroot $(PKG_NAME)-$(PKG_VERSION)-1-$(PKG_ARCH).hpkg

.PHONY: all clean check ci package install-addon uninstall-addon

# System One MCP server (stdio transport)
systemone_server: src/mcp/systemone_server.cpp src/json.o
	$(CXX) $(CXXFLAGS) -o $@ $< src/json.o $(LDLIBS)

