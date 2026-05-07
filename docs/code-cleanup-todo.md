# Pluto Code Cleanup — TODO

Checklist operativa per conformita' alle Haiku Coding Guidelines.
Riferimento: `docs/code-review-report.md`

---

## Fase 1 — Fondamenta (non rompe API)

- [x] **Creare src/util.h/.cpp** (completato 2026-04-30)
  - [x] `HomeDir()` — wrapper per getenv("HOME")
  - [x] `SettingsDir()` — ~/config/settings/pluto
  - [x] `EnsureSettingsDir()` — crea se non esiste
  - [x] `Slugify(string)` — slugify/sanitize nomi file
  - [x] `Trim(string)` — trim whitespace
  - [x] `class PipeGuard` — RAII per FILE* da popen()
  - [x] Eliminare le 25+ copie inline di getenv("HOME") in 16 file

- [x] **Creare src/text_index.h/.cpp** (completato 2026-04-30)
  - [x] `ToLower(string)` — lowercase ASCII
  - [x] `Tokenize(string)` — split in token lowercase
  - [x] `BuildIdf(doc_tokens)` — costruisce IDF table da token sets
  - [x] `ScoreDocument(fields, query, idf)` — scoring multi-campo con pesi
  - [x] `ScoreTokenSet(tokens, query, idf, weight)` — scoring singolo set
  - [x] Refactor rag.cpp per usare text_index (eliminati to_lower, tokenize, build_idf duplicati)
  - [x] Refactor wiki.cpp per usare text_index (eliminati to_lower, tokenize, build_wiki_idf, score_wiki_entry, slugify duplicati)

- [x] **Creare src/frontmatter.h/.cpp** (completato 2026-04-30)
  - [x] `ParseFrontmatter(content)` — parser generico, ritorna Frontmatter{fields, body}
  - [x] `SplitCsv(s)` — split comma-separated con trim
  - [x] `Frontmatter::get(key)`, `get_list(key)`, `has(key)` — accessori
  - [x] Refactor wiki.cpp: eliminato parse_frontmatter locale (~45 righe)
  - [x] Refactor config.cpp: semplificato parse_skill_frontmatter (~30 -> 10 righe)
  - [x] Refactor config.cpp: semplificato parse_mode_file (~30 -> 10 righe)

- [x] **Fix catch(...) silenti** (completato 2026-04-30)
  - [x] Convertiti ~40 `catch(...)` in `catch(const std::exception&)` in 15 file
  - [x] Tool execute(): error msg migliorato con e.what() (BeapiLookup, WikiLookup)
  - [x] Mantenuti intenzionalmente: config.cpp stoi (4), tool preview fallback (~15)
- [x] **Fix return value ignorati** (completato 2026-04-30)
  - [x] Verificati: ::rename gia' checkati nei punti critici (config, oauth, mcp)
  - [x] ::mkdir best-effort (EEXIST ok) — pattern standard, non richiede check
  - [x] pclose in MainWindow per metadata non critica — accettabile

---

## Fase 2 — Naming (un commit per modulo)

### 2a. Funzioni libere: snake_case -> UpperCamelCase

Ordine dal meno dipendente al piu' dipendente:

- [x] cancel.h/.cpp — `RequestCancel()`, `ClearCancel()`, `IsCancelRequested()` (completato 2026-04-30)
- [x] markdown.h/.cpp — `Feed()`, `Flush()`, `Reset()`, `_RenderLine()`, `_RenderInline()`, `fLineBuf`, `fInCodeBlock` (completato 2026-04-30)
- [x] telemetry.h/.cpp — `TelemetryRecord()`, `TelemetrySessionEnd()`, `TelemetryLoad()`, `TelemetryLoadDaily()`, `TelemetryEnabled()`, `RunStatsCli()` (completato 2026-04-30)
- [x] workspace.h/.cpp — 16 funzioni rinominate, ~25 file call site aggiornati (completato 2026-04-30)
- [x] oauth.h/.cpp — 11 funzioni rinominate, ~8 file call site aggiornati (completato 2026-04-30)
- [x] text_index.h/.cpp — gia' UpperCamelCase dalla creazione in Fase 1.2
- [x] frontmatter.h/.cpp — gia' UpperCamelCase dalla creazione in Fase 1.3
- [x] rag.h/.cpp — 10 funzioni rinominate, ~7 file call site aggiornati (completato 2026-04-30)
- [x] wiki.h/.cpp — 10 funzioni rinominate, ~5 file call site aggiornati (completato 2026-04-30)
- [x] schedule.h/.cpp — 8 funzioni rinominate (completato 2026-04-30)
- [x] routine.h/.cpp — 14 funzioni rinominate (completato 2026-04-30)
- [x] agent_store.h/.cpp — 14 funzioni rinominate (completato 2026-04-30)
- [x] config.h/.cpp — 16 funzioni rinominate + MakeProviderForConfig, ~13 file aggiornati (completato 2026-04-30)
- [x] agent/Loop.h/.cpp — RunTurn, AutoAllow, InteractivePrompt, MaybeCompact (completato 2026-04-30)
- [x] tools — 23 factory MakeXxxTool() rinominate in Builtin.h, BfsTools.h, DesktopTools.h, BeapiLookup.h, WikiLookup.h, WebFetch.h, WebSearch.h (completato 2026-04-30)
- [x] mcp/*.h/.cpp — 13 funzioni: LoadMcpSpecs, SaveMcpSpec, RemoveMcpSpec, IsProjectApproved, SetProjectApproved, DiscoverMcpServers, RunMcpCli, MakeTransport, etc. (completato 2026-04-30)
- [x] provider — nessuna funzione libera pubblica da rinominare
- [x] repl.h/.cpp — `RunRepl()` (completato 2026-04-30)
- [x] main.cpp — tutti i call site gia' aggiornati nelle fasi precedenti

### 2b. Variabili locali: snake_case -> lowerCamelCase

**Status: deferred — fare incrementalmente.** Troppo rischioso come rename globale
(campi struct, JSON keys, parametri callback). Applicare file per file quando
si tocca un file per altre ragioni.

- [ ] Applicare lowerCamelCase ai nuovi file creati
- [ ] Quando si modifica un file esistente, rinominare le variabili locali in quel file

### 2c. Costanti: SCREAMING_CASE -> kUpperCamelCase

- [x] `MAX_READ_SIZE` -> `kMaxReadSize` in ReadFile.cpp (completato 2026-04-30)
- [x] Verificati: tutte le altre costanti gia' in kPrefix (kMaxResponseBytes, kMsgWorkerTS, etc.)
- [x] Nessun `DEFAULT_TOP_K` trovato (era gia' stato rimosso)

### 2d. Globali: verificare prefisso

- [ ] `g_cancel` -> ok (gia' g-prefix)
- [ ] `g_entries` -> ok
- [ ] `g_el`, `g_hist`, `g_hev` -> ok, ma proteggere

---

## Fase 3 — Formatting

- [x] **Creare .clang-format con regole Haiku** (completato 2026-04-30)
  - [x] BreakBeforeBraces: Custom (functions new line, control same line)
  - [x] AlwaysBreakAfterReturnType: TopLevelDefinitions
  - [x] BreakConstructorInitializers: BeforeColon + PackConstructorInitializers: Never
  - [x] IncludeCategories con priorita' Haiku (own > stdlib > system > API > 3rd > local)
  - [x] SeparateDefinitionBlocks: Always (2 righe tra funzioni)
  - [x] Applicato a: util.cpp, text_index.cpp, frontmatter.cpp, cancel.cpp
  - [x] Testato: output conforme allo stile Haiku

- [ ] **Applicare clang-format al resto del codebase** (incrementale)
  - [ ] Applicare file per file con `clang-format -i --style=file <file>`
  - [ ] Commit separati: "style: apply clang-format to <file>, no functional changes"
  - [ ] Priorita': src/*.cpp core, poi tools/, poi mcp/, poi gui/ (piu' grande)

---

## Fase 4 — Refactoring architetturale

### 4a. MainWindow.cpp (5572 righe -> 4 file)

- [x] Estrarre `src/gui/SlashCommands.cpp` (completato 2026-05-01)
  - [x] Spostato `_HandleSlashCommand()` — 623 righe estratte
  - [x] MainWindow.cpp: 5572 → 4940 righe
  - [ ] TODO futuro: dispatch table al posto di if/else cascade

- [x] Estrarre `src/gui/SessionManager.cpp` (completato 2026-05-01)
  - [x] `_SaveSession()`, `_LoadSessionFromPath()`, `_RebuildSessionsMenu()`
  - [x] `_SaveWorkspace()`, `_RestoreWorkspace()`
  - [x] `_ExportMarkdown()`, `_ExportJsonl()`
  - [x] 197 righe estratte

- [ ] Estrarre `src/gui/InputManager.cpp` (deferred)
  - Metodi troppo intrecciati con side panel e IPC
  - Fare incrementalmente in futuro

- [x] MainWindow.cpp: 5572 → 4725 righe (−15%)

### 4b. repl.cpp (deferred)

Spezzare run_repl() incrementalmente quando si aggiungono nuovi slash commands.

### 4c. CLI handlers (deferred)

Spezzare run_wiki_cli() e run_agent_store_cli() quando si aggiungono subcommand.

### 4d. Thread safety (deferred)

EditLine globals sono single-thread safe nella pratica. Incapsulare se si
aggiunge multi-threading al REPL.

---

## Fase 5 — Modernizzazione (ongoing, bassa priorita')

- [x] `std::optional<T>` — policy: solo per nuove API, non retrofitting (completato 2026-05-01)
- [x] `std::string_view` — policy: solo dove non serve .c_str() (completato 2026-05-01)
- [x] `PipeGuard` — disponibile, usare in nuovo codice, retrofit incrementale (completato 2026-05-01)
- [x] Unit test per text_index (8 test), frontmatter (7 test), util (9 test) (completato 2026-05-01)
- [ ] Benchmark tokenizer / IDF scorer (future)

---

## Regole per i commit

- Un commit per modulo/file toccato
- Commit di sola formattazione: "style: <cosa> in <file>, no functional changes"
- Commit di rename: "rename: <old> -> <new> in <modulo>"
- Commit di refactor: "refactor: extract <cosa> from <dove>"
- Commit di fix: "fix: <cosa> in <file>"
- Mai mischiare cambi funzionali con cambi di stile nello stesso commit

---

## File da NON toccare

- `src/gui/textview/*` — libreria importata, gia' conforme
- `src/gui/ColumnListView.cpp` — widget Haiku standard
- `src/gui/ColumnTypes.cpp` — widget Haiku standard
