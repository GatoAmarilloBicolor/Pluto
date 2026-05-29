# Pluto changelog

## 0.7.0 — 2026-05-30

### Added

- **Welcome wizard (CLI + GUI)** — first-run setup with "Quick start"
  section: Ollama (zero signup, offline) and OpenRouter (one signup,
  many free models) at the top. CLI shows all 15+ providers dynamically
  from .conf files. GUI shows a 3-button dialog (OpenRouter, Ollama,
  Preferences).
- **`/share` command** — export the current conversation as a Markdown
  file. `/share` auto-generates a filename from the session title;
  `/share path.md` writes to a specific file. Includes metadata header,
  user/assistant messages, and tool call blocks.
- **Together AI and Hugging Face providers** — added as bundled provider
  profiles (data/providers/14-together.conf, 15-huggingface.conf).
- **Base system prompt** — the model now knows it is Pluto, has access
  to tools, and knows the current workspace path.

### Fixed

- **GUI blank window on startup** — fixed race condition in
  `_RestoreWorkspace` that skipped tab creation for single-tab
  workspaces, leaving the window empty.
- **Provider switch not applied for keyless providers** — fixed
  `api_key_env` guard that prevented switching to Ollama/LM Studio
  from Preferences.
- **Thinking spinner flicker** — spinner now updates text in-place
  via `Replace()` instead of removing and re-adding the paragraph.
- **Path traversal in WorkspaceAllows** — fast path now rejects paths
  containing `..` even if they match the workspace prefix.
- **HTTP 400 with free models** — removed blanket tool blacklist for
  `:free` models; provider now auto-retries without tools on 400.
- **Provider .conf not found from GUI** — resolve data/providers path
  relative to executable, not working directory.
- **Test suite link errors** — fixed missing dependencies in 7 test
  targets after code cleanup.
- **test_session crash** — skip BQuery in test mode (requires
  BApplication context).
- **test_markdown/test_rag naming** — updated to UpperCamelCase API.

### Changed

- **`make` builds all three binaries** — default target changed from
  `Pluto_cli` to `all` (CLI + GUI + Tracker add-on).
- **OpenRouter config updated** — removed dead gemma-3-27b-it:free,
  added gemma-4-31b and qwen3-72b free models.
- **Groq config updated** — added qwen-qwq-32b model.

## 0.6.0 — 2026-05-02

### Added

- **Wiki LLM** — project knowledge base using markdown files with YAML
  frontmatter and BFS attributes. `pluto wiki ingest <url|file>` analyzes
  content via LLM and saves structured .md entries. `pluto wiki search`,
  `pluto wiki list`, `pluto wiki stats` for management. Tool `wiki_lookup`
  available to the model during conversations. Storage: `<workspace>/.pluto/wiki/`
  or `~/config/settings/pluto/wiki/`. BFS attributes (wiki:topic, wiki:tags,
  wiki:source) enable Tracker queries.
- **First-run wizard** — interactive provider picker on first launch (no
  config file). 7 providers: Anthropic, Gemini, OpenAI, Ollama, OpenRouter,
  xAI, Cerebras. Saves config immediately with model, base_url, api_key_env.
- **Cerebras provider** — added to first-run wizard, GUI settings, and
  provider profiles (data/providers/12-cerebras.conf).
- **GUI /wiki command** — `/wiki` shows wiki stats, `/wiki <url>` ingests
  content in background. Autocomplete entry added.

### Changed

- **OAuth check scoped to Anthropic** — `ResolveCredentials()` only checks
  OAuth tokens when provider is "anthropic". Non-Anthropic providers no
  longer see OAuth refresh errors.
- **Local servers no-key** — providers with empty `api_key_env` (Ollama,
  LM Studio) now get dummy credentials instead of "no credentials" error.

### Code quality (Haiku Coding Guidelines conformance)

- Extracted shared modules: `src/util.h` (HomeDir, SettingsDir, Slugify,
  Trim, PipeGuard), `src/text_index.h` (Tokenize, BuildIdf, ScoreDocument),
  `src/frontmatter.h` (ParseFrontmatter, SplitCsv).
- Renamed ~153 free functions from snake_case to UpperCamelCase across all
  modules (cancel, markdown, telemetry, workspace, oauth, rag, wiki,
  schedule, routine, agent_store, config, agent/Loop, tools, mcp).
- Fixed ~40 silent `catch(...)` blocks to `catch(const std::exception&)`.
- Configured `.clang-format` with full Haiku style rules (Allman braces for
  functions, return type on own line, constructor initializer colon on own
  line, Haiku include ordering).
- Extracted `gui/SlashCommands.cpp` (623 lines) and
  `gui/SessionManager.cpp` (197 lines) from MainWindow.cpp (5572 -> 4725).
- Added 24 unit tests for new modules (text_index, frontmatter, util).

### Previous (0.6.0-alpha, 2026-04-24)


- **Auto-compaction** — `maybe_compact()` in `Loop.{h,cpp}` automatically
  summarizes the conversation when cumulative input tokens exceed 90% of the
  context window. The summary replaces old messages, preserving the most
  recent user turn. Integrated in the REPL after each `run_turn`. Status
  callback shows "Compacting conversation…" during summarization.
- **Tool `web_fetch`** — `src/tools/WebFetch.{h,cpp}`. Fetches a URL via
  HTTP GET (libcurl), strips HTML tags/scripts/styles and decodes common
  entities. Output capped at 32 KB. Supports `raw` flag to skip stripping.
  Registered in both CLI and GUI.
- **Tool `web_search`** — `src/tools/WebSearch.{h,cpp}`. Searches the web
  with three backends: Brave Search (`BRAVE_API_KEY`), Tavily
  (`TAVILY_API_KEY`), or SearXNG (`SEARXNG_URL`). Auto-detects available
  engine. Returns formatted results with title, URL, and snippet.
- **Plan mode** — `cfg.plan_mode` flag, toggled via `/plan` in the REPL.
  Filters write tools (write_file, edit_file, bash, write_attr, remove_attr,
  desktop_script, launch_app) from tool definitions. Injects
  `<mode>plan</mode>` into the system prompt instructing the model to
  describe changes without making them.
- **Headless mode** — `pluto -p "prompt" [-f text|json] [-q]`. Executes a
  single turn with auto-approved tool calls and exits. `-f json` produces
  structured output with text, model, token counts, and tool_calls array.
  `-q` suppresses stderr status messages. Suitable for CI, scripting, and
  pipeline integration.

### Fixed

- **OAuth partial read** — `oauth.cpp` callback socket read now loops until
  `\r\n\r\n` (end of HTTP headers) or buffer full, instead of relying on a
  single `read()` call that could return partial data on slow connections.
- **Streaming hang prevention** — `Anthropic.cpp` now sets
  `CURLOPT_CONNECTTIMEOUT` (30s) and stall detection via
  `CURLOPT_LOW_SPEED_LIMIT` (1 byte/s for 120s). Previously a stalled
  connection would hang forever.
- **Credential leak in debug logs** — `Anthropic.cpp` now strips
  `user:pass@` from URLs before logging.
- **Agent run trust bypass** — `pluto agent run` now verifies workspace trust
  before auto-approving tool calls. Previously ran with full `--yolo`
  permissions regardless of trust status.
- **format_reply stack overflow** — `DesktopTools.cpp` `format_reply` now has
  a depth limit of 8 levels to prevent stack overflow on deeply nested
  BMessages.
- **pclose error handling** — `Bash.cpp` now detects `pclose()` returning -1
  and reports the error with `strerror` instead of passing a corrupt status
  to `WIFEXITED`.

---

## 0.5.0 — 2026-04-23

### Security

- **Fixed path traversal in workspace sandbox** — `canonicalize()` now
  normalizes `..` components after resolving the longest existing parent
  prefix. Previously a path like `<workspace>/../../../etc/shadow` would
  bypass the `starts_with` workspace check, allowing all file tools to
  read/write outside the authorized directory. New `normalize_path()`
  function performs lexical `.`/`..` resolution. Added
  `test_path_traversal_blocked` test with 6 assertions.
- **Fixed buffer overflow in `SessionStore::save`** — `snprintf` buffer
  was 48 bytes for a format requiring up to 66 bytes (timestamp + slug +
  hex suffix). Increased to 80 bytes.
- **Fixed command injection in agent store** — `git clone` and `rm -rf`
  commands now use `shq()` shell quoting for all interpolated values
  (URLs, paths). Previously a URL containing a single-quote could break
  out of the shell string and execute arbitrary commands. Applied to all
  5 `popen`/`system` call sites in `agent_store.cpp`.
- **Fixed script injection in routine engine** — `routine.cpp` now uses
  `shq()` for all user-controlled values (`name`, `watch_dir`, `model`,
  file paths) written into generated shell scripts and passed to
  `std::system()`. Previously a routine name containing shell
  metacharacters could execute arbitrary code.
- **Fixed script injection in schedule engine** — `schedule.cpp` now
  uses `shq()` for `prompt_file`, `model`, `name`, and the pluto binary
  path in all generated `launch_daemon` wrapper scripts and `system()`
  calls.
- **Eliminated shell injection in RAG BFS export** — `rag_export_bfs()`
  replaced `system("addattr ...")` with native `BNode::WriteAttrString()`
  API calls. No shell is invoked at all — class names and paths that
  previously went unescaped into `addattr` commands are now written
  directly via the Haiku storage kit.
- **Fixed SSRF in MCP SSE transport** — `resolve_url()` now validates
  that server-supplied endpoint URLs share the same origin
  (scheme+host+port) as the configured SSE URL. A malicious MCP server
  can no longer redirect JSON-RPC calls to `file:///`, internal hosts,
  or external servers. Mismatched origins cause `start()` to fail with
  a clear error message.
- **Fixed undefined behavior in curl write callbacks** — `write_cb`
  (Anthropic) and `oai_write_cb` (OpenAI-compat) now catch all C++
  exceptions before they cross the C ABI boundary of libcurl. A
  `cb_error` flag aborts the transfer cleanly via `CURLE_WRITE_ERROR`
  instead of causing undefined behavior (e.g. from `std::bad_alloc`).
- **Fixed permanent tool disablement after model error** — the
  `tools_disabled` flag in `Loop.cpp` now tracks which model caused the
  disablement and automatically resets when the user switches models
  via `/model`. Previously, a tool-unsupported error on a free-tier
  model would permanently disable tools for the entire session.
- **Fixed dangling pointer in agent loop** — `active_text` raw pointer
  into `std::vector<ContentBlock>` replaced with an integer index. The
  pointer could dangle after `push_back` reallocated the vector (e.g.
  when receiving text, image, and tool blocks in sequence).
- **Fixed config migration data loss** — `maybe_migrate_settings()` now
  checks return values of `mkdir` and `rename` and rolls back on
  failure, restoring the original config file. Previously a failed
  `mkdir` would strand the config at `<path>.migrating` permanently.
- **Fixed newline injection in config persistence** — `save_config()`
  now strips `\n`/`\r` from all string values before writing. A system
  prompt containing a newline followed by `provider = evil` could
  previously corrupt the config file and inject arbitrary settings.
- **Fixed API key file TOCTOU** — `write_key_file()` now creates the
  temp file with `open(O_CREAT|O_TRUNC, 0600)` instead of `ofstream`
  followed by `chmod`. The file is born with restricted permissions,
  eliminating the window where another process could read the key.
- **Fixed HTTPS image generation on Haiku** — `_StreamImageGen()` now
  sets `CURLOPT_CAINFO`, `CURLOPT_POST`, and `CURLOPT_POSTFIELDSIZE`,
  matching the streaming chat path. Previously HTTPS endpoints would
  fail with certificate verification errors on Haiku.
- **Fixed OOM on large file reads** — `ReadFile` now checks file size
  (64 MB limit) and does an 8 KB sentinel probe for binary detection
  before loading the full file. Previously a model requesting a multi-GB
  file would cause an out-of-memory kill.
- **Fixed `g_hist` null-deref in REPL** — `read_line()` now checks
  `g_hist` is non-null before calling `history()`. Prevents a crash
  if `history_init()` failed while `el_init()` succeeded.
- **Stricter JSON number parsing** — `parse_number()` now rejects a
  bare `-` without trailing digits (e.g. `"-}"`) as a parse error
  instead of silently producing `Value(-0.0)`.
- **Removed hard-coded development path** — `search_agents()` no longer
  references the author's home directory. Example agents are now found
  via the relative `examples/agents` path (source tree) or the system
  install path `/boot/system/data/pluto/examples/agents`.
- **Fixed MCP discovery accepting redirects** — `discover_mcp_servers()`
  now requires HTTP 2xx (was 2xx–3xx). A 301/302 redirect to an
  external host is no longer reported as a live MCP server.
- **Fixed CSV injection in telemetry** — model names containing commas
  are now sanitized before writing to `stats.csv`, preventing column
  misalignment.
- **Fixed world-readable routine temp files** — `routine run` now uses
  `mkstemp()` instead of a predictable path with `ofstream`. The temp
  file is created with mode 0600, preventing other users from reading
  the expanded prompt.
- **Removed unused include** — `<TranslatorRoster.h>` removed from
  `ReadFile.cpp` (no API from it was called).
- **Fixed OAuth token TOCTOU** — `save_oauth_tokens()` now creates
  the temp file with `open(O_CREAT, 0600)` instead of `ofstream` +
  `chmod`. Eliminates the window where another process could read tokens.
- **Fixed non-atomic MCP config writes** — `write_json_file()` now
  writes to a temp file and atomically renames it. A crash mid-write
  no longer corrupts the MCP approval/config files.
- **Fixed thread-unsafe setenv in REPL hooks** — `user_prompt_submit`
  hooks now use shell env-prefix (`PLUTO_PROMPT='...' cmd`) instead of
  `setenv`/`unsetenv`, which raced with the SSE background subscriber
  thread calling `getenv`.
- **Fixed slash command re-entry after custom expansion** — custom
  commands that expand to text starting with `/` no longer accidentally
  trigger built-in slash commands (`/untag`, `/tools`, etc.). The
  expanded body now skips the slash dispatch and goes directly to the
  user message path.
- **Fixed incomplete retry-without-tools path** — the retry streaming
  handler in `Loop.cpp` now handles `IMAGE_BLOCK` events (previously
  silently dropped). Images from models retried without tools are now
  correctly added to the conversation and shown in the UI.
- **Capped `raw_body` memory in OpenAI streaming** — `oai_write_cb`
  now stops accumulating `raw_body` after 64 KB. Previously the full
  response was stored twice (in `buffer` for SSE parsing and in
  `raw_body` for error reporting), doubling peak memory for large
  streaming responses.
- **Fixed Makefile `clean` target** — `clean` now removes `'Ask pluto'`
  (correct case) instead of `'Ask Pluto'` which left a stale binary on
  case-sensitive filesystems.
- **Fixed `test_tracker_addon` link** — added missing `-lbe` (uses
  `<image.h>` and `<OS.h>` from libbe).
- **Added header dependency tracking for md4c** — `md4c.o` now builds
  with `-MMD -MP` and its `.d` file is included in `DEPS`. Previously
  header changes in md4c were not tracked, causing stale-object builds.
  Also explicitly defines `CC` to avoid relying on Make's default.

### Added

#### CLI/GUI command alignment
- CLI now supports `/routines` (list available routines) and
  `/routine <name>` (expand and send a routine as a prompt in the current
  chat). Previously these were GUI-only.
- GUI now supports `/title [new]` (show or set session title),
  `/tag [name]` (show or add a tag), and `/untag <name>` (remove a tag).
  Previously these were CLI-only.
- Updated `/help` output in both CLI and GUI to list all available commands.
- Added autocomplete entries for `/title`, `/tag`, `/untag` in the GUI.

#### `/init` command — project memory scaffolding
- New `/init [--force]` command creates a `pluto.md` project memory file in
  the current workspace root.
- Auto-detects git remote, main programming language (by file extension
  counts), and build system (Makefile, CMakeLists.txt, Cargo.toml, etc.).
- Generates a template with detected info and an `## Instructions` section
  for user customization.
- Reloads project memory immediately so it takes effect without restarting.
- If `pluto.md` already exists, warns and suggests `--force` to overwrite.
- Works in both CLI and GUI (with autocomplete in the GUI input field).
- Shared implementation in `workspace.{h,cpp}` via `init_project_memory()`.

#### REPL line editing with libedit
- Replaced `std::fgets` with libedit (BSD editline) for interactive input.
- Arrow keys up/down navigate command history.
- Emacs keybindings: Ctrl-A (beginning of line), Ctrl-E (end), Ctrl-K (kill
  to end), Ctrl-W (delete word), Home/End.
- History is persistent: saved to `~/config/settings/pluto/history` (500
  entries) on exit, loaded on startup.
- Graceful fallback to `fgets` if libedit initialization fails.
- Build now links `-ledit`; `.PackageInfo` requires `lib:libedit`.
- `tests/test_history.cpp` — 7 tests: init/finalize, add/walk, save/load
  roundtrip, size limit, missing file, editline init, special characters.

#### Multi-instance IPC (`PlutoMessenger`)
- New `src/ipc/PlutoMessenger.{h,cpp}` — peer discovery and messaging API
  for communication between running Pluto instances.
- `find_peers()` enumerates all Pluto GUI instances via `BRoster::GetAppList`
  and queries each for workspace, model, session title, and busy state using
  the BeOS scripting protocol.
- `send_to(team_id, what, data)` sends a `BMessage` to a specific peer.
- `broadcast(what, data)` sends to all peers except self.
- `query(team_id, what, data, timeout_ms)` performs synchronous request/reply.
- New REPL command `/peers` — lists running Pluto GUI instances with PID,
  workspace, model, state (idle/busy), and session title in a formatted table.

#### Scripting suite in GUI App
- `App.cpp` now exposes a scripting suite via the standard BeOS protocol:
  - `Workspace` (string, read) — current working directory
  - `Model` (string, read) — active model ID
  - `SessionTitle` (string, read) — current session title
  - `Busy` (bool, read) ��� whether a turn is in progress
- Implemented via `GetSupportedSuites`, `ResolveSpecifier`, and
  `B_GET_PROPERTY` handler in `MessageReceived`.
- Testable from Terminal: `hey Pluto get Workspace`, `hey Pluto get Model`.
- `tests/test_ipc.cpp` — 7 tests: defaults, find_peers, send/query invalid,
  broadcast, message constants, field assignment.

#### GUI Agent Store dialog
- New `File > Agents…` menu item opens `AgentStoreDialog`.
- Lists all installed agents with name, version, and description.
- Detail panel shows author, version, required tools, bundled skills,
  routines, and commands for the selected agent.
- **Install…** button opens a native `BFilePanel` to browse for an
  agent directory from Tracker.
- **Remove** button with `BAlert` confirmation dialog. Unregisters
  all associated skills, routines, and commands.
- **Show prompt** button displays the agent's prompt body in a
  monospace text view.
- Drag & drop of agent directories onto the dialog window is supported.
- `src/gui/AgentStoreDialog.{h,cpp}` — new dialog.

#### Trigger-based skill loading
- Skills now support optional `trigger:` frontmatter in SKILL.md.
- Skills without triggers remain always-on (backward compatible).
- Skills with `trigger: keyword1, keyword2` are listed in an
  `<available_skills>` catalog in the system prompt but only injected
  into the active prompt when the user message contains a matching
  keyword (case-insensitive).
- `inject_triggered_skills()` in `config.cpp` handles on-demand injection.
- CLI REPL prints `[skill] activated: <name>` when a skill is triggered.
- `tests/test_skills.cpp` — 6 tests: always-on, triggered, injection on
  match, mixed, case-insensitive, empty skills dir.

#### Agent Store (`pluto agent install/remove/list/info`)
- New `src/agent_store.{h,cpp}` — agent package manager for Pluto.
- Agents are directories with `AGENT.md` containing YAML frontmatter
  (name, description, author, version, tools, skills, routines, commands)
  and a prompt body.
- `pluto agent install <path>` installs an agent from a local directory,
  copies all files to `~/config/settings/pluto/agents/<name>/`, and
  automatically registers bundled skills, routines, and commands.
- `pluto agent install <url>` clones a Git repository and installs agents
  found in it (supports both single-agent repos and multi-agent registries).
- `pluto agent remove <name>` removes an agent and unregisters its resources.
- `pluto agent list` shows installed agents with version and description.
- `pluto agent info <name>` shows full metadata and prompt body.
- `check_agent_deps()` validates that an agent's required tools are available.
- 4 example agents in `examples/agents/`:
  - `haiku-dev` — Haiku OS development expert (Be API, BFS, app_server)
  - `code-reviewer` — structured code review with severity ratings
  - `shell-assistant` — Haiku Terminal helper (bash, hey, pkgman)
  - `project-onboarder` — analyzes a repo and generates pluto.md
- `tests/test_agent_store.cpp` — 9 tests: parse with/without frontmatter,
  install local, duplicate detection, list (sorted), remove, dependency
  check, resource registration/cleanup, missing file handling.

#### OAuth authentication for Claude Max/Pro (`pluto login`)
- New `src/oauth.{h,cpp}` — full OAuth 2.0 + PKCE implementation.
- `pluto login` opens a browser for Claude account authentication,
  runs a local callback server, exchanges the authorization code for
  tokens, and saves them to `~/config/settings/pluto/oauth-tokens` (chmod 600).
- `pluto logout` removes saved tokens, reverting to API key auth.
- Token refresh is automatic — `resolve_credentials()` checks expiry and
  refreshes via the token endpoint before each session.
- `AnthropicProvider` now supports `AuthType::OAUTH` with
  `Authorization: Bearer` header and `anthropic-beta: oauth-2025-04-20`.
- PKCE helpers use OpenSSL (`EVP_Digest` for SHA256, `RAND_bytes`).
- `Credentials` struct in `config.h` — OAuth takes priority over API key.
- `tests/test_oauth.cpp` — 11 tests covering PKCE, token storage
  roundtrip, permissions, URL param extraction, expiry logic.
- Build now links `-lcrypto -lnetwork` in addition to `-lcurl -lbe`.

#### Inline image preview in side panel
- When a tool result is a path to an image file (PNG, JPEG, GIF, BMP,
  WebP), Pluto-gui decodes it via `BTranslatorRoster` and shows a
  scaled preview in the side panel below the text output.
- Custom `ImagePreviewView` renders the `BBitmap` with aspect-ratio
  scaling. Hidden when no image is displayed.
- Build now links `-ltranslation`.

#### Persistent font zoom
- Font zoom level (Cmd+=/Cmd+-/Cmd+0) is now saved to config as
  `font_delta` and restored on next launch.

#### Drag & drop attachment chips
- Dropped images now appear as individual chip buttons below the input,
  each with a 🖼 icon, filename, and × to remove. Replaces the old
  plain-text chip bar.

#### Model quick-switch from status bar
- Click on the status bar to open a popup with all available models.
- Models are fetched dynamically: Anthropic `/v1/models` (with OAuth or
  API key auth), Ollama `/api/tags`, or OpenAI `/v1/models`.
- Both providers shown together with section headers (`— Claude —`,
  `— hostname —`). Selecting a model from a different provider
  automatically switches the provider and recreates the connection.
- Model choice is saved to config and persisted across sessions.

#### Built-in `launch_app` tool
- New Haiku-native tool to launch applications by MIME signature via
  `be_roster->Launch()`. If already running, brings the app to front.
- Registered in both CLI and GUI.

#### Routine triggers (on_boot, on_file_change, periodic)
- Routines with automatic schedules (`on_boot`, `on_file_change <dir>`,
  `every Nm`) now generate `launch_daemon` jobs via `install_routine_job()`.
- `set_routine_enabled()` automatically installs/removes the job when
  enabling/disabling a routine with an automatic schedule.
- New CLI commands: `pluto routine install <name>` and
  `pluto routine uninstall <name>` for manual job management.
- For `on_file_change`, a polling script checks directory mtime every 30s.
- For periodic schedules (`every 30m`, `every 2h`), a looping script runs
  the routine at the configured interval.
- 4 new tests in `test_routine.cpp` (10 total): on_boot job generation,
  manual-fails, periodic job, enable/disable job lifecycle.

#### MCP system prompt injection
- `McpManager::system_prompt_block()` generates a `<mcp_servers>` block
  describing all connected MCP servers and their tools, injected into the
  system prompt. This tells the model what remote tools are available and
  when to prefer them.
- Per-app generated MCP tools (names starting with `app_`) are now filtered
  out during registration to avoid overwhelming the model with hundreds of
  redundant wrappers for `get_property`/`set_property`.

#### Syntax highlighting for code blocks
- Fenced code blocks in the GUI side panel now have syntax highlighting
  for C++, Python, Shell, and JavaScript.
- Token types: keyword (blue), string (red-brown), comment (muted green),
  number (teal), preprocessor (brown-orange), type (dark teal).
- Colors auto-adapt to Haiku dark/light themes via `_InitPalette()`.
- `src/gui/SyntaxHighlight.h` — header-only lexer, ~300 lines.
- `tests/test_syntax.cpp` — 14 tests covering all languages, roundtrip,
  edge cases.

#### Documentation
- `docs/mcp-tutorial.md` — "Build a Haiku MCP server in 10 minutes"
  tutorial with a compilable 200-line C++ example server.
- `docs/bquery-cookbook.md` — 15+ BQuery predicate recipes covering find
  by name/type/size/date, Pluto sessions, custom attributes, live queries,
  volume-specific queries, and performance tips.

#### HaikuPorter recipe
- `pluto-0.3.0.recipe` — HaikuPorter recipe for building and packaging
  Pluto for HaikuDepot. Installs CLI + GUI + Tracker add-on + man page +
  all documentation.

#### New provider profiles in Settings dialog
- Added OpenAI with 8 models (GPT-4.1, GPT-4.1 mini, GPT-4.1 nano,
  GPT-4o, GPT-4o mini, o3, o3 mini, o4 mini).
- Added Grok (xAI) with 5 models (Grok 3, Grok 3 Mini, Grok 3 Fast,
  Grok 3 Mini Fast, Grok 2).
- Added Google Gemini with 3 models (Gemini 2.5 Pro, 2.5 Flash, 2.0 Flash).
- Added Groq with 4 models (Llama 3.3 70B, Llama 3.1 8B, Gemma 2 9B,
  Mixtral 8x7B).
- Added DeepSeek with 2 models (DeepSeek V3, DeepSeek R1).
- Split Anthropic into two profiles: "Anthropic (API key)" for direct
  API access and "Anthropic (Claude Max/Pro)" for OAuth login.
- Updated OpenRouter presets with Opus 4.6, Sonnet 4.6, Grok 3.
- Auto-select first model when switching providers in the dropdown.

#### Project-local session storage (`.pluto/`)
- When Pluto opens inside a workspace, chat sessions are now saved to
  `.pluto/sessions/` in the project root instead of the global
  `~/config/settings/pluto/sessions/`.
- Each project gets its own isolated chat history, similar to `.git/`.
- The `.pluto/sessions/` directory is created automatically on first save.
- Without a workspace, the global session directory is used as before.
- Workspace changes at runtime (via Tracker add-on) update the session
  directory automatically.

#### Per-provider API key storage
- API keys are now saved per-provider in
  `~/config/settings/pluto/keys/<ENV_VAR_NAME>` (chmod 600).
- Switching between providers preserves each provider's key.
- `resolve_api_key()` checks: env var → per-provider file → legacy file.
- Settings dialog shows key status per provider: "✓ Key configured"
  (green) or "No API key" (red), with masked bullet placeholder.
- `resolve_provider_key()` API for checking per-provider keys without
  legacy fallback.

#### Image generation support
- `OpenAICompatProvider` detects image models (`gpt-image-*`, `dall-e-*`,
  `imagen-*`) and routes requests to `/v1/images/generations` instead
  of `/chat/completions`.
- Supports both `gpt-image-1` (`output_format: png`) and `dall-e-3`
  (`response_format: b64_json`) parameter formats.
- Generated images appear in the Detail panel with "Generating image…"
  progress text. Revised prompts are shown when returned by the API.
- Right-click image in Detail panel → "Save image…" opens a native
  `BFilePanel` save dialog. PNG export via `BTranslatorRoster`.

#### Model picker dialog
- Clicking the model name in the status bar now opens a searchable
  model picker dialog instead of a popup menu.
- Background fetch of available models from all configured providers.
- Live search filter — type to narrow the list (e.g. "claude", "llama").
- Model count shown as "N / total models".
- Double-click to select and switch model.
- Handles large model catalogs (e.g. OpenRouter's 300+ models).

#### User-defined provider profiles
- Users can add custom providers via `.conf` files in
  `~/config/settings/pluto/providers/`.
- Format: `label`, `base_url`, `api_key_env`, and `model` lines.
- Custom providers appear in Preferences after the built-in providers.
- Example files in `examples/providers/` (TEMPLATE.conf, fireworks.conf,
  together.conf, perplexity.conf).

#### Color scheme override
- New "Color scheme" picker in Preferences: Auto / Dark / Light.
- Dark mode: forced dark background (30,30,30) + light text (220,220,220).
- Light mode: forced white background + dark text.
- Auto: follows system theme as before (via `ui_color()`).
- Persisted in config as `color_scheme = dark|light|auto`.
- Applied to chat output, input, side panel, and status bar.
- Works independently of the system theme, like Terminal's color override.

#### Settings dialog improvements
- Professional grouped layout with BBox sections: Provider, Model,
  Appearance, Advanced — matching Haiku preference panel style.
- Fixed-width layout (560px) prevents the dialog from resizing when
  switching providers.
- Removed separate "Model ID" text field — only the preset menu remains.
- Single "API key" field with masked input and per-provider status.
- OAuth login button shown for "Anthropic (Claude Max/Pro)" profile.
- "Fetch Models" button passes auth headers for remote providers
  (Gemini, Groq, etc.) and strips `models/` prefix from Gemini IDs.
- Model presets now include `max_tokens` and `context_window` values per
  model. Selecting a preset auto-fills both fields.
- System prompt and cache toggle moved to a collapsible "Advanced" section
  with a tooltip explaining the role of `pluto.md` files.

#### Image input / Vision
- `@image.png` mentions in the REPL now create `IMAGE` content blocks with
  base64-encoded data. Supported formats: PNG, JPEG, GIF, WebP (20 MB limit).
- `build_user_content()` splits user input into TEXT and IMAGE blocks,
  replacing the old `expand_file_mentions()`.
- Anthropic provider serializes IMAGE blocks as `{"type": "image", "source":
  {"type": "base64", ...}}`.
- OpenAI-compat provider serializes IMAGE blocks as `{"type": "image_url",
  "image_url": {"url": "data:...;base64,..."}}` using multi-part content.
- Session save/load preserves IMAGE blocks via JSONL.
- `src/base64.h` — header-only base64 encoder.
- `tests/test_image.cpp` — 11 tests covering base64 encoding (RFC 4648
  vectors), ContentBlock IMAGE type, and JSON serialization roundtrip.

#### GUI: Image input, Schedules, Tracker add-on
- Drag & drop of image files (PNG, JPEG, GIF, WebP) onto Pluto-gui creates
  IMAGE content blocks with base64 encoding. A chip bar below the input
  shows attached filenames. Both `ChatInput` and `ChatOutput` intercept
  `B_SIMPLE_DATA` from Tracker and forward it as `B_REFS_RECEIVED` to the
  window so images are handled correctly instead of being pasted as text.
- `@image.png` mentions typed in the GUI input field are also expanded to
  IMAGE blocks via the shared `build_user_content()` in `src/input.h`.
- File > Schedules… opens a Schedule Manager dialog for viewing, enabling,
  disabling, running, and removing scheduled tasks. The **Add…** button
  opens a full creation form (`AddScheduleDialog`) with fields for name,
  prompt file, event trigger, interval, and model. A **Browse…** button
  opens a native Haiku `BFilePanel` to select the prompt `.md` file from
  Tracker instead of typing the path manually.
- Help > Install/Remove Tracker add-on manages the Tracker context menu
  add-on from within Pluto-gui.
- `src/gui/ScheduleDialog.{h,cpp}` — schedule list + creation form.
- `src/input.h` — shared image detection, base64 encoding, and
  `build_user_content()` used by both CLI and GUI.
- `Pluto-gui` now links `-ltracker` (required by `BFilePanel`).

#### GUI Routines panel
- New File > Routines… opens a RoutineDialog with:
  - List of all routines with active/paused status
  - Detail panel showing schedule, model, tools, and notify settings
  - Run now — opens Terminal, pipes output to log file
  - Enable/Disable toggle
  - Show log — displays last 100 lines of the routine's execution log
    in a monospace text view
  - BNotification fired for routines with `notify: true`
- `src/gui/RoutineDialog.{h,cpp}` — new dialog.

#### Local telemetry (`pluto stats`)
- Opt-in local telemetry via `touch ~/config/settings/pluto/telemetry`.
- Token usage (in/out/cache) recorded per-turn to a CSV file.
- `pluto stats` shows aggregated table per model: tokens, turns, sessions,
  estimated cost in USD.
- `pluto stats reset` clears all data.
- Integrated in both CLI and GUI (on_usage callback).
- `src/telemetry.{h,cpp}` — no network, strictly local.

#### Tracker workspace integration
- When a file is dropped from Tracker onto Pluto-gui and it is outside the
  current workspace, a BAlert offers to trust the file's parent directory
  (No / This session / Always). Makes it easy to start working on a new
  project by just dropping a file from it.

#### Routine engine (`pluto routine`)
- New `src/routine.{h,cpp}` — recurring workflow engine with YAML frontmatter.
- Routine files (`.md`) in `~/config/settings/pluto/routines/` with fields:
  `name`, `schedule`, `model`, `tools`, `notify`, `enabled`.
- Template variables: `{{date}}`, `{{clipboard}}`, `{{changed_files}}`.
- CLI sub-commands: `list`, `run <name>`, `enable/disable <name>`, `log <name>`.
- Execution via `pluto agent run` with model override.
- Run logs saved to `~/config/settings/pluto/routine-logs/<name>.log`.
- `tests/test_routine.cpp` — 6 tests: parse with/without frontmatter,
  template expansion, enable/disable, logging, listing.

#### GUI slash commands and hooks
- GUI input field now supports slash commands: `/tools` (list all registered
  tools), `/model [ID]` (view or switch model), `/cost` (token summary),
  `/help` (command list), `/commands` (list custom commands).
- Custom slash commands from `~/config/settings/pluto/commands/<name>.md`
  are loaded at startup and available in the GUI with `{{args}}` substitution.
- `hook_user_prompt` hooks are now executed before each turn in the GUI,
  matching CLI behavior. `PLUTO_PROMPT` env var is set.

#### Session browser
- New View > Session browser… (Cmd+B) opens a filterable session list dialog.
- Shows date, title, model, and token count for each saved session.
- Live search field filters by title, model, and tags.
- Detail panel shows full metadata (created/updated, turns, tags).
- Double-click or Load button loads the session into the current conversation.
- Delete button with confirmation removes sessions.
- `src/gui/SessionBrowser.{h,cpp}` — new dialog.

#### Code quality
- `.clang-format` configuration file for consistent formatting (tab
  indentation, K&R braces, 95 column limit, namespaces not indented).
  Ready for incremental use on modified files.

#### Visible network retry
- New `StreamEvent::RETRY_INFO` event type emitted by both providers
  before each retry (connection errors, HTTP 429, 5xx).
- New `UiCallbacks::on_status` callback propagates retry info to the UI.
- CLI: retry messages printed to stderr as `[pluto] HTTP 429, retrying…`.
- GUI: retry messages shown in the status bar and as dim `[retry]` lines
  in the chat output, so the user always knows what's happening.

#### Tracker add-on "Ask Pluto"
- `src/haiku/TrackerAddOn.cpp` — Tracker context menu add-on (.so).
  Right-click files in Tracker → "Ask Pluto" opens a Terminal with Pluto
  analyzing the selected files (via `@path` mentions for content inlining).
- Build: `make Ask\ Pluto`; install: `make install-addon`.
- `tests/test_tracker_addon.cpp` — verifies the .so loads and exports
  the required `process_refs` symbol.

#### Scheduled tasks (`pluto schedule`)
- New `src/schedule.{h,cpp}` — manage recurring agent tasks via Haiku's
  `launch_daemon`.
- Sub-commands: `list`, `add`, `remove`, `enable`, `disable`, `run`.
- Schedule configs stored as key=value in
  `~/config/settings/pluto/schedules/<name>.conf`.
- For periodic schedules (`--interval N`), generates a looping shell script
  and a `launch_daemon` job in `~/config/settings/launch/`.
- For event-based schedules (`--event boot|login`), generates a direct
  `launch_daemon` job.
- `tests/test_schedule.cpp` — 5 tests: save/load roundtrip, list, remove,
  launch_daemon job generation, not-found handling.

#### Native Haiku tools (Fase 4)
- `read_attr` / `write_attr` / `list_attrs` / `remove_attr` — BFS extended
  attributes exposed as first-class tools (string, int32, int64, float,
  double, bool).
- `query_fs` — wrapper over `BQuery` with full predicate syntax (operators,
  glob wildcards, optional volume, configurable limit).
- `live_query` — runs a `BQuery` in live mode for a configurable window
  (default 5s, max 60s) and returns initial matches plus add/remove events
  via a `BLooper` receiving `B_QUERY_UPDATE`.
- Both binaries now link `-lbe` globally.

#### Caching, retry, cancel (Fase 5 + Fase 7)
- `cache_control: ephemeral` on system prompt + last tool definition.
- Parsing of `cache_read_input_tokens` and `cache_creation_input_tokens`,
  surfaced via `UiCallbacks::on_usage(in, out, cache_read, cache_create)`.
- Retry with exponential backoff (1s/2s/4s, max 3 attempts) on HTTP 429,
  5xx, and pre-stream connection errors.
- Automatic context-window truncation in `Loop::run_turn` (drops oldest
  user/assistant pairs when estimated tokens > 80% of `context_window`).
- `--no-cache` CLI flag, `cache=false` config key.
- Cooperative cancel via `src/cancel.{h,cpp}`. SIGINT in CLI aborts the
  in-flight stream without exiting the REPL; `sigaction SA_RESTART` keeps
  `fgets` working at the prompt.

#### Persistent sessions (Fase 6)
- New `src/storage/SessionStore.{h,cpp}` — JSONL session files with BFS
  extended attributes (`pluto:title`, `pluto:model`, `pluto:tags`,
  `pluto:tokens_in/out`, `pluto:created`, `pluto:updated`, `pluto:turns`).
- `fs_create_index` is called automatically on first save so sessions
  become searchable from Tracker's Find dialog.
- REPL commands: `/save`, `/load <query|idx>`, `/list`, `/resume`, `/delete`,
  `/title`, `/tag`, `/untag`.
- Auto-titling from the first user message (60 char trim).

#### Native GUI (Fase 9)
- `BApplication` + `BWindow` with `BLayoutBuilder`, worker thread per turn
  via `spawn_thread`, UI updates via `BMessenger`+`BMessage`.
- `Pluto-gui` binary, separate from `pluto` CLI, sharing the same core.
- First-run: opens Preferences automatically when no credentials are
  found, so users can configure any provider (Ollama, Claude, OpenAI, etc.).
- Menu bar: File / View / Session / MCP / Help.
- `BSplitView` chat | side panel (collapsible). Side panel shows the
  full untruncated tool result after every call.
- Custom `ChatOutput`/`ChatInput` BTextView subclasses, multiline input
  (Enter sends, Shift+Enter newline), monospace by default.
- Header markers per role (`You`, `Pluto`, `◆ tool`) in bold.
- ASCII-art "Pluto" banner.
- Collapsible tool blocks (click `▾`/`▸` to toggle, offset bookkeeping).
- Animated streaming cursor `█` via `BMessageRunner` (500 ms blink).
- Custom `TokenGauge` BView (green→yellow→orange→red gradient).
- Status bar with state, model, token totals, context %, cache hits.
- MCP integration in GUI: bootstrap with project-scope `BAlert` approval
  (Deny/Once/Always), MCP menu lists connected servers and tool counts.
- Tool approval per call via `BAlert` (Deny/Allow/Always), worker-to-UI
  sync via `sem_id`.
- Drag & drop file → path appended to input.
- `SettingsDialog` for model, max_tokens, context_window, system prompt,
  cache toggle. Persisted via `pluto::save_config`.
- Stop button replaces Send during a turn (calls `pluto::request_cancel`).
- Font zoom Cmd `=`/`-`/`0`, preserves per-run colors via `B_FONT_SIZE`.
- Session menu: Save (Cmd+S), Resume last (Cmd+R), Recent sessions submenu
  (top 15). Loaded conversations re-rendered into output.
- `BNotification` on turn complete when window is not active, click brings
  the window to front via `SetOnClickApp`.

#### Quality (Fase 7)
- `make check` runs `tests/test_json.cpp` (assert-style, no framework).
- `README.md`, `docs/architecture.md` with module map, threading diagram,
  and BQuery cookbook.
- Header dependency tracking via `-MMD -MP` in CXXFLAGS — prevents stale
  `.o` files when headers change (which previously caused an ABI mismatch
  crash in `~MainWindow`).

### Removed
- **`ApiKeyDialog`** — the modal first-run authentication dialog has been
  removed. Credentials are now configured via the Preferences dialog,
  which supports all providers (not just Claude).

### Fixed
- **OpenAI-compat error messages** — error bodies wrapped in JSON arrays
  (Gemini `[{...}]` format) are now unwrapped correctly, and `raw_body`
  is preferred over SSE residual for error reporting. Errors like
  HTTP 429 now show the actual server message instead of `]`.
- **Gemini `models/` prefix** — model IDs from Gemini's `/v1/models`
  endpoint (e.g. `models/gemini-2.5-pro`) are stripped to just the model
  name in both the quick-switch picker and Fetch Models.
- **Banner truncation** — long model/provider names in the welcome banner
  are truncated with `…` to prevent overflowing into the ASCII art.
- **Provider rebuild on settings change** — the provider is now always
  recreated when Preferences are saved, fixing the bug where switching
  between two openai-compat providers (e.g. Gemini → OpenRouter) would
  silently keep the old connection.
- **Model picker OAuth** — `oauth_ensure_valid_token()` is now called
  before fetching the Anthropic model list, fixing empty results when
  the OAuth token had expired.
- **Ask Pluto Tracker add-on workspace** — workspace now correctly set to the
  selected folder (not the binary directory). Works when no files are selected
  (click on Tracker window background). Message routing fixed through
  `App::RefsReceived` to `MainWindow`. Single directory selection sets
  workspace to that directory. Launch via `--workspace=` argument.
- **Window title trust indicators** — replaced Unicode symbols (✓/⚠) with
  ASCII `[trusted]`/`[untrusted]` for compatibility.
- **System prompt persistence bug** — `save_config()` was writing the
  composite `system_prompt` (user + memory + skills + MCP block) to disk.
  On next launch, runtime injection re-appended everything, causing infinite
  growth (22 KB+). Fixed by adding `user_system_prompt` field: only the
  user-editable base prompt is persisted; runtime-injected content
  (project memory, skills, MCP) is never saved.
- **MCP system prompt block included `app_*` tools** —
  `system_prompt_block()` now filters per-app wrappers, matching the
  registry filter. System prompt dropped from ~22 KB to ~1.5 KB.
- Context gauge (ctx %) now uses session-accumulated tokens (`fTokensIn +
  fTokensOut`) instead of last-turn-only tokens, so the percentage reflects
  actual context window usage over the conversation.
- MCP servers from `.mcp.json` now appear correctly in Pluto-gui when
  launched from Tracker double-click or Deskbar. Two fixes:
  1. `main_gui.cpp` now `chdir()`s to the binary's own directory at
     startup (via `image_info`), before `workspace_init()`. This ensures
     the workspace root is the project directory, not `/boot/home`.
  2. `cwd()` in `mcp/Config.cpp` prefers `workspace_root()` over
     `getcwd()` as fallback.

#### Multi-instance GUI
- `pluto.rdef` changed from `B_SINGLE_LAUNCH` to `B_MULTIPLE_LAUNCH`.
  Multiple GUI instances can now be opened by double-clicking the icon
  or via the "Ask Pluto" Tracker add-on while Pluto is already running.
- `App.cpp` — `pick_signature()` generates a unique BApplication
  signature per process (`application/x-vnd.Hay-pluto-<pid>` for the
  second instance onward) so each can coexist with the app_server.

#### Multi-instance safety (Fase A)
- **`src/filelock.h`** — new RAII `ScopedFileLock` using `flock(2)` with
  timeout and 10ms retry polling. Header-only, no external dependencies.
- `save_config()` and `save_api_key()` in `config.cpp` now use atomic
  write (tmp with PID + rename) protected by `ScopedFileLock`.
- `save_oauth_tokens()` in `oauth.cpp` — PID in temp filename + file lock.
- `trust_workspace()` / `untrust_workspace()` in `workspace.cpp` — wrapped
  in `ScopedFileLock`; trust re-checks under lock to prevent duplicates;
  untrust read-filter-rewrite runs atomically (TOCTOU fix).
- `telemetry_record()` / `telemetry_session_end()` in `telemetry.cpp` —
  file lock with 500ms timeout on CSV append.
- `log_access()` in `workspace.cpp` — file lock with 500ms timeout.
- `SessionStore::save()` — random 4-hex suffix in filename to prevent
  collision between concurrent instances creating sessions simultaneously.
- GUI approval semaphore in `MainWindow.cpp` — name includes PID
  (`pluto-approval-<pid>`) so each GUI instance has its own semaphore.

#### BeAPI RAG enhancements (Fase K3)
- **BFS export** — `pluto beapi export` writes each snippet as a file in
  `~/config/settings/pluto/beapi/` with `beapi:topic` and `beapi:classes`
  BFS attributes. Searchable via Tracker Find or `query_fs`.
- **GUI side panel** — when RAG snippets are auto-injected, the side panel
  shows "BeAPI reference" with the list of injected topics, and a dim
  `[beapi] injected: ...` line appears in the chat. New `on_rag_inject`
  callback in `UiCallbacks`.
- **Custom snippets** — `rag_load_custom()` loads user-contributed snippets
  from `~/config/settings/pluto/beapi-custom.jsonl`. Same JSONL format as
  the main index. Classes auto-extracted if omitted. Merged at startup.
- **`pluto beapi stats`** — shows number of loaded entries.

#### BeAPI RAG loop integration (Fase K2)
- **`src/tools/BeapiLookup.{h,cpp}`** — new `beapi_lookup` tool the model
  can call explicitly. Parameters: `query` (string), `top_k` (int, 1-5).
  Returns top-k code snippets with topic, classes, and complete code.
- **Auto-inject in `Loop::run_turn`** — when the last user message mentions
  Be class names (detected by `rag_extract_classes`), the top-3 RAG results
  are injected into the system prompt as a `<beapi_reference>` block.
- **`apply_rag_hint()`** in `config.cpp` — adds a `<beapi_hint>` block to
  the system prompt informing the model about `beapi_lookup`.
- **`pluto beapi <query>`** CLI subcommand — standalone search of the BeAPI
  reference. Shows top-5 results with scores, classes, and full snippets.
  Index auto-discovered from `./data/`, `~/config/settings/pluto/`, or
  `/boot/system/data/pluto/`.

#### BeAPI RAG knowledge base (Fase K1)
- **`data/beapi-index.jsonl`** — 390 deduplicated BeAPI code snippets
  (from `dataset_v3.jsonl`) covering 202 Be classes. ~500 KB, shipped
  with the package.
- **`src/rag.{h,cpp}`** — local retrieval engine. `rag_init()` loads
  index into memory (~200 KB). `rag_search(query, top_k)` scores entries
  with class match (5x IDF) + topic keyword (3x) + question keyword (1x).
  `rag_extract_classes()` detects Be class names in text. ~1ms per query,
  zero external dependencies.
- `tests/test_rag.cpp` — 10 tests: init, search by class/keyword/multiple,
  ranking, top_k, no match, class extraction.

### Changed
- `OpenAICompatProvider` stub removed. Only `anthropic` provider is
  supported until a real second backend is implemented.
- MCP bootstrap centralized in new `pluto::McpManager` class
  (`src/mcp/Manager.{h,cpp}`), removing the duplicated loop in `main.cpp`
  and `src/gui/App.cpp`.
- **GUI refactor R2d+R2b — conversation state and worker thread moved to ConversationView.**
  MainWindow no longer owns conversation state (messages, tool blocks,
  streaming markdown, tokens, session metadata, cursor, shimmer). All
  per-conversation fields now live in `ConversationView`, accessed via
  public getters and `Handle*()` methods. MainWindow's `kMsgWorker*`
  handlers delegate rendering to `fConv->Handle*()` and only manage
  chrome (side panel, status bar, title, notifications). ~600 lines of
  duplicate rendering methods removed from MainWindow.cpp. R2b: worker
  thread (`WorkerCtx`, `_WorkerEntry`, `_StartTurn`) moved to
  ConversationView. ConversationView overrides `MessageReceived` to handle
  all `kMsgWorker*`, `kMsgApproval`, and `kMsgCursorTick` messages.
  MainWindow delegates `_SendCurrentInput()` to `fConv->SendInput()`.
  Tool approval uses `ConversationDelegate::RequestToolApproval` (returns
  0/1/2 for deny/allow/always). Cursor `BMessageRunner` created in
  `ConversationView::AttachedToWindow`. Shared message constants moved to
  `ChatColors.h`. MainWindow.cpp reduced from 4657 to 3735 LOC (-922).
  R3: MainWindow now uses `BTabView` as container. `_ActiveConv()` helper
  returns the selected tab's ConversationView.
  R4: Multi-tab management. `PlutoTabView` subclass notifies MainWindow on
  tab selection via `kMsgTabChanged`. File > New tab (Cmd+T) creates a new
  ConversationView tab (max 10). File > Close tab (Cmd+W) with confirmation
  if conversation has content. Tab labels auto-update from session title
  (truncated to 30 chars). Cmd+Right/Left to navigate between tabs.
  `_OnTabChanged()` refreshes status bar, side panel, title bar, and input.
  R5: Multi-tab test suite (`tests/test_tabs.cpp`, 17 test cases) verifying
  state isolation between tabs: independent messages, tokens, session
  metadata, busy flags, tool tracking, delegate routing, reset, font zoom,
  cost estimates, and markdown export. Documentation updated: architecture.md
  with per-tab threading model, user-guide.md with tab management section
  and keyboard shortcuts.

### Fixed
- Crash in `~MainWindow` due to ABI mismatch between stale `App.o` and
  fresh `MainWindow.o` after editing `MainWindow.h`. Root cause: the old
  Makefile had no header-dependency tracking. Fixed via `-MMD -MP`.
- `query_fs` raw-string literal terminated prematurely on `)"` inside
  description — switched to custom delimiter `R"JSON(...)JSON"`.

---

## 0.2.0 — Initial sketch

- `provider::Provider` abstraction, `AnthropicProvider` with SSE streaming.
- `agent::Loop` with tool_use cycle.
- 6 built-in tools (`read_file`, `write_file`, `edit_file`, `bash`, `grep`,
  `glob`).
- MCP client with HTTP, SSE, stdio transports; project / user / local scopes.
- Interactive REPL + `--one-shot` mode.
- Sub-command `pluto mcp` for server management.
