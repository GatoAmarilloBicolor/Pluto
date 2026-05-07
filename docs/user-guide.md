# pluto — User Guide

> The native Claude client for Haiku OS.
> A production of the **Movement for the Liberation of Walter OS**.

---

## Table of Contents

1. [What is pluto](#what-is-pluto)
2. [Installation](#installation)
3. [First Run](#first-run)
4. [Interactive CLI](#interactive-cli)
5. [Native GUI](#native-gui)
6. [Configuration](#configuration)
7. [Providers and Models](#providers-and-models)
8. [Workspace and Permissions](#workspace-and-permissions)
9. [Built-in Tools](#built-in-tools)
10. [Haiku-Native Tools](#haiku-native-tools)
11. [MCP Servers](#mcp-servers)
12. [Sessions and Memory](#sessions-and-memory)
13. [Skills and Custom Commands](#skills-and-custom-commands)
14. [Hook System](#hook-system)
15. [Non-Interactive Execution](#non-interactive-execution)
16. [Routines](#routines)
17. [Scheduled Tasks](#scheduled-tasks)
17. [Tracker Add-on](#tracker-add-on)
18. [Keyboard Shortcuts (GUI)](#keyboard-shortcuts-gui)
19. [Cost Tracking](#cost-tracking)
20. [Troubleshooting](#troubleshooting)
21. [For Developers](#for-developers)

---

<a id="what-is-pluto"></a>

## What is pluto

pluto is a client for the Anthropic Claude API written entirely in C++17 for
Haiku OS. No Electron, no Node, no Python — just libcurl, OpenSSL, and the
Be API.

It ships as two binaries compiled from the same source:

- **`Pluto_cli`** — command-line interface with an interactive REPL
- **`Pluto`** — native GUI with `BWindow`, split view, collapsible tool
  blocks, animated streaming cursor, token gauge, and much more

pluto can use Claude (Anthropic) directly, or any OpenAI-compatible provider
(Ollama, OpenRouter, LM Studio) for local or alternative models.

### What makes it special

- **16 integrated tools**, including web fetch/search, plus 8 Haiku-specific
  tools (BFS attributes, BQuery, live query, node monitor, clipboard,
  desktop scripting)
- **Image input (vision)** — drag&drop or `@image.png` sends images to
  Claude for visual analysis
- **Native GUI** that honours the Haiku theme via `ui_color()`, with
  `BSplitView`, `BNotification`, collapsible blocks, inline edit diffs,
  image attachment chips, and a token budget gauge
- **Tracker add-on** — right-click files in Tracker to "Ask pluto"
- **Scheduled tasks** via `launch_daemon` for automated recurring prompts
- **MCP client** with HTTP/SSE/stdio transports and auto-discovery of
  local servers
- **Session storage** as JSONL files with BFS extended attributes —
  searchable from Tracker's Find dialog
- **Workspace trust** system with audit logging, like Claude Code's project
  trust but using `BNode` attributes
- **Prompt caching** with ephemeral `cache_control`, retry with exponential
  backoff, and cancel-on-Ctrl+C
- **Custom skills, slash commands, and hooks** for personalised workflows
- **OpenAI-compatible provider** for Ollama, OpenRouter, LM Studio, or
  any OpenAI-API-compatible server

---

<a id="installation"></a>

## Installation

### Requirements

- Haiku R1/beta5 or later
- `gcc` (included in Haiku base)
- `libcurl` and `libbe` (included in Haiku base)

### Building from source

```sh
cd /boot/home/Desktop/pluto
make              # builds Pluto_cli (CLI)
make Pluto        # builds Pluto (GUI)
make 'Ask pluto'  # builds Tracker add-on (.so)
make check        # runs the 20 self-contained test suites
make ci           # clean build + full test suite
make install-addon   # installs Tracker add-on
make uninstall-addon # removes Tracker add-on
make clean        # removes objects and binaries
```

### Packaging

```sh
make package      # creates pluto-0.3.0-1-x86_64.hpkg
```

The `.PackageInfo` declares all dependencies (`lib:libcurl`, `lib:libbe`,
`lib:libcrypto`, `lib:libssl`).

---

<a id="first-run"></a>

## First Run

### Setting up the API key

**Option A — environment variable** (recommended):

```sh
export ANTHROPIC_API_KEY=sk-ant-…
./pluto
```

**Option B — persistent file**:

The key is stored in `~/config/settings/pluto-api-key` (single line,
chmod 600). Both `pluto` and `pluto-gui` read it automatically.

**Option C — Claude Max/Pro account** (OAuth):

```sh
./pluto login
```

This opens your browser for Claude account authentication. After logging
in, pluto saves OAuth tokens locally and uses them automatically. No API
key needed. Use `pluto logout` to revert to API key auth.

**Option D — GUI dialog**:

If no key is found, `pluto-gui` pops a modal `ApiKeyDialog` that saves the
key to the persistent file for you.

### Workspace trust

On first launch in a directory, pluto asks:

```
⚠ First time in /boot/home/myproject
  Authorize pluto to work here? [y]once [a]always [n]o:
```

- **y** — session trust (valid until pluto exits)
- **a** — permanent trust (saved to `~/config/settings/pluto/trusted_workspaces`)
- **n** — tools that access files will error

In the GUI, this appears as a `BAlert` with Deny / This session / Always
trust buttons.

**Tracker integration**: when you drag and drop a file from Tracker onto
pluto-gui and that file is outside the current workspace, pluto automatically
offers to trust the file's parent directory via a `BAlert`. This makes it
easy to start working on a new project — just drop a file from it.

Path canonicalization resolves symlinks and normalizes `..` components
to prevent directory traversal attacks — a tool cannot escape the workspace
by requesting a path like `workspace/../../../../etc/passwd`.

Use `--yolo` to bypass all permission checks (dangerous).

---

<a id="interactive-cli"></a>

## Interactive CLI

Start with:

```sh
./pluto
```

### REPL commands

| Command | Description |
|---|---|
| `/help` | List all commands |
| `/quit`, `/exit` | Exit pluto |
| `/reset` | Clear conversation history |
| `/init [--force]` | Create `pluto.md` project memory in workspace (auto-detects language, build system, git remote) |
| `/model [ID]` | Show or switch model |
| `/cost` | Show session token totals (turns, input, output) |
| `/tools` | List all registered tools (built-in + MCP) |

### Session management

| Command | Description |
|---|---|
| `/save [title]` | Save current session to BFS store |
| `/load <query\|index>` | Load a session by title substring or numeric index from `/list` |
| `/list` | Show saved sessions (up to 20, sorted by recency) |
| `/resume` | Load the most recently updated session |
| `/delete` | Delete the currently loaded session file |
| `/title [new]` | Show or set the session title |
| `/tag [name]` | Show tags, or add one to current session |
| `/untag <name>` | Remove a tag |

Sessions are saved as JSONL files in `~/config/settings/pluto/sessions/`
with metadata stored as BFS extended attributes (`pluto:title`,
`pluto:model`, `pluto:tokens_in`, `pluto:tokens_out`, `pluto:created`,
`pluto:updated`, `pluto:turns`, `pluto:tags`).

BFS indexes are automatically created on `pluto:title`, `pluto:model`,
`pluto:tags`, `pluto:updated`, and `pluto:created`, so sessions appear in
Tracker's Find dialog.

### SSE subscription

| Command | Description |
|---|---|
| `/watch [URL]` | Subscribe to an SSE stream (default: `http://localhost:2607/sse`) |
| `/unwatch` | Stop the subscription |
| `/events` | Show pending events without sending a turn |

Accumulated events are injected into the next user turn as a
`<system-reminder>` block so Claude sees what happened.

### Multi-instance IPC

| Command | Description |
|---|---|
| `/peers` | List running Pluto GUI instances with PID, workspace, model, state |
| `/ask <peer> <text>` | Send a prompt to a Pluto GUI peer (by PID or workspace) |
| `/delegate <peer> <text>` | Fire-and-forget prompt to a peer (notifies when done) |

The CLI can discover running Pluto GUI instances via `BRoster` and query
their state (workspace, model, session title, busy status) through the BeOS
scripting protocol. GUI instances expose a scripting suite that can also be
tested from Terminal:

```sh
hey Pluto get Workspace
hey Pluto get Model
hey Pluto get Busy
```

**Cross-instance prompts**: use `/ask` to inject a prompt into another
Pluto GUI instance's conversation. The peer processes it as if the user
typed it. Identify the peer by its PID (from `/peers`) or by a workspace
directory substring:

```
/ask 463 summarize the current session
/ask pluto /ask myproject explain the auth flow
```

`/delegate` works the same way but is fire-and-forget: the peer processes
the prompt in the background and sends a `BNotification` when the turn
completes.

**Workspace collision detection**: when a new Pluto instance starts in a
directory already used by another peer, a warning is displayed:

```
⚠ Another Pluto instance (PID 463) is already working in /boot/home/myproject
  model: claude-sonnet-4-6, idle
```

### Line editing and history

The REPL uses **libedit** (BSD editline) for line editing with:

- Arrow keys up/down to navigate command history
- Emacs keybindings: Ctrl-A (start), Ctrl-E (end), Ctrl-K (kill), Ctrl-W (word)
- Home/End keys
- Persistent history saved to `~/config/settings/pluto/history` (500 entries)

### Routines

| Command | Description |
|---|---|
| `/routines` | List available routines with schedule and status |
| `/routine <name>` | Expand a routine's template and run it in the current chat |

These commands are available in both CLI and GUI. For full routine
management (enable, disable, install triggers), use the `pluto routine`
subcommand or the GUI Routines dialog.

### Custom commands

| Command | Description |
|---|---|
| `/commands` | List user-defined custom slash commands |
| `/<name> [args]` | Run a custom command |

Custom commands live in `~/config/settings/pluto/commands/<name>.md`.
Arguments are substituted for `{{args}}` in the template body.

### File and image mentions

Type `@path/to/file` in your message and it will be expanded inline:

```
> Explain @src/config.cpp
```

This inserts the file contents wrapped in `[file: src/config.cpp]...[/file]`
markers.

**Image files** (`.png`, `.jpg`, `.jpeg`, `.gif`, `.webp`) are treated
specially: instead of inlining text, pluto reads the file, base64-encodes it,
and sends it as an `IMAGE` content block so Claude can see and analyze the
image (vision). Maximum file size: 20 MB.

```
> What's in this screenshot? @screenshot.png
> Compare these two mockups @old.png @new.png
```

In the GUI, drag and drop image files from Tracker onto the pluto-gui window
(you can drop onto any part of the window). Images appear as chips below the
input area showing the filename (e.g. `[IMG] photo.png`). Type your message
and press Send to include them. You can attach multiple images at once.

### Cancel a streaming turn

Press **Ctrl+C** during streaming to abort the in-flight request. The REPL
continues running — only the current turn is cancelled.

---

<a id="native-gui"></a>

## Native GUI

Launch with:

```sh
./Pluto
```

You can also double-click `Pluto` in Tracker. The app automatically
detects its own directory and uses it as the workspace root, so MCP servers
configured in `.mcp.json`, project memory (`pluto.md`), and workspace trust
all work correctly regardless of how the app was launched.

### Window layout

```
┌─ Menu bar: File │ View │ Session │ MCP │ Help ──────────┐
│  [Find bar — hidden by default, toggled with Cmd+F]     │
│  [ Tab 1: New conversation | Tab 2: My project | + ]    │
├─────────────────────┬───────────────────────────────────-┤
│                     │                                    │
│   Chat output       │   Side panel (detail)              │
│   (scrollable)      │   - Full untruncated tool output   │
│                     │   - Inline edit diffs (red/green)  │
│   ▾ ◆ read_file    │                                    │
│     (collapsible)   │                                    │
│                     │                                    │
├─────────────────────┴───────────────────────────────────-┤
│  [ Input area (multiline)                    ] [ Send ]  │
│  Ready · claude-opus-4-5 · in 1234 / out 567 · ctx 12%  │
│  ▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░  $0.04                     │
└──────────────────────────────────────────────────────────┘
```

### Tabs (multi-conversation)

Pluto supports multiple simultaneous conversations in tabs. Each tab is
fully independent: its own message history, worker thread, token
accounting, session metadata, and tool approval state.

- **New tab**: File > New tab (Cmd+T). Opens a fresh conversation.
  Maximum 10 tabs.
- **Close tab**: File > Close tab (Cmd+W). Asks for confirmation if the
  tab has content. If the tab has a turn in progress, you must cancel it
  first. Closing the last tab resets it instead of removing it.
- **Switch tabs**: Cmd+Right / Cmd+Left to cycle between tabs, or click
  on a tab.
- **Tab labels**: automatically set from the session title (first user
  message, truncated to 30 characters). New tabs show "New conversation".
- **Independent state**: each tab can be thinking, running tools, or idle
  independently. Saving a session (Cmd+S) saves the active tab's
  conversation. Loading a session opens it in the active tab.

### Menu structure

**File**
- New tab (Cmd+T)
- Close tab (Cmd+W)
- New conversation (Cmd+N)
- Preferences (Cmd+,)
- Schedules… (manage scheduled tasks)
- Routines… (manage recurring workflows)
- Export as Markdown
- Export as JSONL
- Quit (Cmd+Q)

**View**
- Session browser (Cmd+B)
- Find (Cmd+F)
- Find Next (Cmd+G)
- Zoom In (Cmd+=)
- Zoom Out (Cmd+-)
- Reset Zoom (Cmd+0)

**Session**
- Save session (Cmd+S)
- Resume last (Cmd+R)
- Recent sessions (dynamic submenu with 15 most recent)
- Reset history

**MCP**
- List of connected servers (click for tool details)
- Discover servers (auto-detect on local ports)
- Peer instances (show running Pluto instances with PID, workspace, model, state)

**Help**
- User guide
- Install/Remove Deskbar icon
- Install/Remove Tracker add-on
- About pluto

### Interactive features

**Collapsible tool blocks**: each tool call shows a header with `▾`/`▸`
arrows. Click the arrow to collapse or expand the output.

**Streaming cursor**: a blinking `█` character at the stream head, animated
via `BMessageRunner` at 500ms intervals.

**Token gauge**: a horizontal bar below the status line that shifts from
green to yellow to orange to red as context usage approaches the limit.

**Cost estimate**: `$X.XX` displayed in the status bar, calculated from
model pricing tables per token family.

**Model quick-switch**: click anywhere on the status bar to open a popup
listing all available models. Claude models are fetched live from the
Anthropic API; Ollama models from your configured server. Both are shown
together with section headers. Selecting a model from a different provider
switches automatically.

**Syntax highlighting**: fenced code blocks with a language tag (e.g.
\`\`\`cpp, \`\`\`python, \`\`\`sh, \`\`\`js) are syntax-highlighted in
the side panel with coloured keywords, strings, comments, numbers,
preprocessor directives, and type names. Colors adapt to your Haiku theme.

**Split view**: drag the grip between chat and side panel. Double-click the
grip to collapse the side panel entirely.

**Inline image preview**: when a tool result is a path to an image file
(PNG, JPEG, GIF, BMP, WebP), the side panel shows a scaled preview of the
image below the text output, decoded via Haiku's `BTranslatorRoster`.

**Inline edit diffs**: when `edit_file` runs, the side panel shows a
coloured unified diff (red for removed lines, green for added).

**Tool approval**: each tool call triggers a `BAlert` with
Deny / Allow This Time / Always Allow. The "Always" choice is remembered
for the session.

**Drag and drop**: drag files from Tracker and drop them anywhere on the
pluto-gui window — on the chat area, the input field, or the side panel.
Text files are added as `@path` mentions in the input; image files
(PNG, JPEG, GIF, WebP) are base64-encoded and queued as vision attachments.
Individual chip buttons appear between the input and the chat area, each
showing 🖼 + filename + × (e.g. `🖼 photo.png ×  🖼 diagram.jpg ×`).
Click × on a chip to remove that image. You can drop multiple images,
then type your message and press Send. Chips clear automatically after
sending.

**Image input (vision)**: you can also type `@image.png` directly in the
input field. Images are sent as content blocks so Claude can see and analyze
them visually. This works in both the CLI and the GUI.

**Notifications**: when a turn completes while the window is not focused, a
native `BNotification` fires with a preview of the response. Click it to
bring the window to the foreground.

**Theming**: colours are computed from `ui_color()` so the app matches your
Haiku theme (light and dark themes auto-detected via background luminance).
Changes are applied live when you switch themes (`B_COLORS_UPDATED`).

**Color scheme override**: in Preferences, a "Color scheme" picker lets you
force Dark or Light mode independently of the system theme (like Terminal).
Dark mode uses a dark background (30,30,30) with light text; Light mode uses
a white background with dark text; Auto follows the system theme as before.
The setting is persisted as `color_scheme = dark|light|auto` in the config
and applied to chat output, input, side panel, and status bar.

**Deskbar replicant**: install a 16x16 pluto icon in the Deskbar tray via
Help > Install Deskbar icon. Click the icon to bring pluto-gui to front
(or launch it if not running). Remove via Help > Remove Deskbar icon.

**Collapsible assistant messages**: long assistant responses (> 400 chars)
get a `▾` arrow on the "pluto" header. Click to collapse/expand, just
like tool output blocks.

**Workspace indicator**: the status bar shows `[trusted] foldername` or
`[untrusted] foldername` at the left edge, so you always know the workspace
trust state.

**Slash commands**: type `/` in the input field to use commands just like in
the CLI. Available commands: `/tools`, `/model [ID]`, `/cost`, `/init`,
`/title [new]`, `/tag [name]`, `/untag <name>`, `/routines`,
`/routine <name>`, `/commands`, `/help`. Custom slash commands from
`~/config/settings/pluto/commands/` work in the GUI too.

**User prompt hooks**: hooks configured via `hook_user_prompt=` in the config
file are executed before each turn in the GUI, same as in the CLI. The
`PLUTO_PROMPT` environment variable is set to the user's message text.

**Session browser** (Cmd+B): opens a filterable dialog showing all saved
sessions. Type in the search field to filter by title, model, or tags in
real time. Select a session to see its full metadata (creation date, turns,
token counts, tags). Double-click or press Load to open a session. You can
also delete sessions from here.

**Stop button**: during a turn, the Send button becomes Stop. Click it (or
press Esc) to cancel the in-flight request.

---

<a id="configuration"></a>

## Configuration

### Config file

Location: `~/config/settings/pluto/config` (key=value format, `#` for comments).

**Note**: on first run, if the legacy config file `~/config/settings/pluto`
exists as a flat file, pluto automatically migrates it to
`~/config/settings/pluto/config` (inside a directory) so that sessions,
skills, commands, and MCP approvals can live alongside it.

| Key | Default | Description |
|---|---|---|
| `provider` | `anthropic` | `anthropic` or `openai-compat` |
| `model` | `claude-opus-4-5` | Model identifier |
| `base_url` | *(empty)* | API base URL (required for openai-compat) |
| `api_key_env` | `ANTHROPIC_API_KEY` | Env var name for the API key |
| `system` | *(empty)* | User system prompt (base only; memory/skills/MCP are injected at runtime) |
| `max_tokens` | `4096` | Max output tokens per turn |
| `context_window` | `200000` | Model context size (for the gauge) |
| `cache` | `true` | Enable ephemeral prompt caching |
| `color_scheme` | `auto` | Color scheme override: `auto`, `dark`, or `light` |
| `hook_pre_tool` | *(none)* | Shell command to run before each tool call (repeatable) |
| `hook_post_tool` | *(none)* | Shell command to run after each tool call (repeatable) |

### API key resolution

1. OAuth tokens in `~/config/settings/pluto/oauth-tokens` (from `pluto login`)
2. Environment variable named in `api_key_env` (default `$ANTHROPIC_API_KEY`)
3. File `~/config/settings/pluto-api-key` (single line, chmod 600)
4. GUI dialog (pluto-gui only, saves to the file above)

### Project memory

- `~/config/settings/pluto/pluto.md` — user-wide persistent memory, always
  loaded as `<user_memory>`
- `./pluto.md` — project-scoped memory, loaded as `<project_memory>` when
  present in the current directory

Use `/init` to create a `pluto.md` in the current workspace. It auto-detects
the project language, build system, and git remote, then generates a template
with an `## Instructions` section for you to fill in. Use `/init --force` to
regenerate an existing file. Works in both CLI and GUI.

Both files are appended to the system prompt at runtime and are **not** saved
to the config file. Only the user-editable base prompt (the `system =` key in
the config, or the Advanced section in Preferences) is persisted.

---

<a id="providers-and-models"></a>

## Providers and Models

### Anthropic (default)

```ini
provider = anthropic
model = claude-opus-4-5
api_key_env = ANTHROPIC_API_KEY
```

Available models: `claude-opus-4-6`, `claude-opus-4-5`, `claude-sonnet-4-6`,
`claude-haiku-4-5-20251001`, or any model ID the API accepts.

Features: streaming SSE, prompt caching, tool use, retry with backoff.

### OpenAI-compatible

```ini
provider = openai-compat
base_url = http://localhost:11434
model = llama3.1
api_key_env = OPENAI_API_KEY
```

Tested with:

| Service | `base_url` | Notes |
|---|---|---|
| Ollama | `http://localhost:11434` | `api_key_env` can be empty |
| OpenRouter | `https://openrouter.ai/api` | Requires API key |
| LM Studio | `http://localhost:1234` | Local, no key needed |

The provider builds OpenAI-format requests (system as message, `tool_calls`
array, `tool` role for results), parses streaming deltas, handles `[DONE]`
sentinel, and supports retry + cancel.

### Additional built-in providers (GUI)

The Preferences dialog includes pre-configured profiles for several providers
beyond Anthropic and generic OpenAI-compat:

| Provider | Models | Notes |
|---|---|---|
| Grok (xAI) | Grok 3, 3 Mini, 3 Fast, 3 Mini Fast, 2 | `XAI_API_KEY` |
| Google Gemini | 2.5 Pro, 2.5 Flash, 2.0 Flash | `GEMINI_API_KEY` |
| Groq | Llama 3.3 70B, 3.1 8B, Gemma 2 9B, Mixtral 8x7B | `GROQ_API_KEY` |
| DeepSeek | V3, R1 | `DEEPSEEK_API_KEY` |
| OpenRouter | Opus 4.6, Sonnet 4.6, Grok 3, and more | `OPENROUTER_API_KEY` |

Select a provider from the dropdown in Preferences (Cmd+,) and the model
list updates automatically. The first model is auto-selected when switching
providers.

### User-defined provider profiles

You can add custom providers by placing `.conf` files in
`~/config/settings/pluto/providers/`. Each file defines one provider:

```ini
label = My Provider
base_url = https://api.example.com/v1
api_key_env = MY_PROVIDER_KEY
model = model-a
model = model-b
model = model-c
```

Custom providers appear in the Preferences provider dropdown after the
built-in providers. Example files are provided in `examples/providers/`
(TEMPLATE.conf, fireworks.conf, together.conf, perplexity.conf).

### Switching models at runtime

- CLI: `/model claude-sonnet-4-6`
- GUI: Session menu or Preferences dialog (Cmd+,)

---

<a id="workspace-and-permissions"></a>

## Workspace and Permissions

pluto enforces a workspace boundary to prevent accidental file access
outside your project.

### Trust levels

| Level | Scope | Duration | Trigger |
|---|---|---|---|
| Session | Current directory | Until pluto exits | `y` at prompt |
| Permanent | Named directory | Forever | `a` at prompt, or `pluto trust add` |
| Yolo | All paths | Current session | `--yolo` flag |

### CLI trust management

```sh
pluto trust list              # show all permanently trusted directories
pluto trust add /boot/home/myproject
pluto trust remove /boot/home/myproject
pluto trust log               # show last 50 access log entries
```

### Access logging

Every file access attempt (allow or deny) is logged to
`~/config/settings/pluto/access.log` with timestamp, path, action, and
result. View with `pluto trust log`.

### Enforcement

The following tools check `workspace_allows(path)` before executing:
`read_file`, `write_file`, `edit_file`, `bash` (cwd check), `read_attr`,
`write_attr`, `list_attrs`, `remove_attr`.

If a path is outside the workspace, the tool returns an error:
`"path outside workspace — use pluto trust add <dir>"`.

---

<a id="built-in-tools"></a>

## Built-in Tools

These 8 tools are always available:

| Tool | Description |
|---|---|
| `read_file` | Read a file and return its contents |
| `write_file` | Write or create a file (overwrites) |
| `edit_file` | Find-and-replace (old string must match exactly once) |
| `bash` | Execute a shell command (timeout configurable, default 120s, max 600s) |
| `grep` | Search files with regex (`grep -rnE` wrapper) |
| `glob` | Find files by pattern (`**/*.cpp` syntax) |
| `web_fetch` | Fetch a URL and return its content as text (HTML stripped automatically) |
| `web_search` | Search the web (Brave/Tavily/SearXNG) and return results |

All file tools respect the workspace boundary.

### Web tools

**`web_fetch`** downloads a URL and returns the content as text. HTML pages
are automatically stripped of tags, scripts, and styles. Use the `raw`
parameter to get unprocessed content. Output is capped at 32 KB.

**`web_search`** searches the web and returns a list of results with titles,
URLs, and snippets. It auto-detects which search engine is configured:

```sh
export BRAVE_API_KEY=...    # Brave Search (preferred)
export TAVILY_API_KEY=...   # Tavily
export SEARXNG_URL=...      # self-hosted SearXNG instance
```

Use `web_fetch` to read full pages from the URLs returned by `web_search`.

---

<a id="haiku-native-tools"></a>

## Haiku-Native Tools

These 8 tools require Haiku and use `libbe`:

### BFS extended attributes

| Tool | Description |
|---|---|
| `read_attr` | Read an attribute value from a file |
| `write_attr` | Write an attribute (types: string, int32, int64, float, double, bool) |
| `list_attrs` | List all attributes on a file (name, type code, size) |
| `remove_attr` | Delete an attribute |

Example:

```
read_attr   path=/boot/home/foo.txt name=BEOS:TYPE
write_attr  path=/boot/home/foo.txt name=pluto:tag value=urgent type=string
list_attrs  path=/boot/home/foo.txt
```

### BQuery

| Tool | Description |
|---|---|
| `query_fs` | Run a one-shot BQuery predicate against a BFS volume |
| `live_query` | Watch for file events matching a predicate for a given duration |

Query predicate syntax:

```
name == "*.cpp" && size > 1024
BEOS:TYPE == "text/x-source-code"
last_modified > %now-1day%
pluto:title == "*refactor*"
```

Operators: `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, parentheses.
Strings support `*` and `?` glob wildcards. Indexed attributes (`name`,
`size`, `last_modified`, `BEOS:TYPE`, plus any custom index) search at
near-zero cost.

Parameters: `predicate` (required), `volume` (default: boot), `limit`
(default: 100).

### Desktop integration

| Tool | Description |
|---|---|
| `clipboard` | Get or set system clipboard text (`action=get` or `action=set text=...`) |
| `node_monitor` | Watch a directory for file events for a given duration |
| `desktop_script` | Send BMessage scripting commands to any running Haiku app |
| `launch_app` | Launch an application by MIME signature (or bring to front if running) |

`node_monitor` event types: `+` create, `-` remove, `→` move, `*` modify,
`@` attribute change.

### BMessage scripting (desktop_script)

The `desktop_script` tool lets Claude control and query other running
Haiku applications using the standard Be scripting protocol. This is
pluto's killer feature — no other OS has uniform cross-app scripting.

**Actions**: `get`, `set`, `count`, `create`, `delete`, `execute`, `suites`.

**Specifiers**: by name (`name="Desktop"`) or by index (`index=0`).

**Examples**:

```
# List what Tracker supports (discovery)
desktop_script app=application/x-vnd.Be-TRAK action=suites

# Count open Tracker windows
desktop_script app=application/x-vnd.Be-TRAK action=count property=Window

# Get the title of Terminal's first window
desktop_script app=application/x-vnd.Haiku-Terminal action=get property=Title index=0

# Get the frame rectangle of a named window
desktop_script app=application/x-vnd.Be-TRAK action=get property=Frame name=Desktop
```

Always start with `action=suites` to discover what an app supports.
The reply is formatted as readable text with type-aware formatting for
BRect, BPoint, BMessage, strings, numbers, and booleans.

**No other Claude client on any OS has these tools.** That's the point.

---

<a id="beapi-reference"></a>

## BeAPI Reference (RAG)

Pluto ships with a local knowledge base of 390 code snippets covering
202 Be classes. When you ask about Haiku-native code, Pluto
automatically finds and injects relevant examples into the context.

### How it works

1. **Auto-inject**: when your message mentions Be class names (BView,
   BSlider, BMessage...), the top 3 matching snippets are silently
   injected into the system prompt. No action needed.
2. **beapi_lookup tool**: the model can also search proactively by
   calling the `beapi_lookup` tool during code generation.
3. **CLI search**: use `pluto beapi <query>` to search the reference
   yourself from the terminal.

### Examples

```bash
# Search for BSlider examples
pluto beapi BSlider vertical

# Find BFilePanel usage
pluto beapi "open file dialog"

# Look up BView drawing
pluto beapi "BView custom Draw"

# Show index stats
pluto beapi stats

# Export as BFS files (searchable from Tracker)
pluto beapi export
```

### Index location

The index file `beapi-index.jsonl` is searched in order:

1. `./data/beapi-index.jsonl` (development)
2. `~/config/settings/pluto/beapi-index.jsonl` (user-installed)
3. `/boot/system/data/pluto/beapi-index.jsonl` (packaged)

### Custom snippets

Add your own BeAPI examples in
`~/config/settings/pluto/beapi-custom.jsonl`. One JSON object per line:

```json
{"topic":"My custom widget","question":"How to...","answer":"```cpp\n...\n```","classes":["BView"]}
```

The `classes` field is optional — if omitted, Be class names are
auto-extracted from the answer. Custom snippets are merged with the
main index at startup.

### BFS export

Run `pluto beapi export` to write all snippets as individual files in
`~/config/settings/pluto/beapi/`. Each file has BFS attributes:

- `beapi:topic` — the snippet topic (string)
- `beapi:classes` — comma-separated Be class names (string)

This means you can search snippets from **Tracker Find** or with
`query_fs 'beapi:topic == *Slider*'`.

### GUI integration

In Pluto-gui, when RAG snippets are automatically injected, the side
panel shows "BeAPI reference" with the list of topics used, and a
`[beapi]` indicator appears in the chat.

---

<a id="mcp-servers"></a>

## MCP Servers

pluto includes a full MCP (Model Context Protocol) client with three
transport types: HTTP, SSE, and stdio.

### Configuration scopes

Scoped config files are merged with `local > project > user`:

| File | Scope | Notes |
|---|---|---|
| `./.mcp.json` | Project | Requires approval on first use |
| `~/config/settings/pluto/mcp.json` | User | Always loaded |
| `~/config/settings/pluto/mcp-local.json` | Local (per-cwd) | Always loaded |

### Managing servers

```sh
pluto mcp list                      # show all servers
pluto mcp get <name>                # show server details
pluto mcp add <name> --url <url> --scope user
pluto mcp remove <name>
pluto mcp discover                  # auto-detect local MCP servers
pluto mcp reset-project-choices     # clear all project approvals
```

### Auto-discovery

`pluto mcp discover` probes well-known ports (2607, 3000, 4000, 8000, 8080)
for running MCP servers. Found servers are connected automatically for the
session without requiring manual configuration.

In the GUI, auto-discovery also runs at startup and found servers appear in
the MCP menu.

### Project approval

Project-scoped servers (from `./.mcp.json`) prompt the user before loading:

- CLI: `[y]once [a]always [n]o`
- GUI: `BAlert` with Deny / Once / Always buttons

"Always" approvals are saved per-project and can be cleared with
`pluto mcp reset-project-choices`.

### MCP in the GUI

Connected servers appear in the **MCP** menu. Click a server name to see its
tool list in a detail dialog.

---

<a id="sessions-and-memory"></a>

## Sessions and Memory

### Saving and loading sessions

Sessions are JSONL files in `~/config/settings/pluto/sessions/` with
metadata as BFS extended attributes.

**CLI:**

```
/save my refactoring session    # save with title
/list                           # show recent sessions
/load 3                         # load by index
/load refactor                  # load by title substring
/resume                         # load most recent
/delete                         # delete current session file
```

**GUI:**

- Cmd+S saves the session
- Cmd+R resumes the last session
- Session > Recent sessions shows the 15 most recent

### Session metadata (BFS attributes)

| Attribute | Type | Description |
|---|---|---|
| `pluto:title` | string | Session name |
| `pluto:model` | string | Model used |
| `pluto:tokens_in` | int64 | Input tokens consumed |
| `pluto:tokens_out` | int64 | Output tokens consumed |
| `pluto:created` | int64 | Creation timestamp (unix) |
| `pluto:updated` | int64 | Last update timestamp (unix) |
| `pluto:turns` | int32 | Number of conversation turns |
| `pluto:tags` | string | Comma-separated tags |

All are indexed via `fs_create_index`, so you can find sessions from
Tracker's Edit > Find… dialog (e.g. `pluto:title == "*refactor*"`).

### Tagging

```
/tag important
/tag bug-fix
/untag important
```

Tags are stored as a comma-separated `pluto:tags` attribute and are
searchable via BQuery.

### Auto-title

If no title is set, pluto uses the first line of your first message
(truncated to 60 characters).

---

<a id="skills-and-custom-commands"></a>

## Skills and Custom Commands

### Skills

Skills are persistent prompt fragments loaded into every conversation.

Location: `~/config/settings/pluto/skills/<name>/SKILL.md`

Each skill file is wrapped in `<skill name="...">` tags and appended to the
system prompt under a `<skills>` block.

Example: create `~/config/settings/pluto/skills/haiku-dev/SKILL.md`:

```markdown
You are an expert Haiku OS developer. You know the Be API, BFS, and
BMessage scripting. Prefer native Haiku solutions over POSIX fallbacks.
```

### Custom slash commands

Location: `~/config/settings/pluto/commands/<name>.md`

Optional YAML frontmatter with a `description:` field. The placeholder
`{{args}}` is substituted with the user's arguments.

Example: create `~/config/settings/pluto/commands/review.md`:

```markdown
---
description: Code review of a file
---
Review the following file for bugs, style issues, and improvements:

@{{args}}
```

Usage:

```
/review src/main.cpp
```

List all commands with `/commands`.

---

<a id="hook-system"></a>

## Hook System

Hooks are shell commands executed before and/or after each tool call.

### Configuration

In `~/config/settings/pluto`:

```ini
hook_pre_tool = echo "Running tool: $PLUTO_TOOL"
hook_post_tool = make check 2>&1 | tail -5
hook_user_prompt = echo "User said: $PLUTO_PROMPT" >> /tmp/pluto.log
```

Multiple hooks are allowed (repeat the key). They run in order.

### Hook types

| Key | When | Env vars |
|---|---|---|
| `hook_pre_tool` | Before each tool call | `PLUTO_TOOL` |
| `hook_post_tool` | After each tool call | `PLUTO_TOOL` |
| `hook_user_prompt` | Before each user turn | `PLUTO_PROMPT` |

Output goes to stderr. Non-zero exit codes are logged but do not block
tool execution.

---

<a id="non-interactive-execution"></a>

## Non-Interactive Execution

### One-shot mode

```sh
./pluto --one-shot "summarize CHANGELOG.md"
```

Sends a single prompt, streams the response, and exits. Tool calls require
approval unless `--yolo` is used.

### Agent mode

```sh
./pluto agent run /path/to/prompt.md [--model claude-sonnet-4-6]
```

Reads the prompt from a file, auto-approves all tool calls (implicit
`--yolo`), runs to completion, and exits. Ideal for scripts and automation.

### Headless mode

```sh
./pluto -p "list all .cpp files in src/" -f text
./pluto -p "explain this Makefile" -f json -q
```

Executes a single turn with auto-approved tool calls, prints output to
stdout, and exits. Designed for scripting, CI pipelines, and tool
integration.

| Flag | Description |
|---|---|
| `-p`, `--prompt` | The prompt text (required for headless mode) |
| `-f`, `--format` | Output format: `text` (default, streaming) or `json` (structured) |
| `-q`, `--quiet` | Suppress status messages on stderr |

The `json` format produces a structured object:

```json
{
  "model": "claude-opus-4-6",
  "text": "...",
  "input_tokens": 1234,
  "output_tokens": 567,
  "error": false,
  "tool_calls": [{"name": "read_file", "input": "..."}]
}
```

### Plan mode

Toggle plan mode in the REPL:

```
/plan
```

When plan mode is ON, write tools (`write_file`, `edit_file`, `bash`,
`write_attr`, `remove_attr`, `desktop_script`, `launch_app`) are removed
from the tool definitions. The model receives a `<mode>plan</mode>` tag in
the system prompt and is instructed to describe changes without making them.

Toggle again to restore full tool access.

### Dry-run mode

```sh
./pluto --dry-run --one-shot "test prompt"
```

Prints the request JSON that would be sent without calling the API.
Useful for debugging prompts and tool schemas.

### Auto-compaction

When the cumulative input tokens exceed 90% of the configured context
window, pluto automatically summarizes the conversation. Older messages
are replaced with a concise LLM-generated summary, preserving the most
recent user turn. This prevents silent truncation on long sessions.

You'll see `(conversation compacted to free context)` in the CLI when this
happens.

### All subcommands

| Subcommand | Description |
|---|---|
| `pluto` | Interactive REPL (default) |
| `pluto -p TEXT [-f FMT]` | Headless mode: single turn, stdout, exit |
| `pluto --one-shot TEXT` | Single prompt, print response, exit |
| `pluto agent run FILE` | Run prompt from file (auto-approved) |
| `pluto mcp SUBCMD` | Manage MCP servers |
| `pluto trust SUBCMD` | Manage workspace trust |
| `pluto schedule SUBCMD` | Manage scheduled tasks |
| `pluto routine SUBCMD` | Manage routines |
| `pluto login` | Authenticate with Claude Max/Pro account (OAuth) |
| `pluto logout` | Remove saved OAuth tokens |
| `pluto stats` | Show local telemetry (opt-in) |

---

<a id="routines"></a>

## Routines

Routines are reusable prompt templates that can be triggered manually or
automatically. They live as `.md` files in `~/config/settings/pluto/routines/`.

### Routine format

```markdown
---
name: daily-review
schedule: manual
model: claude-sonnet-4-6
tools: read_file, grep
notify: true
enabled: true
---
Review all files modified since yesterday in this project.
Today is {{date}}.
```

### Frontmatter fields

| Field | Default | Description |
|---|---|---|
| `name` | filename | Routine identifier |
| `schedule` | `manual` | `manual`, `on_boot`, `on_file_change <dir>`, or `every Nm`/`every Nh` |
| `model` | *(default)* | Model override |
| `tools` | *(all)* | Comma-separated tool allowlist |
| `notify` | `false` | Send BNotification on completion |
| `enabled` | `true` | Can be toggled with `enable/disable` |

### Template variables

| Variable | Expansion |
|---|---|
| `{{date}}` | Current date and time |
| `{{clipboard}}` | System clipboard text (via `BClipboard`) |
| `{{changed_files}}` | List of changed files (for `on_file_change` triggers) |

### CLI commands

```sh
pluto routine list                  # show all routines
pluto routine run daily-review      # run a routine now
pluto routine enable daily-review   # activate (installs launch_daemon job if automatic)
pluto routine disable daily-review  # pause (removes launch_daemon job)
pluto routine install daily-review  # manually install launch_daemon job
pluto routine uninstall daily-review # manually remove launch_daemon job
pluto routine log daily-review      # show last 50 log lines
```

### Automatic triggers

Routines with a non-manual schedule are automatically managed via Haiku's
`launch_daemon`. When you `enable` such a routine, pluto generates the
appropriate job file; when you `disable`, it removes it.

| Schedule | Behaviour |
|---|---|
| `manual` | Only runs with `pluto routine run <name>` |
| `on_boot` | Runs once on `initial_volumes_mounted` |
| `on_file_change <dir>` | Polls `<dir>` every 30s, fires on mtime change |
| `every 30m` | Loops every 30 minutes (also `every 2h`, etc.) |

Job files are written to `~/config/settings/launch/`. For periodic and
file-change triggers, a wrapper script is generated in
`~/config/settings/pluto/routines/scripts/`.

### Storage

- Routine files: `~/config/settings/pluto/routines/<name>.md`
- Run logs: `~/config/settings/pluto/routine-logs/<name>.log`
- Trigger scripts: `~/config/settings/pluto/routines/scripts/<name>.sh`
- Launch jobs: `~/config/settings/launch/pluto-routine-<name>`

### GUI management

In `pluto-gui`, go to **File > Routines…** to open the Routine Manager:

- **Routine list**: all routines with `[active]` or `[paused]` status
- **Details**: schedule type, model, tool allowlist, notify flag
- **Run now**: executes the routine in a Terminal window, output piped
  to the log file. A `BNotification` is sent if `notify: true`
- **Enable/Disable**: toggles the routine on/off (rewrites frontmatter)
- **Show log**: displays the last 100 lines from the routine's log file
  in a monospace text view within the dialog

Example routines are provided in `examples/routines/` — copy them to
`~/config/settings/pluto/routines/` to get started.

---

<a id="scheduled-tasks"></a>

## Scheduled Tasks

pluto can run prompts on a schedule using Haiku's `launch_daemon`. This lets
you automate recurring tasks like daily code reviews, log summaries, or
project status reports.

### CLI management

```sh
pluto schedule list                                     # list all schedules
pluto schedule add daily-review \
  --file ~/prompts/review.md \
  --event boot \
  --interval 60 \
  --model claude-sonnet-4-6                            # create a schedule
pluto schedule remove daily-review                      # delete a schedule
pluto schedule enable daily-review                      # activate
pluto schedule disable daily-review                     # pause
pluto schedule run daily-review                         # run immediately
```

### Options

| Option | Description |
|---|---|
| `--file <path>` | Path to the `.md` prompt file (required) |
| `--event boot\|login` | Trigger: run on system boot or user login |
| `--interval <mins>` | Repeat every N minutes (generates a looping script) |
| `--model <id>` | Model override for this schedule |

### How it works

- Schedule configs are saved in
  `~/config/settings/pluto/schedules/<name>.conf`.
- For periodic schedules, a shell script and a `launch_daemon` job are
  generated in `~/config/settings/launch/` so the task starts automatically
  on boot/login and loops with `sleep`.
- For one-shot event-based schedules, a direct `launch_daemon` job fires
  `pluto agent run <file>` at the configured event.
- Output goes to `/tmp/pluto-schedule-<name>.log`.

### GUI management

In `pluto-gui`, go to **File > Schedules…** to open the Schedule Manager.

**Schedule list**:
- All configured schedules are shown with their status (`[active]` or `[paused]`)
- Select a schedule to see its details (prompt file, event, interval, model)
- **Enable/Disable** toggles the schedule on or off
- **Run now** executes the schedule immediately in a Terminal window
- **Remove** deletes the schedule (with confirmation)

**Creating a new schedule**:
- Click **Add…** to open the creation form
- Fill in the fields:
  - **Name** — a short identifier (letters, digits, hyphens)
  - **Prompt file** — path to the `.md` prompt file. Click **Browse…** to
    open a native Haiku file dialog and select the file from Tracker
  - **Event** — when to trigger: `(none)`, `boot`, or `login`
  - **Interval** — repeat every N minutes (0 = run once on event)
  - **Model** — optional model override
- Click **Create** to save the schedule and install the `launch_daemon` job

---

<a id="tracker-add-on"></a>

## Tracker Add-on

pluto includes a Tracker context menu add-on called **"Ask pluto"**. When
installed, right-clicking files in Tracker shows an "Ask pluto" entry.
Selecting it opens a Terminal with pluto analyzing the selected files.

### Installing the add-on

**From the GUI:**

Go to **Help > Install Tracker add-on**. The add-on is copied to
`~/config/non-packaged/add-ons/Tracker/`. Right-click any file in Tracker
to see the new entry.

**From the command line:**

```sh
make 'Ask pluto'          # build the add-on (.so)
make install-addon        # install to Tracker add-ons directory
```

**Removing:**

GUI: **Help > Remove Tracker add-on**, or:

```sh
make uninstall-addon
```

### How it works

When you select files in Tracker and click "Ask pluto":

1. The add-on collects the selected file paths
2. It opens a Terminal window with `pluto --one-shot --workspace=<dir>`
3. Each file is passed as an `@path` mention so pluto can read its contents
4. The workspace is set to the selected folder (or the parent of selected files)
5. pluto analyzes the files and shows the result
6. Press Enter to close the Terminal when done

The add-on also works when no files are selected — clicking on the
background of a Tracker window sets the workspace to that directory.
Selecting a single directory sets the workspace to that directory.

The add-on works best when `pluto` is in your `PATH` (copy to
`~/config/non-packaged/bin/` or use the `.hpkg` package).

---

<a id="keyboard-shortcuts-gui"></a>

## Keyboard Shortcuts (GUI)

| Shortcut | Action |
|---|---|
| Enter | Send message |
| Shift+Enter | Insert newline in input |
| Cmd+T | New tab |
| Cmd+W | Close tab |
| Cmd+Right | Next tab |
| Cmd+Left | Previous tab |
| Cmd+N | New conversation |
| Cmd+S | Save session |
| Cmd+R | Resume last session |
| Cmd+Q | Quit |
| Cmd+B | Session browser |
| Cmd+, | Preferences |
| Cmd+F | Find in conversation |
| Cmd+G | Find next match |
| Esc | Close find bar / cancel turn |
| Cmd+= | Zoom in (larger font) |
| Cmd+- | Zoom out (smaller font) |
| Cmd+0 | Reset zoom |

---

<a id="cost-tracking"></a>

## Cost Tracking

### Token accounting

pluto tracks input and output tokens per session. Cache creation and cache
read tokens are tracked separately.

**CLI:** use `/cost` to see totals.

**GUI:** the status bar shows `in X / out Y tok · ctx Z% · $0.XX` and the
token gauge visualises context window usage.

### Prompt caching

Enabled by default. pluto adds `cache_control: {"type": "ephemeral"}` to
the system prompt and the last tool definition to cache the tools+system
prefix across turns.

Cache stats are logged to stderr:

```
[pluto] cache: read=12345 created=678
```

Disable with `--no-cache` or `cache = false` in config.

### Local telemetry

pluto can optionally track token usage and costs over time, stored
strictly locally (no network). To enable:

```sh
touch ~/config/settings/pluto/telemetry
```

Once enabled, every turn's token usage is appended to
`~/config/settings/pluto/stats.csv`. View aggregated stats:

```sh
pluto stats
```

Output:

```
Model                           In tok   Out tok  Turns  Sess.   Cost USD
-----                           ------   -------  -----  -----   --------
claude-opus-4-6                  45000     12000     15      3   $1.5750
claude-sonnet-4-6                 8000      3200      5      2   $0.0720
TOTAL                            53000     15200     20      5   $1.6470
```

Clear all data: `pluto stats reset`. Remove the `telemetry` file to disable.

### Retry logic

On HTTP 429 (rate limit) or 5xx errors, pluto retries up to 3 times with
exponential backoff (1s, 2s, 4s). Connection errors before streaming starts
are also retried. Mid-stream failures are reported immediately to avoid
duplicate output.

Retry attempts are visible to the user:

- **CLI**: messages like `[pluto] HTTP 429, retrying in 2s…` appear on stderr
- **GUI**: the status bar updates with the retry message, and a dim
  `[retry]` line appears in the chat output so you can see exactly when
  and why retries happened

---

<a id="troubleshooting"></a>

## Troubleshooting

### "ANTHROPIC_API_KEY not set"

Set the environment variable or save the key to
`~/config/settings/pluto-api-key`. The GUI prompts automatically.

### "path outside workspace"

The file is outside the trusted directory. Either:
- Run `pluto trust add /path/to/dir`
- Respond `a` (always) to the trust prompt on next launch
- Use `--yolo` (not recommended)

### MCP server won't connect

1. Check the server is running: `curl http://localhost:2607/sse`
2. Check config: `pluto mcp list` and `pluto mcp get <name>`
3. For project servers, check approval: `pluto mcp reset-project-choices`
4. Try auto-discovery: `pluto mcp discover`

### Build fails

Ensure you're on Haiku R1/beta5+ with `gcc` available. Run `make clean`
then `make` to rebuild from scratch. Run `make ci` for a full validation.

### "curl: ..." errors

Check your internet connection. If behind a proxy, set `https_proxy` in the
environment. The CA bundle is at `/system/data/ssl/CARootCertificates.pem`.

### Debug logging

Set `PLUTO_LOG=debug` to enable verbose logging to stderr:

```sh
PLUTO_LOG=debug ./pluto
```

Shows cache stats, retry attempts, and other internal state. Without this
variable, only warnings and errors appear.

---

<a id="for-developers"></a>

## For Developers

### Build targets

| Target | Description |
|---|---|
| `make` | Build `Pluto_cli` (CLI) |
| `make Pluto` | Build `Pluto` (GUI) |
| `make 'Ask pluto'` | Build Tracker add-on (.so) |
| `make install-addon` | Install Tracker add-on |
| `make uninstall-addon` | Remove Tracker add-on |
| `make check` | Run all 18 test suites |
| `make ci` | Clean build + full test suite |
| `make package` | Create `.hpkg` for HaikuDepot |
| `make clean` | Remove objects and binaries |

### Test suites

| Test | Covers |
|---|---|
| `test_json` | JSON parser/serializer roundtrip |
| `test_session` | SessionStore save/load/list/tag/remove via tmpdir |
| `test_sse` | SSE stream parser and event emission |
| `test_tools` | Built-in tools on isolated tmpdir |
| `test_workspace` | Workspace trust, scoping, access logging |
| `test_desktop_script` | BMessage scripting via desktop_script tool |
| `test_image` | Base64 encoding, IMAGE content blocks, JSON roundtrip |
| `test_schedule` | Schedule save/load/list/remove, launch_daemon job gen |
| `test_routine` | Routine parsing, template expansion, enable/disable, logging |
| `test_tracker_addon` | Tracker add-on .so loading and symbol export |
| `test_syntax` | Syntax highlighting lexer for C++, Python, Shell, JS |
| `test_oauth` | OAuth PKCE, token storage, expiry |
| `test_agent_store` | Agent parsing, install, list, remove |
| `test_skills` | Always-on skills, triggered skills, injection |
| `test_markdown` | Terminal markdown rendering |
| `test_rag` | BeAPI retrieval, TF-IDF scoring, class extraction |
| `test_history` | libedit history integration |
| `test_ipc` | Multi-instance peer discovery and messaging |
| `test_toolblock` | ToolBlockManager offset management (collapse/expand) |
| `test_streaming_md` | StreamingMarkdown line buffering and code block state |

All tests are self-contained (no external framework) and use `CHECK` macros
with tmpdir isolation.

### Source layout

```
src/
  main.cpp, repl.cpp            CLI entry + interactive loop
  config.{h,cpp}                configuration parser + hooks
  cancel.{h,cpp}                cooperative cancel flag (SIGINT)
  workspace.{h,cpp}             workspace trust + access logging
  routine.{h,cpp}               routine engine (parse, expand, run, log)
  schedule.{h,cpp}              scheduled task management (launch_daemon)
  telemetry.{h,cpp}             opt-in local token/cost tracking
  base64.h                      header-only base64 encoder
  input.h                       shared image detection + @mention expansion
  json.{h,cpp}                  self-contained JSON parser
  agent/Loop.{h,cpp}            agentic turn loop
  tools/                        6 built-in tools
  haiku/BfsTools.{h,cpp}        BFS attrs + BQuery tools
  haiku/DesktopTools.{h,cpp}    clipboard + node monitor
  haiku/TrackerAddOn.cpp         Tracker context menu add-on (.so)
  storage/SessionStore.{h,cpp}  JSONL session storage + BFS attrs
  mcp/                          MCP client + manager + transports
  provider/                     Anthropic + OpenAI-compat providers
  gui/                          BApplication, MainWindow, dialogs,
                                RoutineDialog, ScheduleDialog,
                                SessionBrowser, settings
tests/                          20 test runners
docs/                           architecture, user guide, man page
```

See `docs/architecture.md` for module responsibilities, data flow diagrams,
and the GUI threading model.

### Contributing

The project uses a simple Makefile with header dependency tracking via
`-MMD -MP`. No external build system required.

Before submitting changes:

1. `make ci` must pass
2. Both `pluto` and `pluto-gui` must build without warnings
3. New features should have a test in `tests/` where feasible

---

*If you're reading this from a lesser OS: we're sorry. We can't help you.
But you can always install Haiku.*
