# pluto — architecture

This document describes the internal structure of pluto so that a new
contributor can make a change confidently. It focuses on responsibilities,
data flow, and the threading model. For features and roadmap see `TODO.md`.

## Overview

pluto is three binaries built from a shared core of ~40 compilation units:

- **`Pluto_cli`** — CLI with an interactive REPL and a `--one-shot` mode.
- **`Pluto`** — Haiku-native GUI using `BApplication` + `BWindow`.
- **`Ask pluto`** — Tracker add-on (shared library) for context-menu integration.

The split is intentional. The agent loop, provider, tool registry, MCP
client, and storage know nothing about UI. Both binaries assemble the same
objects in different entry points and register the same tools.

```
┌──────────────────────────────────────────────────────────────────────┐
│             Pluto_cli (CLI)              Pluto (GUI)                 │
│                   │                           │                      │
│           src/main.cpp                 src/gui/main_gui.cpp          │
│           src/repl.cpp                 src/gui/App.cpp               │
│                   │                    src/gui/MainWindow.cpp       │
│                   └───────────┬───────────────┘                      │
│                               ▼                                      │
│                         agent::Loop                                  │
│            ┌──────────────────┼───────────────────┐                  │
│            ▼                  ▼                   ▼                  │
│     provider::Provider   ToolRegistry         Config                 │
│            │                  │                                      │
│     Anthropic (SSE)      built-in tools +                           │
│     /v1/messages         MCP RemoteTools                             │
│                                │                                     │
│                           mcp::McpClient                             │
│                           (http / sse / stdio)                       │
└──────────────────────────────────────────────────────────────────────┘
```

## Modules

### `src/agent/Loop`

Owns one turn of the conversation. Takes the current message list, calls
`Provider::stream` until `stop_reason`, collects `tool_use` blocks, runs
each tool through the registry, appends `tool_result` messages, and loops
back until the assistant stops calling tools.

Public entries:
- `pluto::run_turn(provider, cfg, tools, messages, permission, ui)` — one
  turn of the agent loop.
- `pluto::maybe_compact(provider, cfg, messages, cumulative_tokens, ui)` —
  auto-compaction: when cumulative input tokens exceed 90% of the context
  window, asks the model to summarize older messages and replaces them with
  the summary. Called by the REPL after each `run_turn`.

`UiCallbacks` is the output surface: `on_text`, `on_tool_start`,
`on_tool_result`, `on_error`, `on_usage`, `on_status`. The REPL and the
GUI implement these differently; the loop doesn't care.

Plan mode: when `cfg.plan_mode` is set, `run_turn` filters out write tools
(write_file, edit_file, bash, write_attr, remove_attr, desktop_script,
launch_app) from the tool definitions and injects a `<mode>plan</mode>`
directive into the system prompt.

Tool-use detection: models that don't support tools (free-tier, small
models) trigger a retry without tools. The `tools_disabled` flag remembers
this per-model and auto-resets when the user switches models via `/model`.

### `src/provider/Provider`

Abstract base with two implementations: `AnthropicProvider` (SSE streaming
via `/v1/messages`) and `OpenAICompatProvider` (~320 LOC, supports Ollama,
OpenRouter, LM Studio, and any OpenAI-API-compatible server).

`Provider::stream(request, cb)` emits a sequence of `StreamEvent`s:
`TEXT_DELTA`, `TOOL_USE_START`, `TOOL_USE_INPUT_DELTA`, `TOOL_USE_END`,
`MESSAGE_END` (with usage + cache stats), or `ERROR`.

Anthropic streaming details:

- Wire protocol: Server-Sent Events from `/v1/messages`.
- Incremental SSE parser in `write_cb` + `process_event` handles
  `message_start`, `content_block_start/delta/stop`, `message_delta`, `error`.
- Prompt caching: `cache_control: {type: "ephemeral"}` on the system prompt
  block and on the last tool definition (caching the tools+system prefix).
- Retry loop: 3 attempts with exponential backoff 1s / 2s / 4s, retryable
  only on HTTP 429 / 5xx or pre-stream connection errors. Mid-stream
  failures are reported as errors (no duplicate output).
- Cancellation: `curl` progress callback polls `pluto::is_cancel_requested()`
  and returns non-zero to abort with `CURLE_ABORTED_BY_CALLBACK`.
- Exception safety: both `write_cb` (Anthropic) and `oai_write_cb`
  (OpenAI-compat) catch all C++ exceptions to prevent undefined behavior
  at the C ABI boundary of libcurl. A `cb_error` flag aborts the transfer
  cleanly via `CURLE_WRITE_ERROR`.

### `src/tools`

Tool interface:

```cpp
class Tool {
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual std::string input_schema_json() const = 0;
  virtual std::string preview(const std::string& input_json) const;
  virtual ToolResult execute(const std::string& input_json) = 0;
};
```

Built-in tools: `read_file`, `write_file`, `edit_file`, `bash`, `grep`,
`glob`, `web_fetch`, `web_search`. Haiku-specific tools in
`src/haiku/DesktopTools.cpp`: `desktop_script` (BMessage scripting),
`clipboard` (BClipboard get/set), `node_monitor` (BNodeMonitor watch).
`ToolRegistry` is just a `map<name, unique_ptr<Tool>>`.

`web_fetch` (`src/tools/WebFetch.cpp`) downloads a URL via libcurl, strips
HTML tags/scripts/styles and decodes entities, caps output at 32 KB.
`web_search` (`src/tools/WebSearch.cpp`) queries Brave/Tavily/SearXNG and
returns formatted results. Both are registered in CLI and GUI entry points.

`read_file` has size and binary guards: rejects files over 64 MB, and
probes the first 8 KB for null bytes before loading the full content.

### `src/haiku/BfsTools`

Haiku-specific tools that require `libbe`: `read_attr`, `write_attr`,
`list_attrs`, `remove_attr`, `query_fs`. These wrap `BNode`, `BQuery`,
`BVolumeRoster`, and `fs_attr.h`. They're the reason the CLI also links
`-lbe`.

### `src/mcp`

A minimal MCP (2024-11-05) client.

- `Transport` is the base; `HttpTransport`, `StdioTransport`,
  `SseTransport`, `SseSubscriber` are the implementations.
  `SseTransport::start()` validates that the server-supplied endpoint
  URL shares the same origin (scheme+host+port) as the configured SSE
  URL, preventing SSRF via malicious MCP servers.
- `McpClient::connect()` does `initialize` + `notifications/initialized` +
  `tools/list` over JSON-RPC 2.0.
- `register_mcp_tools(client, registry)` wraps every remote tool as a
  `RemoteTool` in the main `ToolRegistry`, prefixed `<server>__<tool>`.
  Per-app generated tools (name starting with `app_`) are filtered out to
  avoid overwhelming the model with hundreds of redundant wrappers.
- `McpManager::system_prompt_block()` generates a `<mcp_servers>` block
  listing connected servers and their tools, injected into the system prompt
  so the model knows what remote tools are available.
- `Config.cpp` reads `.mcp.json` / `~/config/settings/pluto/mcp.json` /
  `mcp-local.json`, merges them (local > project > user) with dedup
  warnings, and enforces per-project approval.
- `write_json_file()` uses atomic temp+rename for crash safety.
  `discover_mcp_servers()` requires HTTP 2xx (not 3xx) to report a
  server as live.

**Lifetime gotcha**: `RemoteTool` holds raw pointers to `McpClient`. The
clients must outlive the tool registry — the CLI and GUI both declare the
clients vector after the registry so destruction order works out (members
destruct in reverse, the clients die first, but `RemoteTool`'s destructor
doesn't touch the client so dangling is harmless).

### `src/config`

Plain `key=value` parser for `~/config/settings/pluto`. Recognized keys:
`provider`, `model`, `base_url`, `api_key_env`, `system`, `max_tokens`,
`context_window`, `cache`, `hook_pre_tool`, `hook_post_tool`,
`hook_user_prompt`.

**System prompt separation**: `Config` has two fields:
- `user_system_prompt` — the user-editable base prompt, read from and
  written to the `system =` config key. This is what the Preferences
  dialog shows.
- `system_prompt` — the composite prompt used at runtime: starts from
  `user_system_prompt`, then `apply_project_memory()` appends
  `<user_memory>` / `<project_memory>`, `apply_skills()` appends
  `<skills>`, and `McpManager::system_prompt_block()` appends
  `<mcp_servers>`. `save_config()` only persists `user_system_prompt`
  to avoid leaking runtime-injected content into the config file.

**Persistence safety**: `save_config()` sanitizes all string values
(strips `\n`/`\r`) before writing to prevent config injection.
`write_key_file()` creates temp files with `open(O_CREAT, 0600)` to
avoid a TOCTOU window. Legacy config migration
(`maybe_migrate_settings`) rolls back on `mkdir`/`rename` failure.

API key resolution: `getenv(cfg.api_key_env)` first, then
`~/config/settings/pluto-api-key` (single-line, chmod 600). `save_api_key()`
writes the fallback file.

### `src/cancel`

Thread-safe atomic bool with three functions: `request_cancel()`,
`clear_cancel()`, `is_cancel_requested()`. Set by the SIGINT handler in
`main.cpp`, polled by the curl progress callback in `Anthropic.cpp`,
cleared by the REPL at the start of each turn.

SIGINT is installed via `sigaction` with `SA_RESTART` so interrupted
syscalls (like `fgets` at the prompt) restart instead of returning `EOF`.
Pressing Ctrl+C at an empty prompt sets the flag but has no observable
effect; pressing it during a stream aborts curl cleanly, which surfaces
as an `ERROR` event, which the agent loop converts to a turn end.

### `src/json`

Self-contained JSON parser and serializer, ~250 lines. No exceptions
other than `std::runtime_error` on parse failure. `Value` is a
`std::variant<null, bool, double, string, array, object>`. Used everywhere:
request body construction, response parsing, tool input/output,
configuration. Strict: rejects bare `-` without digits, enforces
nesting depth limit (256), handles surrogate pairs.

Covered by `tests/test_json.cpp` via `make check`.

### `src/agent_store`

Agent package manager. An agent is a directory containing `AGENT.md` with
YAML-ish frontmatter (`name`, `description`, `author`, `version`, `tools`,
`skills`, `routines`, `commands`) and an optional prompt body.

- `install_agent()` copies the directory to `~/.../agents/<name>/`,
  registers bundled skills/routines/commands by copying them into the
  corresponding settings directories.
- `install_agent_from_url()` clones a Git repo to a temp dir, then calls
  `install_agent`. Saves the URL in `.agent-source` for future updates.
  All shell commands use `shq()` (single-quote escaping) to prevent
  command injection via malicious URLs.
- `update_agent()` re-clones the source, compares versions, removes the
  old agent, and reinstalls the new one.
- `preview_agent()` extracts metadata and body text for security review
  before installation (CLI: interactive prompt, GUI: BAlert).
- `remove_agent()` unregisters all bundled resources and deletes the
  agent directory.

Tests: `tests/test_agent_store.cpp` (15 test cases).

### `src/oauth`

OAuth 2.0 + PKCE authentication for Claude Max/Pro plans. Implements the
same flow as Claude Code: authorization code grant with S256 code challenge.

- `oauth_login()` binds a localhost server on a random port, opens the
  browser to `claude.com/cai/oauth/authorize`, waits for the callback,
  exchanges the code for tokens at `platform.claude.com/v1/oauth/token`.
- Token storage: `~/config/settings/pluto/oauth-tokens` (chmod 600).
- `oauth_ensure_valid_token()` checks expiry and refreshes if needed.
- `resolve_credentials()` in `config.cpp` tries OAuth first, falls back
  to API key.

Tests: `tests/test_oauth.cpp` (11 test cases).

### `src/routine`

Routine engine for recurring workflows. Routines are `.md` files in
`~/config/settings/pluto/routines/` with YAML frontmatter: `name`,
`schedule`, `model`, `tools`, `notify`, `enabled`.

- Template expansion: `{{date}}`, `{{clipboard}}`, `{{changed_files}}`.
- Schedules: `manual`, `every Nm`/`every Nh`, `on_boot`, `on_file_change <dir>`.
- `install_routine_job()` generates a shell script + `launch_daemon` job.
  All user-controlled values (routine name, watch directory, model) are
  shell-quoted with `shq()` to prevent injection in generated scripts.
- `set_routine_enabled()` rewrites the frontmatter's `enabled:` field.

GUI: `File > Routines…` opens `RoutineDialog` with list, details, and
Run/Enable/Disable/Log buttons.

Tests: `tests/test_routine.cpp` (10 test cases).

### `src/schedule`

Schedule manager wrapping `launch_daemon` for periodic or event-based
task execution.

- `schedule_save/load/list/remove()` manage config files in
  `~/config/settings/pluto/schedules/<name>.conf`.
- `schedule_install()` generates a launch_daemon job file. All
  interpolated values (prompt file path, model, schedule name) are
  shell-quoted with `shq()` to prevent injection in generated scripts.
- Supports periodic intervals and boot/login events.

GUI: `File > Schedules…` opens `ScheduleDialog` with Add/Remove/Run/
Enable/Disable buttons and a `BFilePanel` for selecting prompt files.

Tests: `tests/test_schedule.cpp`.

### `src/workspace`

Workspace trust system. Controls which directories pluto can read/write.

- `workspace_init()` captures cwd at startup.
- `workspace_allows(path)` checks if a path is inside the workspace or
  a trusted directory. Path canonicalization handles Haiku symlinks
  (`/tmp` → `/boot/system/cache/tmp`).
- `canonicalize()` resolves existing paths via `realpath(3)`. For
  non-existent paths, it resolves the longest existing parent prefix and
  appends the remainder, then normalizes the result with `normalize_path()`
  to eliminate `.` and `..` components. This prevents path traversal
  attacks where `workspace + "/../../../../etc/shadow"` would bypass the
  `starts_with` prefix check.
- First-run prompt: `[y]once [a]always [n]o`. `a` persists in
  `~/config/settings/pluto/trusted_workspaces`.
- All tool operations (read_file, write_file, edit_file, bash, attrs)
  call `workspace_allows()` before executing.
- Access logging: every allow/deny appended to `access.log`.

GUI: workspace indicator in status bar (✓/⚠ + directory name). Drag&drop
of files outside workspace triggers a `BAlert` to trust the parent.

Tests: `tests/test_workspace.cpp` (7 test cases, including path traversal).

### `src/telemetry`

Opt-in local usage tracking. Enabled by the existence of
`~/config/settings/pluto/telemetry` (marker file).

- CSV append to `stats.csv` with per-turn token usage.
- `pluto stats` shows aggregated table: model, tokens in/out, turns,
  sessions, estimated cost USD.
- Integrated in CLI (`repl.cpp` on_usage) and GUI (kMsgWorkerUsage).

### `src/rag`

Local BeAPI knowledge retrieval. Loads `data/beapi-index.jsonl` (390
deduplicated code snippets covering 202 Be classes) into memory and
provides keyword + TF-IDF search.

- `rag_init(path)` — loads the JSONL index (~500 KB, ~200 KB in memory).
- `rag_search(query, top_k)` — scores entries by: class name match
  (5x IDF), topic keyword (3x), question keyword (1x). Returns top-k
  `RagResult` with pointer to entry + score. ~1ms per query.
- `rag_extract_classes(text)` — finds Be class names (`B[A-Z]...`)
  with word-boundary and min-length filtering.
- IDF table precomputed at init time from topic + question + classes.

The loop (`Loop.cpp`) auto-injects up to 3 relevant snippets into the
system prompt when the user message mentions Be classes, and notifies
the GUI via `on_rag_inject` callback. The `beapi_lookup` tool
(`src/tools/BeapiLookup.cpp`) allows the model to search proactively.
`apply_rag_hint()` in `config.cpp` adds a prompt hint when the index
is loaded. Custom user snippets are loaded from
`~/config/settings/pluto/beapi-custom.jsonl` at startup.
`rag_export_bfs()` creates BFS files with `beapi:topic`/`beapi:classes`
attributes for Tracker integration. Attributes are written via the native
`BNode::WriteAttrString()` API (no shell commands).

Tests: `tests/test_rag.cpp` (10 test cases).

### `src/filelock.h`

Header-only RAII file lock using `flock(2)`. `ScopedFileLock` acquires
an exclusive lock on construction and releases on destruction. Supports
timeout with 10ms retry polling (default 2s). Used by all writers that
touch shared config/state files to allow safe concurrent instances.

### `src/gui/ConversationView`

Per-conversation `BView` that owns all state specific to a single
conversation tab: the chat output text view, message history, tool block
state, streaming markdown state, worker thread, token counters, and
session metadata. Each tab in the `BTabView` is a `ConversationView`
instance. Communicates back to `MainWindow` via `ConversationDelegate` —
a pure virtual interface with callbacks for status updates, side panel
content, tool approval, and turn lifecycle.

**State owned per tab**: `fMessages` (conversation history),
`fToolBlocks` (ToolBlockManager), `fStreamingMd` (StreamingMarkdown),
`fWorker` (thread_id), `fBusy`, `fTextOngoing`, token counters
(in/out/cache), session metadata (path, title, turn count),
`fAlwaysApproved` (tool approval set), rendering state (cursor,
shimmer, assistant header offsets).

**Public API**: `SendInput(blocks)` starts a turn, `Cancel()` aborts,
`Reset()` clears state, `LoadSession(path)` / `SaveSession()` for
persistence. `Handle*()` methods (HandleTextDelta, HandleToolStart,
HandleToolResult, HandleError, HandleUsage, HandleTurnDone,
HandleStatusMessage, HandleRagHint) are called from `MessageReceived`
to process worker thread events.

**Worker thread**: spawned per turn via `_StartTurn()`. Posts `BMessage`s
to `BMessenger(this)` (the ConversationView itself, not MainWindow).
`ConversationView::MessageReceived` handles all `kMsgWorker*` messages
and delegates to the appropriate `Handle*()` method, then calls
`ConversationDelegate` callbacks for side-panel / status-bar updates.

Tests: `tests/test_tabs.cpp` (17 test cases covering state isolation
between tabs, independent token accounting, delegate routing,
reset isolation, turn lifecycle).

### `src/gui/ToolBlockManager`

Standalone class managing collapsible tool blocks in a `BTextView`.
Encapsulates the `ToolBlock` struct (header/body offsets, body text cache,
collapsed state) and all offset adjustment logic for collapse/expand.
Arrow glyph replacement is delegated via a function pointer callback to
keep color/styling logic out of this class. Used by `ConversationView`
internally; testable independently with a headless `BTextView`.

Tests: `tests/test_toolblock.cpp` (9 test cases).

### `src/gui/StreamingMarkdown`

Line-buffering state machine for streaming markdown (legacy, kept for
compatibility). No longer used for visual rendering — replaced by
`MarkdownParser` in `HandleTextDelta` which accumulates markdown and
re-renders via md4c on each streaming chunk.

Tests: `tests/test_streaming_md.cpp` (13 test cases).

### `src/gui/textview/` — MarkdownViewer engine

Rich text engine imported from https://codeberg.org/Freaxed/MarkdownViewer
(MIT license). Key components:

- **`TextDocument`** — document model: ordered list of `Paragraph` objects.
  Each `Paragraph` contains `TextSpan` objects with `CharacterStyle`
  (font, color, face) and `ParagraphStyle` (spacing, indentation, bullets).
- **`MarkdownParser`** — converts markdown text to `TextDocument` using
  `md4c` (C parser by Martin Mitáš). Handles headings, bold, italic,
  code blocks, lists, blockquotes, horizontal rules, images, links.
- **`TextDocumentView`** — `BView` subclass that renders a `TextDocument`
  with word-wrap, selection, scrolling. Used as the chat output view.
- **`ParagraphLayout`** — text layout engine with glyph measurement,
  line breaking, font/color per span.
- **`ImageCache`** — bitmap caching for inline images (future use).

### `src/ipc/PlutoMessenger`

Inter-instance communication via native Haiku `BMessenger` and `BRoster`.

- `find_peers()` enumerates running Pluto GUI instances via
  `BRoster::GetAppList`, queries each for Workspace/Model/Busy/SessionTitle
  via the BeOS scripting protocol.
- `find_peer(id)` resolves a peer by numeric team_id or workspace substring.
- `send_to()` / `broadcast()` / `query()` send `BMessage`s to peers.
- `check_workspace_collision()` detects if another instance uses the same
  working directory and returns the conflicting peer's info.
- Message types: `kMsgPeerPing`, `kMsgPeerPong`, `kMsgPeerPrompt`,
  `kMsgPeerGetInfo`, `kMsgPeerAsk` (inject prompt into peer conversation),
  `kMsgPeerDelegate` (inject prompt, notify on completion).
- CLI commands `/ask` and `/delegate` use `send_to()` with `kMsgPeerAsk`/
  `kMsgPeerDelegate`. The GUI's `App::MessageReceived` forwards these to
  `MainWindow`, which injects the prompt into the conversation.

### Skills and custom commands (in `src/config.cpp`)

- **Skills**: `apply_skills()` scans `~/config/settings/pluto/skills/`,
  loads SKILL.md files. Always-on skills are injected into `<skills>` in
  the system prompt. Triggered skills (with `trigger:` frontmatter) are
  listed in `<available_skills>` and injected on demand by
  `inject_triggered_skills()` when the user message matches a keyword.
- **Custom commands**: loaded in `repl.cpp` from
  `~/config/settings/pluto/commands/*.md`. Optional `description:`
  frontmatter. Body supports `{{args}}` substitution.
- **Hooks**: `hook_pre_tool`, `hook_post_tool`, `hook_user_prompt` are
  shell commands executed around tool calls and user input, with
  `PLUTO_TOOL` or `PLUTO_PROMPT` in the environment.

Tests: `tests/test_skills.cpp` (6 test cases).

## GUI threading model

The GUI is single-process, multi-tab. **Each tab has its own worker
thread** — one thread per in-flight turn, per tab. Multiple tabs can
have concurrent turns (each ConversationView spawns its own worker).

```
UI thread (BLooper)            Worker thread (spawn_thread)
───────────────────            ───────────────────────────────
MainWindow::_SendCurrentInput
  → _ActiveConv()->SendInput(blocks)
     ├── PrepareTurn(blocks)
     ├── delegate->OnTurnStarted()  → disable input
     └── _StartTurn()
           └── spawn_thread(_WorkerEntry)
                                 WorkerCtx received
                                   │
                                   ▼
                                 pluto::run_turn(...)
                                   │
                                   │  ui.on_text → BMessage(kMsgWorkerText)
                                   │  ui.on_tool_start → BMessage(kMsgWorkerTS)
                                   │  ui.on_tool_result → BMessage(kMsgWorkerTR)
                                   │  ui.on_usage → BMessage(kMsgWorkerUsage)
                                   ▼
                                 target.SendMessage(&m)
                                   (target = BMessenger(ConversationView))
   ◀──────────── BMessage arrives
ConversationView::MessageReceived(kMsgWorker*)
  → Handle*()                  (rendering + state)
  → delegate->OnToolOutput()   (side panel)
  → delegate->OnStatusChanged()(status bar)
...
                                 run_turn returns
                                 target.SendMessage(kMsgWorkerDone)
   ◀────────────
ConversationView::MessageReceived(kMsgWorkerDone)
  → HandleTurnDone()
  → delegate->OnTurnDone(preview)  → re-enable input
```

The worker holds a `BMessenger` targeting the specific `ConversationView`
(not `MainWindow`). All UI mutations happen on the BLooper thread inside
`ConversationView::MessageReceived`. The worker never touches `BView`
state directly. Because each ConversationView receives its own messages,
a background tab's worker posts to the correct tab — the active tab's
rendering is not affected.

Shared mutable state: `std::vector<Message>* messages` is passed to the
worker via `MutableMessages()`. While a tab's worker runs, `fBusy` is
true, so `SendInput()` returns early — no concurrent access to the
vector. Other tabs are unaffected: they have separate message vectors.

**Cancel scope**: `pluto::request_cancel()` sets a process-global atomic
flag polled by curl's progress callback. Pressing Stop cancels the
in-flight stream for whichever tab last called `request_cancel()`.
Since the cancel flag is cleared at the start of each turn, concurrent
turns are safe — only the most recent cancel request takes effect.

## GUI components

`ConversationView` owns all per-conversation state:

- **`TextDocumentView`** (from MarkdownViewer) — native Haiku document
  view replacing the old `ChatOutput` (`BTextView`). Uses a structured
  document model (`TextDocument` → `Paragraph` → `TextSpan`) with
  `MarkdownParser` (based on md4c) for rendering headings, bold, italic,
  code blocks, lists, blockquotes natively. Source: `src/gui/textview/`
  (~9500 LOC, MIT license, Stephan Aßmus + Andrea Anzani).
- **Message history** (`std::vector<Message>`) — the conversation.
- **`ToolBlockManager`** — collapsible tool block state.
- **`MarkdownParser`** — converts markdown text to `TextDocument`
  paragraphs. Used during streaming (re-renders on each chunk) and
  session replay.
- **Rendering state** — cursor, assistant header offsets, accumulated
  markdown buffer (`fAsstMarkdownBuf`).
- **Token accounting** — session totals + per-turn deltas.
- **Session metadata** — path, title, turn count, turn start time.

`MainWindow` owns window-level chrome:

- **Menu bar** — File / Session / MCP / Help. MCP menu rebuilt from the
  live client list.
- **`BSplitView`** (horizontal) — `BTabView` on the left, detail
  panel on the right (collapsible).
- **`PlutoTabView`** (BTabView subclass) — contains one `ConversationView`
  per tab (max 10). Override `Select()` posts `kMsgTabChanged` to MainWindow.
  `_ActiveConv()` returns the selected tab's ConversationView. File > New
  tab (Cmd+T) creates tabs, File > Close tab (Cmd+W) removes them. Tab
  labels auto-update from session titles.
- **`ChatInput`** (custom `BTextView`) — multiline, Enter sends,
  Shift+Enter inserts a newline.
- **Status bar** (`BStringView`) — "Ready / Thinking / Running tool" +
  model + tok + ctx %.
- **`TokenGauge`** (custom `BView`) — horizontal bar, color shifts
  green → yellow → orange → red.
- **`BMessageRunner`** — 500 ms tick forwarded to `fConv->OnCursorTick()`.
- **Side panel** — tool output, edit diffs, image preview, RAG topics.

## Build system

Makefile with three source groups:

- `CORE_SRCS` — everything shared: config, json, cancel, agent, tools,
  haiku tools, mcp, providers.
- `CLI_SRCS` — `main.cpp`, `repl.cpp`, `mcp/Cli.cpp`.
- `GUI_SRCS` — `gui/main_gui.cpp`, `gui/App.cpp`, `gui/MainWindow.cpp`,
  `gui/SettingsDialog.cpp`, `gui/AgentStoreDialog.cpp`,
  `gui/RoutineDialog.cpp`, `gui/ScheduleDialog.cpp`,
  `gui/SessionBrowser.cpp`, `gui/DeskbarReplicant.cpp`.

`LDLIBS = -lcurl -lbe -lcrypto -lnetwork -ledit` for everything.
GUI adds `-ltracker -ltranslation`.

Targets:
- `make` (default) — builds `Pluto_cli`.
- `make Pluto` — builds the GUI.
- `make check` — compiles and runs all 21 test suites.
- `make ci` — clean build + full test suite.
- `make package` — creates `.hpkg` for HaikuDepot.
- `make clean` — removes objects and binaries.

Header dependency tracking via `-MMD -MP` ensures ABI changes trigger
the correct recompilation.

## Testing

Minimal self-contained tests in `tests/`, no framework. Each test is a
standalone binary compiled by `make check`: `test_json`, `test_session`,
`test_sse`, `test_tools`, `test_workspace`, `test_desktop_script`,
`test_image`, `test_schedule`, `test_routine`, `test_tracker_addon`,
`test_syntax`, `test_oauth`, `test_agent_store`, `test_skills`,
`test_markdown`, `test_rag`, `test_history`, `test_ipc`,
`test_toolblock`, `test_streaming_md`, `test_tabs`.

Extend by adding new `test_*.cpp` files and adding them to the
`TEST_BINS` list in the Makefile.
