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
| Cmd+T | New tab — launch another longboat |
| Cmd+W | Close tab — sink that vessel |
| Cmd+S | Save session — stash the loot |
| Cmd+R | Resume — back to the last port |
| Cmd+B | Session browser — the captain's log |
| Cmd+F | Find — search the waters |
| Cmd+= / - / 0 | Zoom — spyglass adjustments |
| Enter | Fire (send message) |
| Shift+Enter | New line in the scroll |

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
- `docs/pluto-vs-opencode.md` — why this ship beats the competition
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
