cmake_minimum_required(VERSION 3.20)
project(Pluto VERSION 0.7.1 LANGUAGES CXX C)

# =============================================================================
# Haiku detection & SDK paths
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HAIKU_OS TRUE)
    message(STATUS "Building for Haiku")
else()
    set(HAIKU_OS FALSE)
    message(STATUS "Cross-compiling for Haiku from ${CMAKE_SYSTEM_NAME}")
endif()

# =============================================================================
# C++ standard & warnings
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMMON_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)
    if(HAIKU_OS)
        set(COMMON_WARNINGS ${COMMON_WARNINGS} -Wno-deprecated-declarations)
    endif()
    add_compile_options(${COMMON_WARNINGS})
endif()

# =============================================================================
# Optimization
# =============================================================================
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")

# =============================================================================
# Find dependencies
# =============================================================================
find_package(PkgConfig REQUIRED)

if(HAIKU_OS)
    # On Haiku: use system pkg-config
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(BE REQUIRED be)
    pkg_check_modules(CRYPTO REQUIRED libcrypto)
    pkg_check_modules(NETWORK REQUIRED network)
    pkg_check_modules(EDIT REQUIRED libedit)
    pkg_check_modules(TRACKER REQUIRED tracker)
    pkg_check_modules(TRANSLATION REQUIRED translation)
    pkg_check_modules(LOCALE REQUIRED locale)
    pkg_check_modules(SUPPORT REQUIRED support)
    pkg_check_modules(INTERFACE REQUIRED interface)
    pkg_check_modules(STORAGE REQUIRED storage)
    pkg_check_modules(APP REQUIRED app)
    pkg_check_modules(KERNEL REQUIRED kernel)
    pkg_check_modules(SQLITE3 sqlite3)
else()
    # Cross-compile: provide mock pkg-config via toolchain
    find_library(CURL curl REQUIRED)
    find_library(CRYPTO crypto REQUIRED)
    find_library(EDIT edit REQUIRED)
    find_library(SQLITE3 sqlite3 REQUIRED)
    # Haiku libs not available - mock via interface libraries
    add_library(be INTERFACE IMPORTED)
    add_library(network INTERFACE IMPORTED)
    add_library(tracker INTERFACE IMPORTED)
    add_library(translation INTERFACE IMPORTED)
    set_target_properties(be network tracker translation PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/mock_headers/haiku")
endif()

# =============================================================================
# Include directories
# =============================================================================
include_directories(src)

if(HAIKU_OS)
    # Private Haiku headers
    include_directories(
        /boot/system/develop/headers/private/interface
        /boot/system/develop/headers/private/support
        /boot/system/develop/headers/private/locale
        /boot/system/develop/headers/private/shared
        /boot/system/develop/headers/private/kernel
        /boot/system/develop/headers/private/storage
        /boot/system/develop/headers/private/app
    )
    # System headers
    include_directories(
        SYSTEM
        /boot/system/develop/headers
        /boot/system/develop/headers/interface
        /boot/system/develop/headers/support
        /boot/system/develop/headers/storage
        /boot/system/develop/headers/app
        /boot/system/develop/headers/kernel
        /boot/system/develop/headers/locale
        /boot/system/develop/headers/posix
        /boot/system/develop/headers/bsd
        /boot/system/develop/headers/compatibility
    )
else()
    include_directories(mock_headers/haiku)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        include_directories(/opt/homebrew/include)
    endif()
endif()

# =============================================================================
# Compile definitions
# =============================================================================
add_compile_definitions(__HAIKU__)

# =============================================================================
# Source files
# =============================================================================
set(CORE_SRCS
    src/lsp/LspClient.cpp
    src/lsp/LspConfig.cpp
    src/agent_store.cpp
    src/cancel.cpp
    src/markdown.cpp
    src/config.cpp
    src/rag.cpp
    src/wiki.cpp
    src/routine.cpp
    src/schedule.cpp
    src/telemetry.cpp
    src/frontmatter.cpp
    src/text_index.cpp
    src/util.cpp
    src/workspace.cpp
    src/json.cpp
    src/storage/SessionStore.cpp
    src/agent/Loop.cpp
    src/tools/Tool.cpp
    src/tools/ReadFile.cpp
    src/tools/WriteFile.cpp
    src/tools/EditFile.cpp
    src/tools/Bash.cpp
    src/tools/Grep.cpp
    src/tools/Glob.cpp
    src/tools/BeapiLookup.cpp
    src/tools/WikiLookup.cpp
    src/tools/WebFetch.cpp
    src/tools/WebSearch.cpp
    src/tools/TodoWrite.cpp
    src/tools/ApplyPatch.cpp
    src/tools/Lsp.cpp
    src/tools/GraphQuery.cpp
    src/map/Graph.cpp
    src/map/Scanner.cpp
    src/map/BfsAttrs.cpp
    src/systemone/SystemOneConfig.cpp
    src/systemone/DecisionModel.cpp
    src/systemone/RemoteDecisionModel.cpp
    src/systemone/LocalDecisionModel.cpp
    src/systemone/SystemOneClient.cpp
    src/systemone/TokenSaver.cpp
    src/mcp/builtin/builtin_servers.cpp
    src/mcp/builtin/mcp_builtin_manager.cpp
    src/mcp/builtin/mcp_tool_wrapper.cpp
    src/tools/mcp_serena.cpp
    src/tools/mcp_context7.cpp
    src/tools/mcp_deepwiki.cpp
    src/tools/mcp_rea.cpp
    src/tools/mcp_memory.cpp
    src/tools/mcp_playwright.cpp
    src/tools/mcp_sqlite.cpp
    src/mcp/Client.cpp
    src/mcp/Config.cpp
    src/mcp/Manager.cpp
    src/mcp/Transport.cpp
    src/mcp/HttpTransport.cpp
    src/mcp/StdioTransport.cpp
    src/mcp/SseSubscriber.cpp
    src/mcp/SseTransport.cpp
    src/provider/Provider.cpp
    src/provider/Anthropic.cpp
    src/provider/OpenAICompat.cpp
)

set(HAIKU_SRCS
    src/haiku/BfsTools.cpp
    src/haiku/DesktopTools.cpp
    src/haiku/PluginLoader.cpp
    src/haiku/Notifications.cpp
    src/haiku/ModelCaps.cpp
    src/haiku/AsyncStream.cpp
    src/haiku/HaikuAbstraction.cpp
    src/ipc/PlutoMessenger.cpp
    src/oauth.cpp
)

set(CLI_SRCS
    src/repl.cpp
    src/mcp/Cli.cpp
    src/cli/CliApp.cpp
    src/cli/ProviderFactory.cpp
    src/cli/ToolRegistryBuilder.cpp
    src/cli/McpBootstrap.cpp
)

set(GUI_SRCS
    src/gui/main_gui.cpp
    src/gui/App.cpp
    src/gui/MainWindow.cpp
    src/gui/SlashCommands.cpp
    src/gui/SessionManager.cpp
    src/gui/ConversationView.cpp
    src/gui/ChatColors.cpp
    src/gui/ToolBlockManager.cpp
    src/gui/StreamingMarkdown.cpp
    src/gui/SettingsDialog.cpp
    src/gui/AgentStoreDialog.cpp
    src/gui/RoutineDialog.cpp
    src/gui/ScheduleDialog.cpp
    src/gui/SessionBrowser.cpp
    src/gui/MiniMapView.cpp
    src/gui/GraphView.cpp
    src/gui/GraphWindow.cpp
    src/gui/TelemetryDialog.cpp
    src/gui/DeskbarReplicant.cpp
    src/gui/ColumnListView.cpp
    src/gui/ColumnTypes.cpp
    src/gui/textview/Bullet.cpp
    src/gui/textview/BulletData.cpp
    src/gui/textview/CharacterStyle.cpp
    src/gui/textview/CharacterStyleData.cpp
    src/gui/textview/ImageCache.cpp
    src/gui/textview/MarkdownParser.cpp
    src/gui/textview/Paragraph.cpp
    src/gui/textview/ParagraphLayout.cpp
    src/gui/textview/ParagraphStyle.cpp
    src/gui/textview/ParagraphStyleData.cpp
    src/gui/textview/StyleColoursData.cpp
    src/gui/textview/TextDocument.cpp
    src/gui/textview/TextDocumentLayout.cpp
    src/gui/textview/TextDocumentView.cpp
    src/gui/textview/TextEditor.cpp
    src/gui/textview/TextListener.cpp
    src/gui/textview/TextSelection.cpp
    src/gui/textview/TextSpan.cpp
    src/gui/textview/TextView.cpp
    src/gui/textview/UndoableEditListener.cpp
)

# md4c is C
set(MD4C_SRC src/gui/textview/md4c.c)

# =============================================================================
# Core library
# =============================================================================
if(HAIKU_OS)
    set(ALL_CORE_SRCS ${CORE_SRCS} ${HAIKU_SRCS})
else()
    set(ALL_CORE_SRCS ${CORE_SRCS})
endif()

add_library(pluto_core STATIC ${ALL_CORE_SRCS})
target_link_libraries(pluto_core PUBLIC ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} ${SQLITE3_LIBRARIES} be network)
target_include_directories(pluto_core PUBLIC src)

# =============================================================================
# md4c object
# =============================================================================
add_library(md4c STATIC ${MD4C_SRC})
target_compile_options(md4c PRIVATE -Wall -O2)

# =============================================================================
# Pluto_cli
# =============================================================================
add_executable(Pluto_cli ${CLI_SRCS})
target_link_libraries(Pluto_cli pluto_core ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} be network)

# =============================================================================
# Pluto GUI
# =============================================================================
add_executable(Pluto ${GUI_SRCS})
target_link_libraries(Pluto pluto_core md4c ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} be network tracker translation)
target_compile_definitions(Pluto PRIVATE PLUTO_GUI=1)

# =============================================================================
# Tracker add-on
# =============================================================================
add_library(Ask_pluto MODULE src/haiku/TrackerAddOn.cpp)
target_link_libraries(Ask_pluto be)
set_target_properties(Ask_pluto PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "Ask pluto"
)

# =============================================================================
# System One MCP server
# =============================================================================
add_executable(systemone_server src/mcp/systemone_server.cpp)
target_link_libraries(systemone_server pluto_core ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} be network)

# =============================================================================
# Resources (Haiku only)
# =============================================================================
if(HAIKU_OS)
    find_program(RC rc)
    find_program(XRES xres)
    find_program(RESATTR resattr)
    find_program(ADDATTR addattr)

    if(RC AND XRES AND RESATTR AND ADDATTR)
        add_custom_command(
            OUTPUT pluto.rsrc
            COMMAND ${RC} -o pluto.rsrc pluto.rdef
            DEPENDS pluto.rdef pluto.hvif
            COMMENT "Compiling resources"
        )

        add_custom_target(pluto_rsrc DEPENDS pluto.rsrc)

        add_custom_command(
            TARGET Pluto_cli POST_BUILD
            COMMAND ${XRES} -o Pluto_cli pluto.rsrc
            COMMAND ${RESATTR} -o Pluto_cli pluto.rsrc
            COMMAND ${ADDATTR} -t icon -f pluto.hvif BEOS:ICON Pluto_cli
            COMMENT "Embedding resources into Pluto_cli"
        )

        add_custom_command(
            TARGET Pluto POST_BUILD
            COMMAND ${XRES} -o Pluto pluto.rsrc
            COMMAND ${RESATTR} -o Pluto pluto.rsrc
            COMMAND ${ADDATTR} -t icon -f pluto.hvif BEOS:ICON Pluto
            COMMENT "Embedding resources into Pluto"
        )

        add_custom_command(
            TARGET "Ask pluto" POST_BUILD
            COMMAND ${ADDATTR} -t icon -f pluto.hvif BEOS:ICON "Ask pluto"
            COMMENT "Embedding icon into Tracker add-on"
        )
    else()
        message(WARNING "Resource tools (rc, xres, resattr, addattr) not found - skipping resource embedding")
    endif()
endif()

# =============================================================================
# Tests
# =============================================================================
enable_testing()

function(add_pluto_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} pluto_core ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} ${SQLITE3_LIBRARIES} be network)
    if(HAIKU_OS)
        add_test(NAME ${name} COMMAND ${name})
    else()
        # On non-Haiku, only run tests that don't need libbe
        set(HAIKU_ONLY_TESTS test_session test_tools test_workspace test_desktop_script test_routine test_schedule test_rag test_tracker_addon test_oauth test_agent_store test_skills test_lsp test_ipc test_toolblock test_tabs)
        list(FIND HAIKU_ONLY_TESTS ${name} idx)
        if(idx EQUAL -1)
            add_test(NAME ${name} COMMAND ${name})
        endif()
    endif()
endfunction()

add_pluto_test(test_json tests/test_json.cpp)
add_pluto_test(test_expected tests/test_expected.cpp)
add_pluto_test(test_sse tests/test_sse.cpp)
add_pluto_test(test_image tests/test_image.cpp)
add_pluto_test(test_syntax tests/test_syntax.cpp)
add_pluto_test(test_markdown tests/test_markdown.cpp)
add_pluto_test(test_history tests/test_history.cpp)
add_pluto_test(test_streaming_md tests/test_streaming_md.cpp)
add_pluto_test(test_workspace tests/test_workspace.cpp)
add_pluto_test(test_session tests/test_session.cpp)
add_pluto_test(test_tools tests/test_tools.cpp)
add_pluto_test(test_desktop_script tests/test_desktop_script.cpp)
add_pluto_test(test_schedule tests/test_schedule.cpp)
add_pluto_test(test_routine tests/test_routine.cpp)
add_pluto_test(test_rag tests/test_rag.cpp)
add_pluto_test(test_oauth tests/test_oauth.cpp)
add_pluto_test(test_agent_store tests/test_agent_store.cpp)
add_pluto_test(test_skills tests/test_skills.cpp)
add_pluto_test(test_lsp tests/test_lsp.cpp)
add_pluto_test(test_ipc tests/test_ipc.cpp)
add_pluto_test(test_toolblock tests/test_toolblock.cpp)
add_pluto_test(test_tabs tests/test_tabs.cpp)

# =============================================================================
# Benchmarks
# =============================================================================
add_executable(bench tests/bench.cpp)
target_link_libraries(bench pluto_core ${CURL_LIBRARIES} ${CRYPTO_LIBRARIES} ${EDIT_LIBRARIES} be network)

# =============================================================================
# Install targets (Haiku only)
# =============================================================================
if(HAIKU_OS)
    install(TARGETS Pluto_cli Pluto systemone_server DESTINATION bin)
    install(TARGETS Pluto DESTINATION apps)
    install(TARGETS "Ask pluto" DESTINATION add-ons/Tracker)
    install(FILES pluto.hvif DESTINATION apps)
    install(FILES docs/pluto.1 DESTINATION documentation/man/man1)
    install(FILES README.md CHANGELOG.md docs/architecture.md docs/user-guide.md docs/mcp-tutorial.md docs/bquery-cookbook.md DESTINATION documentation/packages/${PROJECT_NAME})
    install(DIRECTORY examples/ DESTINATION documentation/packages/${PROJECT_NAME}/examples)
    install(FILES data/beapi-index.jsonl DESTINATION data/pluto)
    install(DIRECTORY data/providers/ DESTINATION data/pluto/providers)
    install(FILES data/lsp.conf DESTINATION data/pluto)
endif()

# =============================================================================
# Package target (Haiku only)
# =============================================================================
if(HAIKU_OS)
    find_program(PACKAGE_CMD package)
    if(PACKAGE_CMD)
        add_custom_target(package
            COMMAND rm -rf pkgroot
            COMMAND mkdir -p pkgroot/apps pkgroot/bin pkgroot/add-ons/Tracker pkgroot/documentation/man/man1 pkgroot/documentation/packages/${PROJECT_NAME}/examples pkgroot/data/pluto/providers
            COMMAND cp Pluto_cli pkgroot/bin/Pluto_cli
            COMMAND cp Pluto pkgroot/apps/Pluto
            COMMAND cp systemone_server pkgroot/bin/systemone_server
            COMMAND cp pluto.hvif pkgroot/apps/Pluto.hvif
            COMMAND cp "Ask pluto" "pkgroot/add-ons/Tracker/Ask pluto"
            COMMAND cp docs/pluto.1 pkgroot/documentation/man/man1/pluto.1
            COMMAND cp README.md CHANGELOG.md docs/architecture.md docs/user-guide.md docs/mcp-tutorial.md docs/bquery-cookbook.md pkgroot/documentation/packages/${PROJECT_NAME}/
            COMMAND cp -r examples/agents examples/routines examples/providers pkgroot/documentation/packages/${PROJECT_NAME}/examples/
            COMMAND cp data/beapi-index.jsonl pkgroot/data/pluto/
            COMMAND cp data/providers/*.conf pkgroot/data/pluto/providers/
            COMMAND cp data/lsp.conf pkgroot/data/pluto/
            COMMAND cp .PackageInfo pkgroot/.PackageInfo
            COMMAND cd pkgroot && ${PACKAGE_CMD} create -C . ../${PROJECT_NAME}-${PROJECT_VERSION}-1-${CMAKE_SYSTEM_PROCESSOR}.hpkg
            COMMENT "Creating HPKG package"
            DEPENDS Pluto_cli Pluto systemone_server "Ask pluto"
        )
    endif()
endif()

# =============================================================================
# CI target
# =============================================================================
add_custom_target(ci
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target Pluto_cli Pluto
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check
    COMMENT "Full CI build + tests"
    DEPENDS Pluto_cli Pluto
)

# =============================================================================
# Clean
# =============================================================================
add_custom_target(clean_all
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/*
    COMMENT "Full clean"
)