# Pluto vs OpenCode — Confronto tecnico

> Analisi comparativa tra Pluto (client AI nativo Haiku OS) e
> OpenCode (agente AI coding open source cross-platform).
> Aggiornato al 25 aprile 2026.

---

## Panoramica

| | **Pluto** | **OpenCode** |
|---|---|---|
| Tipo | Client AI nativo Haiku OS | Agente AI coding cross-platform |
| Linguaggio | C++17 | Go |
| LOC | ~35.000 | ~80.000+ (stima) |
| Licenza | MIT | MIT |
| Piattaforme | Haiku OS | macOS, Linux, Windows |
| Interfacce | CLI + GUI nativa (Be API) + Tracker add-on | TUI (Bubble Tea) + Desktop (Electron) + IDE extension |
| Binary CLI | **1.4 MB** | ~30 MB |
| Binary GUI | **2.5 MB** | ~100 MB+ (Electron) |
| Dipendenze runtime | libcurl, libbe, OpenSSL, libedit | Go runtime, SQLite, Node.js |
| Installazione | `make` + `make package` (.hpkg) | npm / brew / curl / scoop / docker |
| GitHub stars | — (non pubblicato) | 140K+ |
| Community | Haiku OS community | 850+ contributors, 6.5M utenti/mese |

---

## Tool built-in

| Tool | **Pluto** | **OpenCode** |
|------|:---------:|:------------:|
| read_file | Si | Si |
| write_file | Si | Si |
| edit_file | Si | Si |
| apply_patch | Si | Si |
| bash | Si (timeout, workspace sandbox) | Si |
| grep | Si | Si (ripgrep) |
| glob | Si | Si |
| todo_write | Si | Si |
| web_fetch | Si | Si |
| web_search | Si | Si (Exa AI) |
| skill (prompt injection) | Si | Si |
| lsp | **Si** (clangd, pylsp, gopls) | Si (20+ linguaggi, auto-download) |
| question (ask user) | — | **Si** |
| **read_attr** (BFS) | **Si** | — |
| **write_attr** (BFS) | **Si** | — |
| **list_attrs** (BFS) | **Si** | — |
| **remove_attr** (BFS) | **Si** | — |
| **query_fs** (BQuery) | **Si** | — |
| **live_query** (BQuery live) | **Si** | — |
| **node_monitor** (BNodeMonitor) | **Si** | — |
| **desktop_script** (BMessage) | **Si** | — |
| **clipboard** (BClipboard) | **Si** | — |
| **launch_app** (be_roster) | **Si** | — |
| **beapi_lookup** (RAG 390 snippet) | **Si** | — |
| | **22 tool** | **13 tool** |

Pluto ha **11 tool esclusivi Haiku-native** che nessun altro client AI puo
offrire su qualsiasi altro OS. OpenCode ha LSP integrato (20+ linguaggi)
e `question` tool che Pluto non ha.

---

## Provider e modelli

| Provider | **Pluto** | **OpenCode** |
|----------|:---------:|:------------:|
| Anthropic Claude | Si (API key + OAuth PKCE) | Si |
| OpenAI | Si | Si |
| Ollama (locale) | Si (auto-detect al primo avvio) | Si |
| Google Gemini | Si | Si |
| Groq | Si | Si |
| DeepSeek | Si | Si |
| Grok (xAI) | Si | Si |
| Mistral AI | Si | Si |
| OpenRouter | Si | Si |
| Cerebras | Si | Si |
| AWS Bedrock | Si | Si |
| Azure OpenAI | Si | Si |
| GitHub Copilot | Si | Si |
| LM Studio | Si | Si |
| Provider custom | Si (.conf file) | Si (config) |
| OAuth PKCE login | **Si** (browser flow) | Si |
| Totale built-in | **15** (+ custom illimitati) | **75+** |

Pluto copre tutti i provider principali. La differenza: in OpenCode i
provider sono hardcoded nel binario Go; in Pluto sono **file `.conf`
editabili** — per aggiungerne uno basta copiare un file, senza ricompilare.

```
data/providers/01-openai.conf     # built-in, shippato col pacchetto
data/providers/02-ollama.conf
...
~/config/settings/pluto/providers/mio-provider.conf  # user-defined
```

---

## MCP (Model Context Protocol)

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| Client MCP | Si | Si |
| Trasporto stdio | Si | Si |
| Trasporto SSE | **Si** | Deprecato (issue aperte) |
| Trasporto HTTP | Si | Parziale |
| Scope config | **3 livelli** (project/user/local) | 1 (project) |
| Auto-discovery server locali | **Si** (probe localhost) | — |
| Approval per-server | Si (Deny/Once/Always) | Si |
| System prompt injection | **Si** (tool descriptions) | Si |
| CLI management | `pluto mcp list/add/remove` | config file |

Pluto ha un client MCP piu completo: 3 trasporti funzionanti, 3 scope,
auto-discovery. OpenCode ha problemi documentati con SSE deprecato e
mancanza di supporto stdio in configurazione remota.

---

## Sessioni e persistenza

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| Storage | JSONL + **BFS attributes** | SQLite |
| Ricerca sessioni | **BQuery** (indicizzata BFS) | SQLite query |
| Cercabile dal file manager | **Si** (Tracker Find) | — |
| Auto-compaction | Si (80% context) | Si (95% context) |
| Export | Markdown + JSONL | Share link |
| Session browser GUI | **Si** (filtro live) | Lista TUI |
| Snapshot/undo file | — | **Si** (auto-snapshot) |

Pluto sfrutta BFS per rendere le sessioni cercabili nativamente dal
file manager. OpenCode usa SQLite (piu portatile) e offre undo automatico.

---

## GUI e UX

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| GUI nativa OS | **Si** (Be API, BWindow) | TUI + Electron |
| Theming OS-native | **Si** (ui_color, dark/light auto) | Temi custom |
| Multi-tab | **Si** (BTabView, fork) | — |
| Split view chat/detail | **Si** (BSplitView) | — |
| Mini-map conversazione | **Si** | — |
| Inline diff (edit_file) | **Si** (rosso/verde) | Diff view |
| Image preview | **Si** (BBitmap) | — |
| Markdown rendering | **Si** (md4c nativo) | Si (TUI) |
| Syntax highlighting | **Si** (C++/Py/Shell/JS) | Si |
| Drag & drop file da Tracker | **Si** | — |
| Collapsible tool blocks | **Si** | — |
| Token gauge (barra grafica) | **Si** | Percentuale testo |
| Cost estimate live | **Si** ($0.0123) | — |
| Streaming cursor animato | **Si** (blink block) | Spinner |
| Deskbar replicant | **Si** (icona 16x16) | — |
| BNotification su turn done | **Si** | — |
| Tracker add-on "Ask Pluto" | **Si** (right-click) | — |
| Font zoom persistente | **Si** (Cmd+/-/0) | — |
| Stop button | **Si** | Ctrl+C |

Pluto ha una GUI nativa significativamente piu ricca — multi-tab, split
view, mini-map, inline diff, image preview, Deskbar replicant sono feature
impossibili in un TUI o in Electron senza l'integrazione OS.

---

## Workflow e automazione

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| Plan/Build modes | Si (built-in + custom) | Si |
| Custom slash commands | Si (~/commands/*.md) | Si |
| Skills (prompt injection) | **Si** (always-on + trigger) | Si |
| Hooks (pre/post tool) | **Si** | Si |
| Routines ricorrenti | **Si** (on_boot, on_file_change) | — |
| Scheduled tasks | **Si** (launch_daemon) | — |
| Agent store (install/update) | **Si** (Git + local) | — (community plugins) |
| Project memory file | pluto.md | AGENTS.md |
| One-shot mode | Si | Si |
| Dry-run (zero API call) | **Si** | — |
| @file mentions | Si | Si |
| Image input (vision) | Si | Si |
| Undo/redo file changes | — | **Si** |
| GitHub Actions bot | — | **Si** (/opencode in PR) |
| Share conversation (link) | — | **Si** |
| API server mode (headless) | — | **Si** |
| Mobile remote control | — | **Si** |

Pluto eccelle nell'automazione locale (routines, schedules, agent store,
dry-run). OpenCode eccelle nei workflow di team (GitHub Actions, share,
API server, mobile).

---

## Sicurezza

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| Workspace trust | **Si** (first-run, persist) | — (usa cwd) |
| Access logging | **Si** (timestamp, allow/deny) | — |
| Tool approval | Si (Deny/Allow/Always) | Si (allow/deny/ask) |
| Symlink protection | **Si** (realpath) | — |
| Path traversal guard | **Si** (@file, edit, patch) | — |
| Regex DoS guard | **Si** (pattern cap) | — |
| File lock multi-istanza | **Si** (ScopedFileLock) | — |
| Agent review pre-install | **Si** (80 righe preview) | — |
| File auto-snapshot | — | **Si** |
| OAuth PKCE | Si | Si |
| Token chmod 600 | Si | Si |

Pluto ha un perimetro di sicurezza piu robusto per l'accesso ai file.
OpenCode ha il vantaggio dell'auto-snapshot per recovery.

---

## Multi-istanza e IPC

| | **Pluto** | **OpenCode** |
|---|:---------:|:------------:|
| Multi-istanza | **Si** (B_MULTIPLE_LAUNCH) | — |
| IPC tra istanze | **Si** (PlutoMessenger, BRoster) | — |
| Scripting protocol BeOS | **Si** (`hey Pluto get Workspace`) | — |
| /peers discovery | **Si** | — |
| Workspace collision detect | **Si** | — |
| File lock su config condiviso | **Si** | — |

Feature esclusiva Haiku — impossibile su altri OS senza infrastruttura
custom (D-Bus, named pipes, etc).

---

## Performance

| Metrica | **Pluto** | **OpenCode** |
|---------|-----------|------------|
| JSON parse 50KB | 1.3 ms | N/D (Go encoding/json) |
| RAG search 390 entry | 13.5 ms | N/A |
| Session save 20 turns | 3.6 ms | N/D (SQLite) |
| Session load 20 turns | 1.0 ms | N/D (SQLite) |
| Startup | ~50 ms | ~500 ms+ |
| RAM (CLI) | ~5 MB | ~50 MB+ |
| RAM (GUI) | ~15 MB | ~200 MB+ (Electron) |
| Binary CLI | **1.4 MB** | ~30 MB |
| Binary GUI | **2.5 MB** | ~100 MB+ |

Pluto ha un footprint 20-70x inferiore: C++ nativo senza runtime GC,
senza VM, senza Electron.

---

## Cosa Pluto fa che OpenCode non puo

1. **11 tool Haiku-native**: BFS attrs, BQuery, live_query, node_monitor,
   desktop_script, clipboard, launch_app, beapi_lookup — impossibili altrove
2. **GUI nativa** con multi-tab, split view, mini-map, inline diff, image
   preview, Deskbar replicant, Tracker drag & drop
3. **Routines e schedules** via launch_daemon — automazione OS-level
4. **IPC multi-istanza** via BMessenger/BRoster/scripting protocol
5. **Workspace trust con audit log** — sicurezza enterprise-grade
6. **RAG locale BeAPI** — 390 snippet per codice Haiku corretto al primo colpo
7. **1.4 MB binary**, ~5 MB RAM — funziona su hardware minimo
8. **Provider da file .conf** — aggiungere provider senza ricompilare
9. **Tracker add-on "Ask Pluto"** — right-click su qualsiasi file
10. **Auto-detect Ollama** al primo avvio — zero-config locale

## Cosa OpenCode fa che Pluto non fa (ancora)

1. ~~**LSP integrato**~~ — Pluto ora ha LSP (hover, definition, references, diagnostics, symbols)
2. **Undo/redo** file changes con auto-snapshot
3. **GitHub Actions** — bot `/opencode` nei commenti PR
4. **Share conversation** — link condivisibili per team
5. **API server mode** — headless, controllabile da remoto/mobile
6. **Cross-platform** — macOS, Linux, Windows
7. **Community 140K+ stars** — ecosistema plugin maturo

---

## Conclusione

Pluto e OpenCode operano in nicchie complementari:

**Pluto** e il client AI piu profondamente integrato con il suo OS che
esista. 21 tool, GUI nativa, IPC, BFS, BQuery, routines, agent store —
tutto progettato per sfruttare cio che solo Haiku puo offrire. Binary
70x piu piccolo, RAM 10x inferiore, zero dipendenze runtime, provider
configurabili da file.

**OpenCode** e l'agente AI coding piu versatile e portabile. 75+ provider
nativi, LSP, GitHub Actions, undo/redo, share, 140K stars, 6.5M utenti.
Ma non puo toccare il filesystem con attributi, non puo mandare BMessage,
non puo vivere nel Deskbar.

> Non sono concorrenti. Sono la dimostrazione che ogni OS merita il suo
> client AI nativo — e che Haiku merita il migliore.

---

*Generato il 2026-04-25.*

Sources:
- [OpenCode Official Site](https://opencode.ai/)
- [OpenCode GitHub](https://github.com/opencode-ai/opencode)
- [OpenCode Tools Docs](https://opencode.ai/docs/tools/)
- [OpenCode MCP Config](https://blog.iamcristhian.dev/2026/04/opencode-advanced-configuration-mcp-servers)
- [OpenCode vs Claude Code (DEV)](https://dev.to/tech_croc_f32fbb6ea8ed4/opencode-vs-claude-code-which-ai-cli-coding-agent-wins-in-2026-45md)
- [Free LLM API Resources](https://github.com/cheahjs/free-llm-api-resources)
