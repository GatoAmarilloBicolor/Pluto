# Pluto 0.6.0

Ahoy, ye scurvy coders! This here be **Pluto** — a Haiku-native AI client
forged in C++17, tempered in the fires of the Be API, and launched from the
deck of the only OS brave enough to have multithreaded, attribute-rich,
query-aware file systems since 1995 while the rest of the fleet was still
arguin' about long filenames.

No Electron. No Node. No Python. Just libcurl, OpenSSL, and `libbe` — the
holy trinity of a true Haiku seafarer. She ships as three binaries from the
same hull: a streaming REPL (`Pluto_cli`), a native GUI (`Pluto`) with
multi-tab conversations and markdown rendering, and a Tracker add-on
(`Ask pluto`) for right-click AI queries on yer selected files.

```
       _       _
 _ __ | |_   _ | |_  ___
| '_ \| | | | || __|/ _ \
| |_) | | |_| || |_| (_) |
| .__/|_|\__,_| \__|\___/
|_|
  Hoist the sails, we're codin' now.
```

---

![Pluto GUI — tool use in action](docs/gui-tool-use.png)
*The AI reads project files, runs commands, and shows tool output in the detail panel.*

![Pluto GUI — image vision](docs/gui-vision.png)
*Drag & drop an image and the model analyzes it with full markdown rendering.*

---

## The Fleet (Three Binaries, One Hull)

Pluto ships as three binaries, each forged for a different kind of voyage:

| Binary | What it be | How to summon |
|--------|-----------|---------------|
| **`Pluto_cli`** | Streaming REPL for the terminal. Lightweight, scriptable, pipes like a true Unix sailor | `pluto` from Terminal |
| **`Pluto`** | Native Haiku GUI with multi-tab conversations, markdown rendering, and drag & drop | Double-click in Tracker or `Pluto` from Terminal |
| **`Ask pluto`** | Tracker add-on — right-click files, send 'em to the AI | Right-click → "Ask pluto" in Tracker |

---

## The Treasure Map (Features)

### The Ship's Core

- **Streaming** — incremental SSE parser, text appears as Claude types it,
  fast as a cannonball across the bow
- **Multi-provider** — Anthropic (Claude), OpenAI, Gemini, OpenRouter,
  Ollama, LM Studio, Cerebras, Groq, DeepSeek, Mistral, Azure OpenAI,
  AWS Bedrock, GitHub Copilot, and any OpenAI-compatible vessel on the seas
- **23 built-in tools** — `read_file`, `write_file`, `edit_file`, `bash`,
  `grep`, `glob`, `web_fetch`, `web_search`, `wiki_lookup`, `lsp` (code
  intelligence via clangd/pylsp/gopls), plus 11 Haiku-native tools that no
  other ship can carry (BFS attributes, BQuery, live queries, node monitor,
  clipboard, desktop scripting, app launcher)
- **MCP client** — HTTP, SSE, stdio transports; project/user/local scopes;
  per-project approval; remote tools surfaced as first-class cannons
- **Prompt caching** — ephemeral `cache_control` on system + tools prefix,
  saves doubloons on every voyage
- **Retry** — exponential backoff on 429, 5xx, connection storms
- **Cancel** — Ctrl+C (CLI) or Stop button (GUI), abandon ship mid-stream

### Haiku-Native Plunder (Things No Other Ship Can Do)

These here be the treasures ye won't find on any other OS. Arr!

- **BFS extended attributes** — `read_attr`, `write_attr`, `list_attrs`,
  `remove_attr` let yer AI parrot interact with Haiku's object database
- **BQuery** — `query_fs` with full predicate syntax. `live_query` for
  real-time file monitoring. Like havin' a crow's nest that sees everything
- **Desktop scripting** — `desktop_script` sends `BMessage` to running apps
  via the BeOS scripting protocol. Command the entire fleet from one wheel
- **Clipboard** — `clipboard` get/set via `BClipboard`
- **Node monitor** — `node_monitor` watches directory changes, like a lookout
  spottin' enemy sails on the horizon
- **Tracker add-on** — right-click files in Tracker, "Ask pluto" opens a
  terminal with the booty attached to the prompt
- **Deskbar replicant** — a persistent flag in the Deskbar shelf, always
  ready for duty
- **Theming** — live dark/light adaptation via `ui_color()`, follows the
  captain's orders (system theme)
- **Multi-instance IPC** — `BRoster` peer discovery, scripting suite,
  `/peers`, `/ask`, `/delegate` commands. A whole fleet o' Plutos talkin'
  to each other
- **Session indexing** — JSONL sessions with BFS attributes searchable from
  Tracker Find. Yer old conversations never lost at sea

### The Knowledge Vault

- **BeAPI RAG** — 390 code snippets for 202 Be classes, auto-injected
  into the prompt when relevant. Like havin' the BeBook tattooed on yer arm
- **Wiki LLM** — project knowledge base in markdown files with BFS
  attributes. `pluto wiki ingest <url>` fetches and analyzes content;
  `wiki_lookup` tool searches it during conversations. Arr, yer ship
  remembers everything!
- **Agent store** — install/remove/update agents from local paths or Git
  repos. Security review before any scallywag code gets aboard
- **Routines** — recurring workflows with templates, triggers (manual,
  on_boot, on_file_change, every Nm/Nh), `launch_daemon` integration.
  Set it and forget it while ye sleep in the hammock
- **Schedules** — periodic or event-based agent execution via `launch_daemon`

![Schedules dialog](docs/gui-schedules.png)
*Create recurring tasks with the Schedules dialog — powered by Haiku's launch_daemon.*

![Agent Store](docs/gui-agent-store.png)
*Browse, install, and inspect agents from the Agent Store.*
- **Skills** — always-on or trigger-based SKILL.md injection
- **Custom commands** — `~/config/settings/pluto/commands/*.md` with
  `{{args}}` substitution. Yer own secret signals
- **Hook system** — `hook_pre_tool`, `hook_post_tool`, `hook_user_prompt`

### The Captain's Quarters (GUI)

- **Multi-tab** — up to 10 concurrent conversations, each with its own
  crew (worker thread, state, session). Cmd+T to hoist a new tab
- **Markdown rendering** — powered by md4c. Headings, bold, italic, code
  blocks with syntax highlighting, lists, blockquotes, tables, task lists,
  strikethrough, clickable links. Professional as a captain's log
- **Detail panel** — full tool output with code blocks framed in
  `┌─ lang ──` / `└────────`, blockquotes with thick `┃` bars,
  auto-width tables with box drawing
- **Thinking spinner** — `⠋ thinking…` braille animation while the AI
  ponders yer question
- **Syntax highlighting** — C++, Python, Shell, JavaScript
- **Inline diff viewer** — `edit_file` shows red/green diffs
- **Image input** — drag & drop from Tracker or paste bitmap clips
- **Image preview** — decoded via `BTranslatorRoster`
- **Collapsible blocks** — click `▾`/`▸` to show/hide tool output
- **Session browser** — searchable list (Cmd+B)
- **Session branching** — fork to new tab
- **Mini-map** — color-coded conversation overview
- **Find** — Cmd+F with wrap-around
- **Context gauge** — color-coded token budget bar
- **Cost tracker** — live USD estimate
- **Export** — Markdown or JSONL
- **Settings** — 15+ provider presets, font zoom, color scheme

### The Armored Hull (Security)

This vessel has been through **three rounds of audit** — 34 bugs fixed,
zero critical remaining. She's seaworthy:

- Workspace sandbox with path canonicalization
- Shell injection prevention in all shell-out paths
- Symlink resolution before workspace checks (`realpath()`)
- Regex DoS protection (pattern length caps)
- SSRF prevention in MCP SSE transport
- Atomic file writes for config, keys, OAuth tokens
- File locking for multi-instance safety (`flock(2)`)
- Orphan `tool_use` repair after cancel (no more HTTP 400)

---

## Hoistin' the Sails (Installation)

### From Release

```sh
unzip pluto-0.6.0-haiku-x86_64.zip
cd pluto-0.6.0
./install.sh    # copies everything to ~/config/non-packaged/
```

### From Source

Requires Haiku R1/beta5+ with `gcc`, `libcurl`, `libedit`, and `libbe`.

```sh
make              # CLI
make Pluto        # GUI
make 'Ask pluto'  # Tracker add-on
make check        # 21 test suites
make bench        # performance benchmarks
```

---

## Settin' Sail (Quickstart)

### First Launch (the Welcome Wizard)

On first run, Pluto asks ye to pick yer provider:

```
  Welcome to Pluto!

  Choose your LLM provider:

    1. Anthropic (Claude)         — API key or Claude Max/Pro
    2. Google (Gemini)            — free API key
    3. OpenAI (GPT)               — API key
    4. Local (Ollama/LM Studio)   — no key needed
    5. OpenRouter                 — API key, many models
    6. xAI (Grok)                 — API key
    7. Cerebras                   — fast inference, free tier
```

Pick yer ship and the config be saved. Change anytime with `/model` or
the Settings dialog.

### With Claude Max/Pro (OAuth — the Captain's way)

```sh
pluto login       # opens browser, PKCE flow
pluto             # sails with yer Claude account
```

### With an API Key (the Pirate's way)

```sh
export ANTHROPIC_API_KEY=sk-ant-…
pluto
```

Or let the GUI ask ye for the key on first launch — she'll stow it safely
in `~/config/settings/pluto-api-key` (chmod 600, arr).

### With Local Treasure (Ollama)

```sh
pluto --provider openai-compat --base-url http://localhost:11434 --model llama3.1
```

No API key. No registration. No doubloons spent. Just ye and yer local model.

### One-Shot Cannonball

```sh
pluto --one-shot "summarize CHANGELOG.md"
pluto --one-shot "explain @src/main.cpp"
```

---

## The Captain's Quarters (Using the GUI)

Launch `Pluto` from Tracker or Terminal. On first run she'll probe for a
local Ollama on `localhost:11434` — if found, ye can start chattin' with no
key at all. Otherwise she'll open Preferences so ye can pick yer provider
and stow yer API key.

### Navigatin' the Deck

Type yer message in the input bar at the bottom and press **Enter** to fire.
**Shift+Enter** adds a new line without sendin'. Press **Escape** to cancel
a request mid-stream — abandon ship!

The conversation appears in a split view: the main chat on the left, and a
**detail panel** on the right for tool output, project scans, and rendered
markdown. Click the status bar at the bottom to quick-switch models.

### Tabs — Launch Another Longboat

**Cmd+T** hoists a new tab. Each tab be an independent conversation with its
own model, session, and history. Right-click a tab for options: close,
close others, rename. **Cmd+Left/Right** switches between tabs.

**Fork to new tab** (Session menu) duplicates yer current conversation into
a fresh tab — perfect for explorin' a different course without losin' the
original.

### Sessions — The Captain's Log

- **Cmd+S** — save the current session to the hold
- **Cmd+R** — resume where ye left off
- **Cmd+B** — open the **Session Browser**: a searchable list of all saved
  sessions with dates, models, and token counts. Double-click to restore one.
  Sessions be stored as JSONL with BFS attributes — searchable from
  Tracker's Find dialog too!

### Preferences — Adjustin' the Sails

![Provider list](docs/gui-providers.png)
*15+ providers ready to sail — from Anthropic to local Ollama.*

![Model selection](docs/gui-model-select.png)
*Pick a model from the preset list or fetch available models from the server.*

**Cmd+,** opens Preferences. Pick yer provider from the dropdown:
Anthropic (API key or OAuth), OpenAI, Gemini, Ollama, OpenRouter, and 13+
more. Set yer model, max tokens, context window, color scheme (Auto follows
the system theme), and system prompt. Hit **Fetch Models** to pull the list
from a remote server.

### Drag & Drop — Loadin' the Cargo

Drop files anywhere on the window:
- **Images** (PNG, JPG) → attached as vision input for the model
- **PDFs** → attached as document context
- **Code and text files** → attached as text context
- **Bitmap clips** from ShowImage, Screenshot, or the clipboard → translated
  to PNG and attached

If a dropped file be outside the workspace, Pluto asks permission before
touchin' it. No scallywag code runs without yer say-so.

### Slash Commands in the GUI

Type `/` in the input bar and an autocomplete popup appears. Navigate with
**Up/Down**, accept with **Tab** or **Enter**. Key commands:

```
/model [ID]      switch yer navigation charts
/tools           inspect the armory
/cost            count the doubloons spent
/init            AI-powered project scan (generates pluto.md)
/wiki [url]      ingest knowledge into the wiki
/plan            toggle plan mode (read-only, no file writes)
/routine <name>  run an automated routine
/reset           clear the deck
/help            summon the command list
```

Custom commands from `~/config/settings/pluto/commands/<name>.md` show up
too — yer own secret signals.

### The Menus

| Menu | Highlights |
|------|-----------|
| **File** | New conversation, New/Close tab, Preferences, Routines, Agents, Login/Logout, Export |
| **View** | Session browser, Find (Cmd+F), Zoom, Tab navigation |
| **Session** | Save, Resume, Recent sessions, Fork to new tab, Reset |
| **MCP** | Connected MCP servers, Discover servers, Peer instances |
| **Help** | User guide, Install Deskbar icon, Install Tracker add-on, Stats, About |

### Tracker Add-On — Ask Pluto

Right-click files or folders in Tracker, select **"Ask pluto"**. The
selected files arrive as attachments in a new conversation. If ye select a
single directory, it becomes the workspace. Install it from Help →
"Install Tracker add-on", or `make install-addon`.

### Deskbar Replicant

Install from Help → "Install Deskbar icon" for a persistent flag in the
Deskbar shelf — always ready for duty, like a loyal parrot on yer shoulder.

---

## The Crew Commands (REPL)

```
/help                summon the command list
/quit                abandon ship
/reset               clear the deck (conversation history)
/tools               inspect the armory
/model [ID]          switch yer navigation charts
/cost                count the doubloons spent
/save [title]        stash the session in the hold
/load                retrieve a stashed session
/resume              pick up where ye left off
/peers               spot other Pluto vessels on the sea
/ask <peer> <text>   send a message in a bottle
/watch [URL]         listen to SSE signals from afar
/wiki [url|file]     ingest knowledge into the wiki
/routines            list yer automated crew
/commands            list yer secret signals
```

Arrows, Ctrl-A/E/K/W, Home/End (libedit). History in
`~/config/settings/pluto/history`. **Ctrl+C** cancels mid-stream.

---

## The Treasure Chest (BFS Queries)

```
query_fs predicate='name == "*.cpp" && size > 1024'
query_fs predicate='BEOS:TYPE == "text/x-source-code"'
query_fs predicate='last_modified > %now-1day%'
query_fs predicate='pluto:title == "*refactor*"'   limit=20
```

Saved sessions get BFS indexes on `pluto:title`, `pluto:model`,
`pluto:tags` — searchable from Tracker Find. No other AI client
on any OS can do this. That be the point, matey.

---

## The Helm (Configuration)

`~/config/settings/pluto/config`:
```
provider       = anthropic
model          = claude-sonnet-4-5
max_tokens     = 16384
context_window = 200000
cache          = true
```

Custom providers: drop a `.conf` in `~/config/settings/pluto/providers/`.

Hooks:
```
hook_pre_tool  = echo "about to fire $PLUTO_TOOL"
hook_post_tool = echo "$PLUTO_TOOL has been fired, cap'n"
```

Workspace trust — on first launch, Pluto asks permission before touchin'
any files. No scallywag code runs without yer say-so.

---

## The Ship's Charts (Keyboard Shortcuts)

| Shortcut | Action |
|----------|--------|
| Enter | Fire (send message) |
| Shift+Enter | New line in the scroll |
| Escape | Cancel request / dismiss autocomplete |
| Up/Down | Prompt history (or autocomplete navigation) |
| Cmd+N | New conversation — clear the deck |
| Cmd+T | New tab — launch another longboat |
| Cmd+W | Close tab — sink that vessel |
| Cmd+S | Save session — stash the loot |
| Cmd+R | Resume — back to the last port |
| Cmd+B | Session browser — the captain's log |
| Cmd+F | Find — search the waters |
| Cmd+, | Preferences — adjust the sails |
| Cmd+= / - / 0 | Zoom — spyglass adjustments |
| Cmd+Left / Right | Previous / next tab |
| Cmd+Q | Quit — return to port |

---

## The Ship's Blueprint

```
src/                        149 files, ~33000 LOC
  main.cpp, repl.cpp        the wheel and the compass
  config.{h,cpp}            the navigator's charts
  agent/Loop.{h,cpp}        the engine room
  provider/                 Anthropic + OpenAI-compat cannons
  tools/                    the armory (22 weapons)
  haiku/                    Haiku-native treasures
  mcp/                      MCP fleet communications
  storage/                  the cargo hold (sessions)
  ipc/                      ship-to-ship messaging
  gui/                      the captain's quarters
data/beapi-index.jsonl      390 BeAPI scrolls
tests/                      21 test crews
examples/                   agents, routines, providers
docs/                       maps and manuals
```

---

## The Captain's Library

- `docs/user-guide.md` — the full sailing manual
- `docs/architecture.md` — the ship's blueprints
- `docs/mcp-tutorial.md` — build yer own MCP vessel in 10 minutes
- `docs/bquery-cookbook.md` — 15+ BQuery treasure maps
- `docs/pluto-press-kit.md` — press kit for the curious journalist
- `docs/pluto.1` — man page (`man pluto` after install)

---

## The Horizon (Roadmap)

See `TODO.md` for the full navigation chart.

**Recent plunder (0.5.0–0.6.0):**
Web fetch, web search, plan mode, headless mode, auto-compaction,
LSP integration, thinking spinner, bitmap clip support, detail panel
restyling, 4 new provider profiles (Mistral, Azure, Bedrock, Copilot)

**Next voyage:**
Undo/redo, native Gemini provider, replicant input bar, plugin system

---

## The Pirate's Code (License)

MIT — free as in Walter, free as in the seven seas.

---

## About

A production of the **Movement for the Liberation of Walter OS** — because
every operating system deserves its own AI client, especially the one that
had multithreaded, attribute-rich, query-aware file systems since 1995 while
everyone else was still arguing about long filenames.

```
        / \__
       (    @\___     Woof!
       /         O
      /   (_____/
     /_____/  U  PLUTO
              the pirate dog of Haiku
```

If ye're readin' this from a lesser OS: we be sorry. We can't help ye.
But ye can always install Haiku. Fair winds, sailor.
