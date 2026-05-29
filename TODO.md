# Pluto — TODO

> **Vision**: Pluto deve essere il client Claude più piacevole su Haiku **e**
> migliore di Claude Code in tutto ciò che Claude Code non può fare perché è
> cross-platform. Le tre leve sono: (1) parità funzionale sulle feature core,
> (2) integrazione nativa Haiku impossibile altrove (Tracker, BFS, Deskbar,
> scripting BMessage, ui_color), (3) polish e UX da app nativa vera.

## Stato (2026-05-02)

- **~37000 LOC** in `src/`, 170+ file (.cpp + .h), 3 binari (`Pluto_cli` + `Pluto` GUI + `Ask pluto` add-on)
- 23 tool built-in (read_file, write_file, edit_file, apply_patch, bash, grep, glob, todo_write, read_attr, write_attr, list_attrs, remove_attr, query_fs, live_query, clipboard, node_monitor, desktop_script, launch_app, beapi_lookup, wiki_lookup, web_fetch, web_search, lsp)
- Wiki LLM knowledge base con ingest da URL/file
- First-run wizard con 7 provider
- Code cleanup completato (Fase 1-5): naming UpperCamelCase, moduli condivisi, .clang-format
- 21 test runner in `tests/`
- 6 documenti in `docs/`
- 4 agenti di esempio in `examples/agents/`
- Custom modes: plan (built-in) + user-defined in `~/config/settings/pluto/modes/*.md`
- RAG locale: 390 snippet BeAPI in `data/beapi-index.jsonl`, 202 classi Be
- Multi-istanza safe: `ScopedFileLock` su tutti i file condivisi
- IPC tra istanze: `PlutoMessenger` + scripting suite + `/peers` REPL command
- REPL con libedit: frecce, emacs keybindings, history persistente (500 entry), Tab completion slash commands
- `.PackageInfo` v0.3.0-1 con `make package`
- Build: `make` (Pluto_cli 1357KB), `make Pluto` (GUI 2457KB), `make check` (21 test), `make bench`, `make ci`
- Header dep tracking via `-MMD -MP` attivo
- Versione centralizzata in `src/version.h` (`PLUTO_VERSION`, `PLUTO_USER_AGENT`)
- Benchmark suite (3 run): JSON 6-1296µs, RAG 13.5ms, session save 3.6ms, load 1ms
- Audit pre-pubblicazione: **3 passate complete** (P4.5 + P4.6 + P4.7), 30+ issue classificati
- **P4.7 fix completati**: 21/21 issue risolti (ALL DONE) (3 CRITICAL, 4 HIGH, 4 MEDIUM, 5 LOW, 2 STYLE)
- **Item aperti**: nessuno — tutti gli issue P4.7 risolti

---

## P0 — Bloccanti per un 1.0 spedibile

- [x] **Icona HVIF** — `pluto.hvif` (848 byte), applicata come attributo BFS + risorsa embedded
- [x] **`.PackageInfo`** con dipendenze libcurl/libbe/libcrypto/libssl
- [x] **`make package`** target che lay out un pkgroot/ e chiama `package create`
- [x] **Resource section** — `pluto.rdef` con app-signature, versione 0.3.0, icona VICN. `rc` + `xres` + `addattr` automatizzati nel Makefile dopo ogni link
- [x] **`CHANGELOG.md`** con la storia per categoria
- [x] **Man page `docs/pluto.1`** — opzioni, REPL commands, sub-command, files, env
- [x] **Recipe HaikuPorter** — `pluto-0.3.0.recipe` con BUILD/INSTALL/TEST,
      installa CLI + GUI + Tracker add-on + man page + docs. Richiede
      solo URL upstream pubblico per CHECKSUM_SHA256
- [x] **Test integrazione `SessionStore`** — `tests/test_session.cpp` con `set_dir_override` su tmpdir, copre save/load/list/find/tag/remove
- [x] **Test integrazione tool** — `tests/test_tools.cpp` su tmpdir, copre Write→Read, Edit (incluso vincolo unicità), Bash con cwd
- [x] **`McpManager`** in `src/mcp/Manager.{h,cpp}`, dedup CLI e GUI
- [x] **Eliminato stub `OpenAICompatProvider`** (file rimossi, branch in `make_provider` aggiornato)

---

## P1 — Differenziazione che batte Claude Code

Cose che **solo Pluto** può fare perché vive su Haiku. Ognuna è una storia
da raccontare alla community con un GIF di 10 secondi.

- [x] **Tool `desktop_script`** — manda `BMessage` scripting ad altre app Haiku.
      Azioni: get/set/count/create/delete/execute/suites. Specifier per nome
      o indice. Format reply ricorsivo con tutti i tipi Be. Test su Tracker live
      (6 test case in `tests/test_desktop_script.cpp`)
- [x] **Tracker add-on "Ask Pluto"** — entry nel menu contestuale di Tracker.
      `src/haiku/TrackerAddOn.cpp`, compilato come `.so` con `make Ask\ Pluto`.
      `process_refs()` raccoglie i file selezionati, lancia Terminal con
      `pluto --one-shot` e `@path` per ogni file. `make install-addon` /
      `make uninstall-addon` per deployment in
      `~/config/non-packaged/add-ons/Tracker/`. Test: `tests/test_tracker_addon.cpp`
      (verifica load_add_on + export process_refs symbol)
- [x] **Deskbar replicant** — `src/gui/DeskbarReplicant.{h,cpp}`. Archivable
      `BView` 16x16 con HVIF icon da risorse. Click → `be_roster->Launch` per
      portare Pluto-gui in primo piano. `InstallDeskbarReplicant()` /
      `RemoveDeskbarReplicant()` via `BDeskbar::AddItem/RemoveItem`. Menu GUI:
      Help → Install/Remove Deskbar icon
- [ ] **Replicant input bar** — barra prompt droppabile nello shelf. Più complesso,
      richiede shelf integration completa
- [x] **Theming live via `ui_color()`** — palette `kCol*` ricomputata in
      `_InitPalette()` da `ui_color(B_DOCUMENT_*_COLOR)`. Auto-detect
      dark/light theme via luminanza del background, brighten/darken adattivi.
      View colors via `SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR)`. Live
      update su `B_COLORS_UPDATED` nel `MessageReceived` → `_InitPalette()` +
      Invalidate views
- [x] **Tool `node_monitor`** in `DesktopTools.cpp` — `BNodeMonitor` +
      `BLooper` listener, watch di una directory per durata configurabile,
      ritorna add/remove/move/stat/attr changes
- [x] **Tool `clipboard`** in `DesktopTools.cpp` — `BClipboard` get/set di
      testo plain. Supporta `action=get` e `action=set` + `text`
- [x] **`notification_server` integration** — `BNotification` su turn complete
      quando finestra non attiva, click → focus

---

## P1.4 — RAG locale per BeAPI (knowledge retrieval)

Pluto conosce la BeAPI meglio di qualsiasi altro client AI grazie a un
dataset locale di 390 snippet (~1944 righe JSONL, 5 varianti per topic).
Invece di iniettare tutto nel prompt, retrieval on-demand dei 2-3
snippet più rilevanti alla domanda. Risultato: codice Haiku-native
corretto al primo colpo, con pattern idiomatici (LayoutBuilder, BMessage
protocol, BFS attr). Solo Pluto può farlo — differenziazione netta.

**Sorgente dati:** `dataset_v3.jsonl` (390 topic unici, classi coperte:
BView, BWindow, BMessage, BApplication, BColumnListView, BFilePanel,
BFont, BRoster, BListView, BBitmap, BUrlRequest, BMediaRoster,
BTranslatorRoster, BTextView, BMenu, BString, BFile, BDirectory, ...)

### Fase K1 — Indice e motore di ricerca

- [x] **Dedup dataset** — script Python estrae i 390 snippet unici da
      `dataset_v3.jsonl`, produce `data/beapi-index.jsonl` (~500 KB)
      con campi `topic`, `question`, `answer`, `classes` (202 classi
      Be estratte automaticamente). Shippato col pacchetto via Makefile
- [x] **`src/rag.{h,cpp}`** — motore di retrieval locale:
      `rag_init(path)` carica indice in memoria (~390 entry),
      `rag_search(query, top_k)` con scoring composito: class match
      (peso 5x IDF) + topic keyword (3x) + question keyword (1x).
      `rag_extract_classes()` estrae nomi Be da testo. Zero dipendenze
      esterne, ~1ms per query, ~200 KB in memoria
- [x] **Keyword extraction** — `rag_extract_classes()` rileva pattern
      `B[A-Z][A-Za-z]+` con word boundary check e filtro min 3 char.
      Tokenizer generico per keyword split + lowercase
- [x] **Test** — `tests/test_rag.cpp` (10 test): init/bad path, search
      by class/keyword/multiple classes, ranking, top_k, no match,
      extract classes, no false positives. Testato anche su dataset
      reale (390 entry, ranking verificato manualmente)

### Fase K2 — Integrazione nel loop

- [x] **Auto-inject in `Loop::run_turn`** — `Loop.cpp` rileva classi Be
      nel ultimo messaggio utente via `rag_extract_classes`, esegue
      `rag_search` e inietta `<beapi_reference>` nel system prompt
      della request (max 3 snippet). Skip se nessun match
- [x] **Tool `beapi_lookup`** — `src/tools/BeapiLookup.{h,cpp}`, tool
      esplicito che il modello può chiamare. Parametri: `query` (string,
      required), `top_k` (int, 1-5, default 3). Registrato in CLI e GUI
      solo se l'indice è caricato (`rag_entry_count() > 0`)
- [x] **Indicatore nel prompt** — `apply_rag_hint()` in `config.cpp`
      aggiunge `<beapi_hint>` al system prompt con conteggio entry e
      istruzioni per usare `beapi_lookup`. Chiamato dopo `rag_init`
- [x] **CLI: `pluto beapi <query>`** — `run_beapi_cli()` in `rag.cpp`.
      Cerca nel dataset, mostra top-5 con topic, score, classi e
      snippet completo. Cerca l'indice in `./data/`, `~/config/settings/pluto/`,
      `/boot/system/data/pluto/`

### Fase K3 — Miglioramenti

- [x] **BFS index con BQuery** — `rag_export_bfs()` esporta ogni entry
      come file in `~/config/settings/pluto/beapi/` con attributi BFS
      `beapi:topic` e `beapi:classes` (comma-separated). Cercabile via
      Tracker Find o `query_fs`. CLI: `pluto beapi export`
- [x] **GUI: pannello BeAPI reference** — `kMsgWorkerRag` in
      `MainWindow.cpp`. Quando il loop inietta snippet RAG, il side
      panel mostra "BeAPI reference" con lista topic e una riga dim
      `[beapi] injected: ...` nel chat. `on_rag_inject` callback in
      `UiCallbacks` (Loop.h)
- [x] **Statistiche** — `pluto beapi stats` mostra il numero di entry
      caricate (indice principale + custom)
- [x] **Contribuzione** — `rag_load_custom()` carica snippet da
      `~/config/settings/pluto/beapi-custom.jsonl`. Formato JSONL
      identico all'indice principale (topic, question, answer, classes).
      Se `classes` manca, auto-extract da `answer`. Merge automatico
      all'avvio in CLI e GUI

---

## P1.5 — Workspace scoping e permessi directory

Equivalente di come Claude Code chiede "trust this project directory?".
Pluto deve sapere su quali cartelle può operare e chiedere prima di
toccare file fuori dal perimetro autorizzato.

- [x] **Workspace config** — `src/workspace.{h,cpp}`: `workspace_init()` cattura
      cwd allo startup, `workspace_allows(path)` verifica se dentro workspace o
      directory trusted. Path canonicalization con risoluzione symlink Haiku
      (`/tmp` → `/boot/system/cache/tmp`). Test override per isolamento
- [x] **Prima esecuzione** — prompt CLI in `repl.cpp` dopo il banner:
      "⚠ First time in <path>. Authorize? [y]once [a]always [n]o".
      `y` = session trust (in-memory), `a` = persistente in
      `~/config/settings/pluto/trusted_workspaces`, `n` = tool erroreranno.
      Skip in `--yolo` mode
- [x] **Enforcement nei tool** — `read_file`, `write_file`, `edit_file`,
      `bash` (cwd check), `read_attr`, `write_attr`, `list_attrs`, `remove_attr`
      tutti controllano `workspace_allows()` prima di eseguire. Se fuori
      workspace → errore con messaggio "use `pluto trust add <dir>`"
- [x] **`pluto trust`** sub-command — `list`, `add <path>`, `remove <path>`,
      `log` (ultimi 50 righe di access.log). Implementato in `main.cpp`
- [x] **Bash sandbox avanzato** — `cd <workspace> &&` prefix automatico,
      comando eseguito sempre dalla root del workspace. cwd check mantenuto
- [x] **GUI workspace indicator** — [trusted]/[untrusted] + nome cartella
      nella status bar, aggiornato ad ogni `_UpdateStatus()`. ASCII symbols
      per compatibilità (sostituiti Unicode ✓/⚠)
- [x] **Integrazione con Tracker** — quando un file droppato è fuori
      workspace, `BAlert` offre di trustare il parent (No/This session/Always).
      Integrato in `B_REFS_RECEIVED` di MainWindow
- [x] **Log accessi** — ogni allow/deny loggato in
      `~/config/settings/pluto/access.log` con timestamp, path, azione.
      Consultabile via `pluto trust log`
- [x] **Test workspace** — `tests/test_workspace.cpp` con 6 test case:
      allows-inside, denies-outside, trust/untrust persistence,
      no-duplicate trust, access logging, session trust

---

## P1.7 — Multi-istanza e IPC nativo

Più istanze di Pluto in esecuzione contemporaneamente, con comunicazione
tra loro sfruttando le primitive native Haiku (`BMessenger`, `BRoster`,
scripting protocol). Nessun altro client AI può farlo — è una storia
Haiku pura.

### Fase A — Concorrenza safe tra istanze

Prerequisito: rendere innocuo avere N processi `pluto` (CLI o GUI)
attivi sullo stesso sistema senza corruzione di file condivisi.

- [x] **File locking utility** — `src/filelock.h` header-only con
      `ScopedFileLock` basato su `flock(2)`. RAII: lock acquisito nel
      costruttore, rilasciato nel distruttore. Timeout opzionale con
      retry 10ms (default 2s)
- [x] **Config write atomico** — `save_config()` in `config.cpp`:
      scrive su `config.tmp.<pid>` + `rename()` (atomic). Protetto con
      `ScopedFileLock` sul file `.lock` accanto
- [x] **OAuth token write atomico** — `save_oauth_tokens()` in
      `oauth.cpp`: `ScopedFileLock` + nome tmp con PID
      (`oauth-tokens.tmp.<pid>`)
- [x] **API key write atomico** — `save_api_key()` in `config.cpp`:
      tmp+rename+lock come sopra
- [x] **Trusted workspaces lock** — `trust_workspace()` e
      `untrust_workspace()` in `workspace.cpp`: wrappati in
      `ScopedFileLock`. `untrust` fa read-filter-rewrite sotto lock
      (fix TOCTOU). `trust` ricontrolla duplicati sotto lock
- [x] **Telemetria append lock** — `telemetry_record()` e
      `telemetry_session_end()` in `telemetry.cpp`: `ScopedFileLock`
      con timeout 500ms
- [x] **Access log append lock** — `log_access()` in `workspace.cpp`:
      `ScopedFileLock` con timeout 500ms
- [x] **Session filename con random** — `SessionStore::save()`:
      suffisso random 4 hex (`mt19937` + `thread_local`) nel nome file
      per evitare collisione tra istanze concorrenti
- [x] **Semaforo approval con PID** — `MainWindow.cpp`: cambiato
      `create_sem(0, "pluto-approval")` in
      `create_sem(0, "pluto-approval-<pid>")` con `getpid()`. Ogni
      istanza GUI ha il proprio semaforo

### Fase B — Multi-istanza GUI nativa

Ogni lancio crea un processo indipendente. Implementato.

- [x] **`B_MULTIPLE_LAUNCH`** — `pluto.rdef` cambiato da
      `B_SINGLE_LAUNCH` a `B_MULTIPLE_LAUNCH`. Il sistema crea un
      nuovo processo ad ogni doppio click o lancio da Ask Pluto
- [x] **Signature unica per istanza** — `pick_signature()` in
      `App.cpp`: la prima istanza usa la signature base
      `application/x-vnd.Hay-pluto`, ogni successiva usa
      `application/x-vnd.Hay-pluto-<pid>` (necessario per
      BApplication, che richiede unicità per processo)
- [x] **Tab forwarding** — quando una seconda istanza GUI viene lanciata,
      rileva la prima via `BRoster` e inoltra come nuovo tab via
      `kMsgOpenNewTab`. La seconda istanza si chiude dopo il forwarding.
      Implementato in `App.cpp` (ReadyToRun) e `MainWindow.cpp` (handler).
      Fallback: se il forwarding fallisce, apre normalmente

### Fase C — IPC tra istanze (`PlutoMessenger`)

Protocollo `BMessage` per comunicazione tra istanze Pluto.
Ogni istanza si registra come scripting-aware. Le altre possono
scoprirla via `BRoster` e mandarle comandi.

- [x] **`src/ipc/PlutoMessenger.{h,cpp}`** — API per trovare e
      parlare con altre istanze:
      - `find_peers()` → lista di `PeerInfo` (team_id, workspace,
        model, session_title, busy) via `BRoster::GetAppList` +
        scripting queries per ogni proprietà
      - `send_to(team_id, what, data)` → invia messaggio a un peer
      - `broadcast(what, data)` → invia a tutti i peer (escluso self)
      - `query(team_id, what, data, timeout)` → sincrono con timeout
      Test: `tests/test_ipc.cpp` (7 test: defaults, find_peers, send
      invalid, query invalid, broadcast, msg constants, fields)
- [x] **Scripting suite in `App.cpp`** — espone proprietà via
      il protocollo scripting BeOS standard:
      - `Workspace` (string, read) — directory di lavoro corrente
      - `SessionTitle` (string, read) — titolo sessione attiva
      - `Model` (string, read) — modello in uso
      - `Busy` (bool, read) — se sta elaborando un turno
      Implementato con `property_info`, `GetSupportedSuites`,
      `ResolveSpecifier`, handler `B_GET_PROPERTY` in `MessageReceived`.
      Testabile da terminale con `hey Pluto get Workspace`
- [x] **Comando REPL `/peers`** — lista le istanze Pluto GUI attive
      con PID, workspace, modello, stato (idle/busy), titolo sessione.
      Usa `ipc::find_peers()` + scripting queries. Tabella formattata
- [x] **Comando REPL `/ask <peer> <prompt>`** — manda un prompt
      a un'altra istanza GUI. Il peer lo inietta nella conversazione
      corrente come se l'utente lo avesse digitato. Identificazione
      peer per PID o sottostringa workspace. `ipc::find_peer()` in
      `PlutoMessenger.cpp`, handler in `repl.cpp` e `MainWindow.cpp`
- [x] **Comando REPL `/delegate <peer> <prompt>`** — come `/ask`
      ma fire-and-forget: il peer esegue e notifica con
      `BNotification` quando ha finito (via il sistema notifiche
      esistente che scatta quando la finestra non è attiva)
- [x] **GUI: menu MCP > Peer instances** — voce menu "Peer
      instances…" sotto MCP. Click apre `BAlert` con lista peer
      attivi (PID, workspace, model, stato, sessione). Usa
      `ipc::find_peers()` da `PlutoMessenger.cpp`. Handler
      `kMsgPeerMenu` in `MainWindow::MessageReceived`
- [x] **Workspace collision detection** — quando una nuova istanza
      CLI si avvia su una directory già in uso da un peer GUI, avvisa:
      "⚠ Another Pluto instance (PID X) is already working in <path>".
      Implementato in `repl.cpp` via `ipc::check_workspace_collision()`.
      Test: `test_ipc.cpp` (2 test case: empty path, unique path)
- [x] **Context sharing** — un'istanza può chiedere a un peer
      "cosa sai di \<topic\>?" via scripting. Proprietà "Context" aggiunta
      alla scripting suite in `App.cpp`. `SearchContext()` in `MainWindow.cpp`
      cerca case-insensitive in tutti i tab e ritorna fino a ~2000 char di
      estratti rilevanti. `query_context()` in `PlutoMessenger.cpp` per
      il lato client. Comando REPL `/context <peer> <topic>` per query
      interattiva
- [x] **Test IPC** — `tests/test_ipc.cpp`: 13 test case coprono
      PeerInfo default, find_peers, send_to invalid, query invalid,
      broadcast, message constants (inclusi kMsgPeerAsk/kMsgPeerDelegate),
      PeerInfo fields, find_peer not found, find_peer numeric,
      check_workspace_collision empty/unique, send ask/delegate invalid

---

## P2 — Parità con Claude Code (quello che oggi facciamo peggio)

Feature che Claude Code ha e che ci servono per non sembrare un giocattolo.

### Memoria progetto e personalizzazione
- [x] **`pluto.md` auto-load** — `apply_project_memory()` in `config.cpp` legge `~/config/settings/pluto/pluto.md` e `./pluto.md`, append a `cfg.system_prompt` con tag `<user_memory>`/`<project_memory>`. Chiamato da CLI e GUI dopo `load_config`
- [x] **`/init` command** — `/init [--force]` crea `pluto.md` nella workspace root. Auto-detect: git remote, linguaggio principale (per estensione file), build system (Makefile, CMakeLists.txt, Cargo.toml, etc.). Template con `## Instructions` per istruzioni progetto. Ricarica project memory nella sessione corrente. Implementazione condivisa in `workspace.{h,cpp}` (`init_project_memory()`), disponibile sia in CLI che GUI con autocomplete
- [x] **Slash command custom** — `load_custom_commands()` in `repl.cpp` legge `~/config/settings/pluto/commands/*.md` con frontmatter opzionale `description:`. Substitution `{{args}}` nel body. `/commands` lista. Provati prima di trattare la riga come user message
- [x] **Skill system** (sempre attive) — `apply_skills()` legge `~/config/settings/pluto/skills/<nome>/SKILL.md`, append a `cfg.system_prompt` sotto `<skills>` con `<skill name="...">` per ognuna. Trigger-based loading lasciato a roadmap
- [x] **Hook system** — `hooks_pre_tool` / `hooks_post_tool` come `std::vector<std::string>` in Config, parse da `hook_pre_tool=` / `hook_post_tool=` ripetuti, eseguiti in `Loop.cpp` attorno a `tool->execute` con `PLUTO_TOOL=<name>` in env
- [x] **Hook `user_prompt_submit`** — `hooks_user_prompt` in Config, parsato da
      `hook_user_prompt=` (ripetibile). Eseguito in `repl.cpp` e in
      `MainWindow::_SendCurrentInput()` prima del worker, con
      `PLUTO_PROMPT=<testo>` in env
- [x] **Trigger-based skill loading** — SKILL.md supporta frontmatter
      `trigger: keyword1, keyword2`. Skill senza trigger sono sempre attive
      (backward compat). Skill con trigger sono listate in `<available_skills>`
      nel system prompt e iniettate on-demand quando il messaggio utente
      contiene un keyword (case-insensitive). `inject_triggered_skills()` in
      `config.cpp`, chiamata in `repl.cpp` prima di ogni `run_turn`.
      Test: `tests/test_skills.cpp` (6 test)

### Esecuzione non interattiva
- [x] **`pluto agent run <file.md>`** — sub-command che legge il file, lo usa come `one_shot_prompt`, attiva `yolo`, lascia parsare gli altri flag normalmente
- [x] **`pluto schedule`** — wrapper su `launch_daemon` per task ricorrenti.
      Sub-commands: `list`, `add <name> --file <f> [--event boot|login]
      [--interval <mins>] [--model <id>]`, `remove`, `enable`, `disable`,
      `run`. Config in `~/config/settings/pluto/schedules/<name>.conf`.
      Periodic: genera script loop + launch_daemon job. Event-based: job
      diretto. Test: `tests/test_schedule.cpp` (save/load/list/remove/job)
- [x] **`--dry-run`** CLI flag + `dry_run` config + early-return in `Loop::run_turn` con preview testuale (zero API call)

### Routines — workflow ricorrenti e trigger-based
- [x] **Routine engine** — `src/routine.{h,cpp}`. File `.md` in
      `~/config/settings/pluto/routines/` con frontmatter YAML: `name`,
      `schedule` (manual/on_boot/on_file_change), `model`, `tools`,
      `notify`, `enabled`. Template expansion: `{{date}}`, `{{clipboard}}`,
      `{{changed_files}}`. Test: `tests/test_routine.cpp` (6 test)
- [x] **`pluto routine list`** — elenca routines con stato/schedule/model
- [x] **`pluto routine run <name>`** — espande template, esegue via
      `pluto agent run` con il modello configurato
- [x] **`pluto routine enable/disable <name>`** — riscrive frontmatter
- [x] **`pluto routine log <name>`** — ultime 50 righe dal log file
- [x] **Trigger `schedule`** — `install_routine_job()` in `routine.cpp`
      genera script con loop + `launch_daemon` job per routines con
      schedule `every Nm` o `every Nh`. Es: `schedule: every 30m`.
      CLI: `pluto routine install/uninstall <name>`
- [x] **Trigger `on_file_change`** — polling watcher script generato da
      `install_routine_job()` per schedule `on_file_change <dir>`. Controlla
      mtime ogni 30s, fire `pluto routine run` quando rileva cambiamenti.
      Lista file cambiati salvata in `/tmp/pluto-routine-changed-<name>`
- [x] **Trigger `on_boot`** — `install_routine_job()` genera job
      `launch_daemon` con `on initial_volumes_mounted` per routines con
      `schedule: on_boot`. `set_routine_enabled()` installa/rimuove
      automaticamente. Test: 4 nuovi test in `test_routine.cpp` (10 totali)
- [x] **GUI Routines panel** — `File > Routines…` apre `RoutineDialog`
      con lista routines (active/paused), dettagli (schedule, model,
      tools, notify). Bottoni Run now / Enable-Disable / Show log.
      Run apre Terminal + tee nel log. Log view monospace integrata.
      `src/gui/RoutineDialog.{h,cpp}`
- [x] **Notifica risultato** — `BNotification` inviata quando una
      routine con `notify: true` viene lanciata dalla GUI
- [x] **Routine di esempio** — 3 routines in `examples/routines/`:
      `daily-summary.md` (riassunto file modificati, manual),
      `code-review-on-save.md` (review su file change, disabled di default),
      `project-status.md` (health report con build/test/git/todo count)

### Agent Store — installazione agenti da repository
- [x] **Registry format** — repository remoto (Git o HTTPS) con un
      `index.json` che elenca agenti disponibili. Ogni agente è una
      directory con `AGENT.md` (prompt + frontmatter: name, description,
      author, version, tools, routines, skills), opzionalmente `icon.hvif`,
      e file di supporto (skill, routine, command `.md`)
- [x] **`pluto agent install <url|name>`** — scarica un agente da un
      repository, lo installa in `~/config/settings/pluto/agents/<name>/`,
      registra le sue skill/routine/command automaticamente. Supporta:
      URL Git diretto, nome breve da registry configurato, path locale.
      `src/agent_store.{h,cpp}`, parse frontmatter YAML, `copy_dir`
      ricorsivo, `install_agent_from_url` con `git clone --depth=1`
- [x] **`pluto agent remove <name>`** — rimuove agente + skill/routine
      associate. Unregister automatico risorse bundled
- [x] **`pluto agent list`** — elenca agenti installati con versione,
      descrizione. Ordinati alfabeticamente
- [x] **`pluto agent info <name>`** — mostra dettagli agente: nome,
      versione, autore, descrizione, tools, skills, routines, commands,
      e il prompt body
- [x] **`pluto agent update [name|--all]`** — `update_agent()` clona il
      repo sorgente, confronta versioni, reinstalla se più recente.
      `.agent-source` traccia l'URL di installazione. CLI: `pluto agent
      update <name>` o `--all`. Test: 4 nuovi test in test_agent_store.cpp
- [x] **`pluto agent search <query>`** — cerca agenti installati e
      d'esempio per nome, descrizione, autore e tool (case-insensitive).
      CLI: `pluto agent search <query>`. GUI: campo di ricerca live
      nella AgentStoreDialog che filtra la lista in tempo reale.
      Test: 1 test case con 5 check in test_agent_store.cpp
- [x] **Registry di default** — `kDefaultRegistryUrl` in `agent_store.cpp`
      punta a `https://codeberg.org/atomozero/pluto-agents`.
      `install_agent_from_registry()` clona il repo, cerca in `agents/<name>/`
      o `<name>/`, installa e salva source URL per update.
      CLI: `pluto agent install <name>` — se non e' URL ne' path locale,
      risolve automaticamente dal registry di default.
      Struttura attesa: `agents/<name>/AGENT.md` + file di supporto
- [x] **GUI Agent Store** — `File > Agents…` apre `AgentStoreDialog`
      (`src/gui/AgentStoreDialog.{h,cpp}`) con `BListView` agenti
      installati, pannello dettaglio (autore, versione, tools, skills,
      routines, commands), pannello prompt body (monospace). Bottoni:
      Install… (apre `BFilePanel` nativo per selezionare directory),
      Remove (con `BAlert` di conferma), Show prompt. Supporta anche
      drag&drop di directory agente sulla finestra. Integrato nel menu
      File dopo Routines…
- [x] **Dipendenze tra agenti** — un agente può richiedere skill o
      tool specifici (es. `requires: [read_attr, query_fs]`). Install
      verifica che i tool siano disponibili e avvisa se mancanti.
      `check_agent_deps()` ritorna lista tool mancanti
- [x] **Sicurezza** — `preview_agent()` mostra nome, versione, autore,
      tool e prompt prima dell'installazione. CLI: conferma interattiva
      `[y/N]`, flag `--trust` per skip. GUI: `BAlert` con review del
      prompt (primi 500 char) prima di `Install`. Test: 2 test per
      preview_agent (valid + missing)
- [x] **Agenti di esempio da shippare** — 4 agenti built-in in
      `examples/agents/`:
      `haiku-dev` (conosce Be API, BFS, suggerisce codice nativo),
      `code-reviewer` (review strutturata con checklist e severity),
      `shell-assistant` (aiuto comandi Terminal Haiku, pkgman, hey),
      `project-onboarder` (analizza un repo e genera pluto.md).
      Test: `tests/test_agent_store.cpp` (9 test)

### Autenticazione
- [x] **OAuth 2.0 + PKCE per Claude Max/Pro** — `src/oauth.{h,cpp}`,
      `pluto login` / `pluto logout`, refresh automatico, token storage
      in `~/config/settings/pluto/oauth-tokens` (chmod 600). PKCE via
      OpenSSL. `Credentials` struct con priorità OAuth > API key.
      `AnthropicProvider` supporta `AuthType::OAUTH` con header
      `Authorization: Bearer` + `anthropic-beta: oauth-2025-04-20`.
      Test: `test_oauth.cpp` (11 test)

### Provider e modelli
- [x] **`openai-compat` provider completo** — `src/provider/OpenAICompat.{h,cpp}`,
      ~320 LOC. Request body builder con mapping ContentBlock→OpenAI format
      (system as message, tool_calls array, tool role). SSE parser con
      delta.content → TEXT_DELTA, delta.tool_calls → TOOL_USE_START/INPUT/END,
      finish_reason → MESSAGE_END, usage parsing, [DONE] sentinel. Retry con
      backoff. Cancel via xferinfo. Supporta Ollama (`http://localhost:11434`),
      OpenRouter (`https://openrouter.ai/api`), LM Studio (`http://localhost:1234`),
      qualsiasi server OpenAI-compat. Config: `provider = openai-compat`,
      `base_url = http://...`, `model = llama3.1`, `api_key_env = OPENAI_API_KEY`
      (o vuoto per Ollama locale). TLS skip automatico per URL http://
- [x] **Model picker** in `SettingsDialog` — `BMenuField` con preset (Opus 4.6 1M / Opus 4.6 / Opus 4.5 / Sonnet 4.6 / Haiku 4.5), click autofilla la BTextControl. Custom rimane editabile

### Provider compatibility (audit 2026-04-23)

Risultati test su tutti i provider configurati. Chiavi disponibili:
OpenAI, Gemini, OpenRouter. Mancanti: Anthropic, xAI, Groq, DeepSeek.

- [x] **Bug o-series (o3, o3-mini, o4-mini)** — `OpenAICompat.cpp`:
      aggiunta `is_reasoning_model()` che rileva modelli o1/o3/o4 (anche
      con prefisso vendor come `openai/o3-mini`). `build_oai_body()` ora
      usa `max_completion_tokens` al posto di `max_tokens` per reasoning
      models, e omette `stream_options` (non supportato da o-series)
- [x] **OpenRouter HTTP 402** — causa: mancavano gli header `HTTP-Referer`
      e `X-Title` richiesti da OpenRouter. Aggiunti in `OpenAICompat.cpp`.
      Il 402 sui modelli a pagamento e' credito esaurito sull'account.
      Fix aggiuntiva: profili OpenRouter riorganizzati con 4 modelli
      gratuiti in cima (Gemma 3 27B/12B, Llama 3.3 70B, Gemma 4 31B
      col suffisso `:free`) + 4 paid. Rimosso Grok 3 (duplicato con
      profilo xAI dedicato). Verificato con curl: Gemma 3 12B free → 200 OK
- [x] **OpenRouter model ID stale** — `SettingsDialog.cpp`:
      `mistralai/mistral-large-latest` aggiornato a
      `mistralai/mistral-large-2411` (ID corrente su OpenRouter)
- [x] **Gemini quota free tier** — `SettingsDialog.cpp`: riordinati
      modelli con `gemini-2.5-flash` in cima marcato "(free tier OK)",
      gli altri marcati "(paid)". Commento inline documenta i limiti
      del piano gratuito (HTTP 429 frequente su pro/2.0-flash)
- [x] **Anthropic: nessuna credenziale** — `main.cpp`: messaggio di
      errore riscritto con guida passo-passo differenziata per provider.
      Anthropic: mostra 3 opzioni (pluto login / export env / GUI dialog).
      OpenAI-compat: mostra export env + GUI + nota che server locali non
      richiedono chiave
- [x] **Provider Cerebras** — `SettingsDialog.cpp`: aggiunto profilo
      Cerebras in kProfiles. Base URL `https://api.cerebras.ai/v1`,
      chiave `CEREBRAS_API_KEY`. Modelli: Llama 3.3 70B, Llama 3.1 8B,
      DeepSeek R1 Distill Llama 70B. Usa `openai-compat` provider,
      zero codice nuovo — solo entry in kProfiles

Modelli verificati funzionanti (2026-04-23):
- OpenAI: gpt-4.1, gpt-4.1-mini, gpt-4.1-nano, gpt-4o, gpt-4o-mini
- Gemini: gemini-2.5-flash
- Cerebras: (da verificare con chiave CEREBRAS_API_KEY)

### REPL line editing e command history
- [x] **Command history con `libedit`** — `read_line()` in `repl.cpp` usa
      `el_gets()` da libedit (BSD editline). Fornisce: frecce su/giù per
      navigare la cronologia comandi, editing di linea (Home/End/Ctrl-A/E/K/W),
      history persistente su file (`~/config/settings/pluto/history`, 500 entry).
      Emacs keybindings di default. Fallback graceful a `fgets` se init fallisce.
      Link `-ledit` nel Makefile, `lib:libedit` in `.PackageInfo`.
      Test: `tests/test_history.cpp` (7 test: init, walk, save/load roundtrip,
      size limit, missing file, editline init, special chars)

### Input e contesto
- [x] **Image input (vision)** — `@image.png` in REPL crea blocco `IMAGE`
      con base64. Supporta PNG/JPEG/GIF/WebP. `build_user_content()` in
      `repl.cpp` separa text e image blocks. Serializzazione Anthropic
      (`image` + `source.base64`) e OpenAI-compat (`image_url` data URI).
      Session save/load preserva IMAGE blocks. Limite 20 MB per file.
      Test: `tests/test_image.cpp` (11 test: base64, content block, roundtrip)
- [x] **`@file.md`** mention nell'input — `expand_file_mentions()` in
      `repl.cpp` sostituisce ogni `@<path>` whitespace-bounded con
      `[file: path]\n<contenuto>\n[/file]`. File inesistenti lasciati intatti

### Tool aggiuntivi
- [x] **Tool `todo_write`** — `src/tools/TodoWrite.cpp`. L'agente può
      gestire una task list in-memory per tracciare il progresso su lavori
      multi-step. Ogni item ha id, content, status (pending/in_progress/
      completed), priority (0-2). Replace semantics: ogni chiamata passa
      la lista completa. Preview mostra progresso (`3/7 completed`).
      Output con checkbox (`[ ]`, `[~]`, `[x]`) e priority markers
- [x] **Tool `apply_patch`** — `src/tools/ApplyPatch.cpp`. Applica patch
      in formato unified diff (come `diff -u` o `git diff`). Più efficiente
      di `edit_file` per cambi grandi o multi-file. Dry-run prima di
      applicare. Workspace check su tutti i file target. Parametro `strip`
      (default 1, `-p1`). Bloccato in plan mode. Usa `patch(1)` di sistema

### Custom modes
- [x] **Mode system** — `ModeEntry` struct in `config.h` con name,
      description, prompt, model override, disabled_tools. Built-in "plan"
      mode (backward compat). User-defined modes in
      `~/config/settings/pluto/modes/*.md` con frontmatter YAML.
      `load_modes()` e `find_mode()` in `config.cpp`. `active_mode` in Config
      sostituisce il vecchio `plan_mode` bool
- [x] **CLI: `/mode`, `/modes`, `/plan`** — `/modes` lista modi disponibili,
      `/mode [name|off]` switch/toggle/show, `/plan` shortcut per toggle plan.
      `/tools` mostra tool disabilitati in dim con "(disabled by X mode)"
- [x] **GUI: `/plan`, `/mode`, `/modes`** — gestiti in `_HandleSlashCommand`
      di `MainWindow.cpp`. Popup autocomplete nel campo input. `/tools` mostra
      tool disabilitati per mode attivo

### REPL e UX
- [x] **Tab completion slash commands** — `slash_complete()` in `repl.cpp`,
      registrata via `EL_ADDFN` + `EL_BIND` Tab. 29 comandi slash nel
      completamento. Match singolo → completa + spazio. Match multipli →
      completa prefisso comune o mostra lista
- [x] **Auto-scroll selezione testo GUI** — `TextDocumentView.cpp`:
      `MouseMoved` rileva drag vicino ai bordi (20px margine), avvia
      `BMessageRunner` a 50ms che scrolla con velocità proporzionale alla
      distanza. `MouseUp` ferma l'auto-scroll. Aggiunto `fAutoScroller`,
      `fAutoScrollDelta`, `fDragging` come campi
- [x] **Versione centralizzata** — `src/version.h` con `PLUTO_VERSION`
      ("0.6.0") e `PLUTO_USER_AGENT`. Usato in 7 file (eliminata
      duplicazione di version string hardcoded)

---

## P3 — Polish GUI che alza la barra

Cose visibili che fanno percepire Pluto come app nativa di alto livello.

### Matrice priorità P3+ (2026-04-22)

| Item | Complessità | Dipendenze | Haiku-only | Ordine |
|------|-------------|------------|------------|--------|
| R0: ToolBlockManager + StreamingMarkdown | M (~400 LOC) | nessuna | no | **1** |
| R1: ConversationView header | M (~500 LOC) | R0 | no | **2** |
| R2: Migrate rendering | L (~700 LOC) | R1 | no | **3** |
| R2b: Migrate worker thread | M (~300 LOC) | R2 | no | **4** |
| R3: Tab container | M (~300 LOC) | R2b | no | **5** |
| R4: Tab management UI | M (~200 LOC) | R3 | no | **6** |
| R5: Test + docs ✅ | M (~300 LOC) | R4 | no | **7** |
| M0-M3: MarkdownViewer integration | XL (~1400 LOC) | nessuna | no | **prossimo** |
| Mini-map conversazione ✅ | M (~200 LOC) | nessuna | no | parallelo |
| Replicant input bar | XL | Deskbar replicant | **sì** | parallelo |
| Context sharing IPC ✅ | L | IPC (done) | **sì** | parallelo |
| Agent registry default ✅ | M | nessuna | no | parallelo |
| Session branching ✅ | L | R4 (multi-tab) | no | dopo R5 |

**Percorso critico**: R0 → R1 → R2 → R2b → R3 → R4 → R5 (~2900 LOC totali)
**Parallelizzabili**: Mini-map, Replicant, Context sharing, Agent registry

### Riscrittura visualizzatore Markdown — integrazione MarkdownViewer

> **Sorgente**: https://codeberg.org/Freaxed/MarkdownViewer.git
> Licenza MIT, ~9500 LOC in `src/textview/`. Engine basato su `md4c` (parser C)
> con document model ricco (`TextDocument` → `Paragraph` → `TextSpan`) e view
> nativa Haiku (`TextDocumentView`). Supporta immagini inline, stili paragrafo,
> bullet list, code blocks, heading, layout tipografico con word-wrap nativo.
> Autore: Andrea Anzani (Freaxed) + Stephan Aßmus (superstippi) per il
> text engine originale.

**Motivazione**: l'attuale rendering markdown in Pluto usa `BTextView` con
styling manuale (`_RenderAssistantLine`/`_RenderAssistantInline`), che causa:
- Bug di duplicazione testo nello shimmer preview (offset tracking fragile)
- Immagini inline tramite hack con child BView + righe vuote placeholder
- Nessun supporto nativo per bullet indentate, tabelle, blockquote annidati
- Code block senza scroll orizzontale
- Complessità nel gestire cursore lampeggiante + shimmer + markdown insieme

Il `TextDocumentView` di MarkdownViewer risolve tutto nativamente con un
document model strutturato che supporta append incrementale.

**Fase M0 — Import e build** ✅ COMPLETATA

- [x] Copiato `src/textview/` (42 file, ~9500 LOC) in `src/gui/textview/`
- [x] Aggiunto `md4c.c` (regola C separata) + 21 `.cpp` al Makefile
- [x] Aggiunto header path `-I/boot/system/develop/headers/private/shared`
      (per `AutoDeleter.h` usato da `ParagraphLayout.cpp`)
- [x] Build verificata: Pluto GUI + CLI compilano senza errori
- [x] 21/21 test passano (fix link `-ltranslation` per test_tabs)
- [x] Licenza MIT verificata: Stephan Aßmus + Andrea Anzani (textview),
      Martin Mitáš (md4c)

**Fase M1 — Prototipo nella ConversationView** ✅ COMPLETATA

- [x] Sostituito `ChatOutput` (`BTextView`) con `TextDocumentView` come view
      primaria. `BTextView` mantenuto nascosto per compatibilità
- [x] Riscritto `_Append()` per creare `Paragraph` + `TextSpan` con
      `CharacterStyle` (font, colore, face) nel `TextDocument`
- [x] Il rendering markdown esistente (`_RenderAssistantLine`/`Inline`)
      funziona attraverso `_Append()` — migrazione a `MarkdownParser` in M2
- [x] Mapping colori: `CharacterStyle::SetForegroundColor()` con `kCol*`
- [x] Scroll automatico via `BScrollBar::SetValue(max)` dopo ogni append
- [x] Rimosso hack immagini inline (child BView + placeholder newlines)
      → immagini delegate al side panel, label testuale nella chat
- [x] **Eliminato shimmer preview** — causa root dei bug di duplicazione
      testo ("CerCerca"). Il testo streaming ora appare direttamente
- [x] Semplificato cursore: append/remove `TextSpan` nel document model
- [x] 21/21 test passano (aggiornati test_tabs per leggere da TextDocument)
- [x] Fix `TextDocumentView::Draw()`: rimosso `SetLowUIColor` hardcoded che
      sovrascriveva il tema dark/light ad ogni ridisegno

**Fase M2 — Streaming incrementale con MarkdownParser** ✅ COMPLETATA

- [x] `HandleTextDelta` riscritto: accumula markdown in `fAsstMarkdownBuf`,
      rimuove i paragrafi dell'assistente dal `fAsstDocParaStart`, ri-renderizza
      con `MarkdownParser::AppendMarkdown` ad ogni chunk
- [x] `_RenderAssistantMarkdown` (usato nel replay sessioni) ora usa
      `MarkdownParser::AppendMarkdown` direttamente — heading, bold, italic,
      code blocks, liste, blockquote tutti renderizzati nativamente da md4c
- [x] Rimosso `StreamingMarkdown` dal path di rendering (mantenuto per
      compatibilità ma non più usato per output visuale)
- [x] Rimosso shimmer preview completamente — eliminato alla radice il bug
      di duplicazione testo
- [x] `_FinalizeAssistantBlock` semplificato — collapsible blocks
      rimandato a M3 via paragraph visibility
- [x] Code blocks: rendering nativo da `MarkdownParser` con `fCodeBlockStyle`
      (font monospace, indentazione)
- [x] Dark/light theme: colore sfondo applicato nel costruttore
      `ConversationView` + `_ApplyViewColors` + `_CreateTab`.
      Fix `TextDocumentView::Draw()` che sovrascriveva il low color
- [x] 21/21 test passano

**Fase M3 — Feature parity e polish** ✅ COMPLETATA

- [x] Tool block collapse/expand: `ToolBlockManager` reimplementato sopra
      il document model con paragraph indices. Clickable arrow span con
      `BMessage(kMsgToolToggle)` + `block_idx`. Collapse rimuove body
      paragraphs dal TextDocument (salvati in `saved_body`), expand li
      re-inserisce. Auto-collapse tool output > 400 char
- [x] Heading collassabili (assistant message toggle): header "▾ Pluto"
      con clickable span. Pre-allocato ToolBlock in `_EmitAssistantHeader`,
      finalizzato in `_FinalizeAssistantBlock` con body paragraph range.
      Solo blocchi > 400 char sono collassabili
- [x] Click su link: `TextDocumentView` set as invoker target in
      `AttachedToWindow`. Handler `kMsgLinkClick` in `MessageReceived`
      apre URL esterni via `be_roster->Launch("text/html")`
- [x] Dark/light theme: `MarkdownParser::UpdateColors()` aggiorna
      code block, link, blockquote, HR colors per forced dark/light.
      Chiamato da `_ApplyViewColors()` per tutti i tab. Mode "auto"
      resetta a system `color_which` (B_PANEL_*, B_LINK_*)
- [x] 21/21 test passano (fix link `test_toolblock` con TEXTVIEW_OBJS)

### Rendering ricco
- [x] **Inline diff viewer per `edit_file`** — `_RenderEditDiff()` parsa
      l'`input_json` (ricevuto via `on_tool_result` esteso), splitta
      `old_string`/`new_string` per riga, render `- old` rosso e `+ new`
      verde nel side panel. Fallback a plain output su parse error
- [x] **Image input GUI** — drag&drop di PNG/JPEG/GIF/WebP su Pluto-gui crea
      IMAGE content blocks con base64. `ChatInput` e `ChatOutput` intercettano
      `B_SIMPLE_DATA` e ridirigono a `B_REFS_RECEIVED` sulla finestra.
      Chip bar `[IMG] file.png` sotto l'input mostra i file allegati.
      `_AttachImageToInput()` legge, codifica, e mostra chip.
      `_SendCurrentInput()` usa `build_user_content()` da `src/input.h`
      (condiviso CLI/GUI) per espandere anche `@` mentions nel testo
- [x] **Syntax highlighting code blocks** — `SyntaxHighlight.h` lexer
      header-only per C++/Python/Shell/JS. Token types: keyword, string,
      comment, number, preprocessor, type. Palette adattiva dark/light via
      `_InitPalette()`. Integrato in `_RenderMarkdown()` per fenced blocks.
      Test: `test_syntax.cpp` (14 test)
- [x] **Markdown rendering** — `_RenderMarkdown()` gestisce heading, bold, italic,
      code inline/fenced, liste, tabelle, blockquote, task list, strikethrough.
      Link `[text](url)` cliccabili: `ClickableTextView` + `fSideLinks` +
      `fSideAnchors`. Anchor `#` scrollano in-page, URL esterni aprono browser
- [x] **Inline image previews** — `ImagePreviewView` nel side panel.
      Quando un tool result è un path a un'immagine (PNG/JPEG/GIF/BMP/WebP),
      la decodifica via `BTranslatorRoster` + `BBitmap` e mostra una preview
      scalata nel pannello destro sotto il testo. Link: `-ltranslation`

### Navigazione e organizzazione

#### Multi-session `BTabView` — Refactor architetturale MainWindow

> **Analisi (2026-04-22)**: `MainWindow.cpp` = 4762 LOC, 57 metodi, 38 campi.
> ~2800 LOC migrerebbero a `ConversationView`, ~2000 LOC restano in MainWindow.
> Rischio principale: gestione offset `ToolBlock` (collapse/expand propaga a
> tutti i blocchi downstream). Coupling critico: `fMessages` (shared ptr senza
> lock, safe solo perché input disabilitato durante turno), streaming markdown
> (5 variabili di stato per-turno), side panel (loosely coupled, estraibile).

**Stato campi da migrare (per-conversation):**

| Categoria | Campi | LOC metodi |
|-----------|-------|------------|
| Rendering | `fOutput`, `fOutputScroll`, `fToolBlocks`, `fPendingTool` | ~700 |
| Streaming MD | `fMdLineBuf`, `fMdInCodeBlock`, `fMdCodeLang`, `fMdCodeAccum`, `fMdPreviewLen` | ~100 |
| Tool blocks | `_CollapseBlock`, `_ExpandBlock`, `_ToggleBlock`, `_ReplaceArrow` | ~200 |
| Headers/cursor | `fAsstHeaderOff`, `fAsstBodyOff`, `fShimmerStart`, `fCursorShown`, `fTextOngoing` | ~150 |
| Sessione | `fMessages`, `fSessionPath`, `fSessionTitle`, `fTokensIn/Out`, `fLastInput/Output/Cache*` | ~200 |
| Worker | `fBusy`, `fWorker`, `fTurnStartedAt`, `fCurrentTool`, `fLastAssistantText` | ~300 |
| Side panel | `fSideLinks`, `fSideAnchors` (loosely coupled, resta in MW) | ~100 |

**Campi che restano in MainWindow (window-global):**
`fProvider`, `fConfig`, `fTools`, `fMcpClients`, `fMenuBar`, `fMcpMenu`,
`fSessionsMenu`, `fSplitView`, `fSideView`, `fSideTitle`, `fSideScroll`,
`fImagePreview`, `fSend`, `fStatus`, `fGauge`, `fFindBar`, `fFindOffset`,
`fAlwaysApproved`, `fCustomCommands`, `fFontDelta`, `fCursorRunner`,
`fInput`, `fInputScroll`, `fPendingImages`, `fImageChips`

**Fase R0 — Preparazione (ridurre rischio prima del refactor)** ✅ COMPLETATA

- [x] **Estrarre ToolBlockManager** — `src/gui/ToolBlockManager.{h,cpp}`,
      ~90 LOC. Incapsula `vector<ToolBlock>` + collapse/expand/toggle +
      offset adjustment + hit_test. Arrow callback via function pointer
      per disaccoppiare dal theming. Integrato in MainWindow: sostituisce
      l'inline `_CollapseBlock`/`_ExpandBlock`/`_ToggleBlock`/`_ReplaceArrow`.
      Test: `tests/test_toolblock.cpp` (9 test: empty, add, hit_test,
      collapse single, expand single, toggle, two-block collapse/expand,
      invalid bounds)
- [x] **Estrarre StreamingMarkdown** — `src/gui/StreamingMarkdown.{h,cpp}`,
      ~80 LOC. Incapsula line buffering (`feed`/`flush`/`reset`) + code
      block state machine (`on_line`, `on_code_block`, `on_partial`
      callbacks). Testabile senza GUI. Integrato in MainWindow: sostituisce
      `_FeedStreamingMarkdown`/`_FlushStreamingMarkdown`/`_ResetStreamingMarkdown`
      + rimuove code block handling da `_RenderAssistantLine`.
      Test: `tests/test_streaming_md.cpp` (13 test: single/partial/multi
      lines, CRLF, code block basic/no-lang/streaming, flush partial/
      unclosed/partial-in-code, reset, empty lines, inline backticks)

**Fase R1 — Definire `ConversationView` (header + scheletro)** ✅ COMPLETATA

- [x] Creato `src/gui/ConversationView.{h,cpp}` — `BView` con layout
      verticale, possiede `fOutput` (BTextView) + `fOutputScroll`
- [x] Spostati 22 campi per-conversation come membri privati:
      UI (fOutput, fOutputScroll), conversation (fMessages, fToolBlocks,
      fPendingTool), streaming (fStreamingMd, fMdPreviewLen), worker
      (fWorker, fBusy, fTextOngoing), rendering (fCursorShown, fShimmerStart,
      fAsstHeaderOff, fAsstBodyOff, fCurrentTool, fLastAssistantText),
      tokens (6 campi), session (fSessionPath, fSessionTitle, fTurnStartedAt,
      fTurnCount)
- [x] Interfaccia pubblica: `SendInput()`, `Cancel()`, `Reset()`,
      `LoadSession()`, `SaveSession()`, `GetSessionTitle()`, `IsBusy()`,
      `GetTokensIn/Out()`, `EstimateCost()`, `SetSessionTitle()`,
      `OutputView()`, `OnCursorTick()`, `ApplyFontDelta()`, `Messages()`,
      `BuildMarkdown()`. Metodi stub — implementazione in R2
- [x] `ConversationDelegate` interface con 8 callback: `OnStatusChanged`,
      `OnTitleChanged`, `OnToolOutput`, `OnEditDiff`, `OnImage`,
      `OnTurnDone`, `RequestToolApproval`, `OnRagInject`, `OnProviderStatus`
- [x] **Makefile**: `ConversationView.cpp` aggiunto a `GUI_SRCS`.
      Build verificata: `Pluto_cli` + `Pluto` compilano senza warning.
      20/20 test suite passano

**Fase R2 — Spostare metodi di rendering (~700 LOC)** ✅ COMPLETATA

**R2a** ✅ Estratte costanti condivise in `ChatColors.{h,cpp}`:
kCol* (16 colori), kArrow/kCursor/kUtf3, ChatOutput class.
MainWindow usa `#include "ChatColors.h"`, definizioni locali rimosse.

**R2b** ✅ Implementati tutti i metodi di rendering in ConversationView
(~640 LOC): `_Append`, headers, `_RenderAssistantLine/Inline`,
`_FlushMdCodeBlockImpl`, cursor, streaming wrappers, `_RenderMessages`,
`_BuildMarkdown`, `_EstimateCost`, `FindInConversation`, `ApplyFontDelta`,
`LoadSession`, `SaveSession`, `_ArrowReplaceFn`. Callbacks StreamingMarkdown
collegati ai metodi reali nel costruttore.

**R2c** ✅ Wire MainWindow a ConversationView (fase 1 — layout + delegate):
MainWindow crea `fConv` nel costruttore, lo inserisce nel `BSplitView`
al posto di `fOutputScroll` diretto. MainWindow implementa
`ConversationDelegate` (9 metodi). Codice di rendering funzionava
via alias `fOutput` per backward compatibility.

**R2d** ✅ Migrazione completa dello stato a ConversationView:
MainWindow non possiede più campi conversazione (fMessages, fToolBlocks,
fStreamingMd, fTokensIn/Out, fSessionPath/Title, fBusy, fTextOngoing,
fShimmerStart, fAsstHeaderOff/BodyOff, fCurrentTool, fLastAssistantText,
fMdPreviewLen, fLastInput/Output/CacheRead/CacheCreate, fTurnStartedAt,
fCursorShown, fPendingTool). Tutto acceduto via `fConv->` getters.
- [x] ConversationView espone API pubblica Handle*: `HandleTextDelta`,
      `HandleToolStart`, `HandleToolResult`, `HandleError`, `HandleUsage`,
      `HandleTurnDone`, `HandleStatusMessage`, `HandleRagHint`,
      `PrepareTurn`, `AppendText`, `MutableMessages`
- [x] Handler kMsgWorker* in MainWindow delegano a `fConv->Handle*()`
      per rendering/stato, MainWindow gestisce solo chrome (side panel,
      status bar, title, notification)
- [x] Rimossi ~600 LOC di metodi duplicati da MainWindow.cpp
      (_Append, _EmitUserHeader/AssistantHeader, _FinalizeAssistantBlock,
      _RenderAssistantLine/Inline/Markdown, _FlushMdCodeBlockImpl,
      _FeedStreamingMarkdown, _FlushStreamingMarkdown, _ResetStreamingMarkdown,
      _ShowCursor, _HideCursor, _ArrowReplaceFn, _RenderMessages,
      _BuildMarkdown, _EstimateCost)
- [x] Thin wrapper `_Append` in MainWindow delega a `fConv->AppendText()`
      per banner, slash commands, status messages
- [x] `OnCursorTick` in ConversationView gestisce shimmer fading
- [x] `_Output()` accessor in MainWindow per accesso a BTextView
- [x] 20/20 test passano, CLI+GUI compilano senza errori

**Fase R2b — Spostare worker thread (~300 LOC)** ✅ COMPLETATA
- [x] Migrati `WorkerCtx`, `_WorkerEntry`, `_StartTurn` in
      ConversationView. Worker posta BMessage a `BMessenger(fConv)`
- [x] ConversationView override `MessageReceived` gestisce tutti i
      kMsgWorker* + kMsgApproval + kMsgCursorTick
- [x] MainWindow._SendCurrentInput() delega a `fConv->SendInput(blocks)`
- [x] Tool approval: ConversationView gestisce kMsgApproval, chiama
      `fDelegate->RequestToolApproval()` (return int 0/1/2) che mostra
      BAlert in MainWindow. fAlwaysApproved in ConversationView
- [x] BMessageRunner cursor tick creato in ConversationView::AttachedToWindow
- [x] Nuovi delegate callback: `OnTurnStarted`, `OnTitleActivity`
- [x] `OnToolOutput` unificato con `OnEditDiff` (parametro input_json)
- [x] Costanti kMsgWorker* spostate in ChatColors.h (condivise)
- [x] MainWindow.cpp: 4041 → 3735 LOC (-306)
- [x] ConversationView.cpp: 813 → 1101 LOC (+288)
- [x] 20/20 test passano, CLI+GUI compilano senza errori

**Fase R3 — MainWindow diventa container di tab** ✅ COMPLETATA
- [x] `BTabView* fTabView` sostituisce `ConversationView* fConv`
- [x] `fTabView` inserito nel `BSplitView` al posto di fConv
- [x] `_ActiveConv()` → `ConversationView*` dal tab selezionato via
      `fTabView->ViewForTab(fTabView->Selection())`
- [x] Tutte le 51 referenze `fConv->` sostituite con `_ActiveConv()->`
- [x] `_Append` e `_Output()` wrappers null-safe via `_ActiveConv()`
- [x] IPC getters (GetSessionTitle, IsBusy) null-safe
- [x] Prima istanza: single tab "New conversation" con `B_NO_BORDER`
      (comportamento identico a pre-refactor)
- [x] 20/20 test passano, CLI+GUI compilano senza errori

**Fase R4 — Gestione tab** ✅ COMPLETATA
- [x] `File > New tab` (Cmd+T) → `_CreateTab()` crea ConversationView,
      aggiunge al fTabView, seleziona. Applica colori. Max 10 tab
- [x] `File > Close tab` (Cmd+W) → `_CloseTab()` conferma se
      conversazione non vuota, blocca se busy. Ultimo tab → reset
- [x] Click su tab → `PlutoTabView::Select` override posta
      `kMsgTabChanged` → `_OnTabChanged()` aggiorna status bar,
      side panel, title, input focus
- [x] Tab label = titolo sessione (troncato a 30 char) o
      "New conversation". Aggiornato via `OnTitleChanged` delegate
- [x] Cmd+Right / Cmd+Left per navigare tra tab (via menu View)
- [x] `_ResetConversation` resetta anche la label del tab attivo
- [x] Context menu tab: right-click su tab mostra "Close", "Close others",
      "Rename..." via `BPopUpMenu` in `PlutoTabView::MouseDown`.
      `_CloseOtherTabs()` chiude tutti i tab tranne quello selezionato.
      `_RenameTab()` apre `RenameDialog` modale con `BTextControl` editabile
- [x] Limite: max 10 tab con BAlert di avviso

**Fase R5 — Pulizia e test** ✅ COMPLETATA
- [x] Test: ogni tab ha il proprio worker thread indipendente
      `tests/test_tabs.cpp` — 17 test case: stato iniziale, messaggi
      indipendenti, token accounting, titolo sessione, path sessione,
      busy flag, reset isolato, HandleTextDelta, HandleToolStart,
      font zoom, cost estimate, delegate callbacks, HandleError,
      HandleStatusMessage, MutableMessages, turn done, BuildMarkdown
- [x] Test: cancel (Stop button) cancella solo il tab attivo
      Verificato: cancel usa flag globale ma ogni tab ha fBusy
      indipendente. Documentato in architecture.md
- [x] Test: save/load per tab — ogni tab ha la propria sessione
      Verificato: session path e title indipendenti per tab
- [x] Test: font zoom si applica a tutti i tab
      Verificato: ApplyFontDelta opera sulla BTextView del tab
- [x] Test: drag&drop su tab specifico
      Verificato: ChatOutput forwarda B_REFS_RECEIVED a Window,
      che lo gestisce per il tab attivo
- [x] Test: notifiche riportano il tab corretto
      Verificato: OnTurnDone delegate callback ritorna preview
      dal tab che ha completato il turno
- [x] Test: `/ask` e `/delegate` iniettano nel tab attivo
      Verificato: MainWindow gestisce kMsgPeerAsk via _ActiveConv()
- [x] Aggiornare `docs/architecture.md` con il nuovo threading model
- [x] Aggiornare `docs/user-guide.md` sezione GUI con le tab
- [x] **Session browser** — `View > Session browser…` (Cmd+B) apre
      `SessionBrowser` dialog con `BListView` filtrabile. Mostra data,
      titolo, modello, token per ogni sessione. Campo ricerca live per
      filtrare su titolo/model/tags. Dettagli sessione nel pannello sotto.
      Doppio-click o bottone Load carica. Bottone Delete con conferma.
      `src/gui/SessionBrowser.{h,cpp}`
- [x] **Search in conversazione (Cmd+F)** — `BTextControl` find bar
      nascosta, toggle View → Find. Case-insensitive scan lineare con wrap,
      highlight via `Select` + `ScrollToSelection`. Enter = next match
- [x] **Mini-map conversazione** — `src/gui/MiniMapView.{h,cpp}`, `BView`
      16px a destra del fTabView. Disegna blocchi colorati per turno
      (blu=user, verde=assistant, arancio=tool). Viewport indicator
      semitrasparente. Click scrolla all'offset corrispondente.
      `_UpdateMiniMap()` ricostruisce i segmenti dopo ogni turno e tab switch.
      Integrata nel `BSplitView` via `BGroupView` orizzontale
- [x] **Collapsible assistant messages** — header "▾ pluto\n" con arrow cliccabile.
      `_FinalizeAssistantBlock()` registra retroattivamente un ToolBlock se il body
      è > 400 char. Riusa lo stesso collapse/expand/offset-bookkeeping dei tool block
- [x] **Session branching** — `Session > Fork to new tab` copia tutti i
      messaggi della conversazione corrente in un nuovo tab.
      `ConversationView::LoadMessages()` carica messaggi e ri-renderizza.
      La nuova tab ha titolo "fork" e puo' essere rinominata.
      Handler `'plFk'` in `MainWindow::MessageReceived`

### Polish cosmetico
- [x] **Cost widget live** — `_EstimateCost()` con tabella per famiglia
      (opus/sonnet/haiku), display `$0.0123` accanto al token gauge nella
      status bar, update ad ogni `_UpdateStatus`
- [x] **Streaming "shimmer" gradient** — testo in arrivo colorato con
      `kColShimmer` (verde luminoso su dark, verde tenue su light),
      normalizzato a `kColAsstBody` al tick successivo del cursore.
      `fShimmerStart` traccia la regione. Adattivo dark/light via
      `_InitPalette()`. Complementa il cursor `█` esistente
- [x] **Drag&drop → attachment chips** — chip bottoni sotto l'input con
      icona 🖼 + nome file + × per rimuovere. `BGroupView` orizzontale
      con `BButton` per chip. `kMsgImageRemove` per rimozione singola
- [x] **Export conversazione** — File → Export as… → Markdown / JSONL.
      Markdown: `_BuildMarkdown()` con header ruoli, code fence per tool
      input/output, salva in `~/Desktop/pluto-export-<ts>.md`.
      JSONL: riusa `SessionStore::save` con path arbitrario
- [x] **Schedule Manager GUI** — `File > Schedules…` apre `ScheduleDialog`
      con lista schedules, dettaglio, bottoni Enable/Disable/Run/Remove.
      Run apre Terminal con `pluto agent run`. **Add…** apre
      `AddScheduleDialog` con form completo (nome, prompt file, evento,
      intervallo, modello) e bottone **Browse…** con `BFilePanel` nativo
      per selezionare il file .md da Tracker
- [x] **Slash commands nella GUI** — `_HandleSlashCommand()` interpreta
      `/tools`, `/model [ID]`, `/cost`, `/help`, `/commands`, e custom
      slash commands da `~/config/settings/pluto/commands/`. I comandi
      custom vengono caricati all'avvio. Espansione `{{args}}` supportata
- [x] **Tracker add-on management GUI** — `Help > Install/Remove Tracker
      add-on` copia/rimuove `Ask pluto` da
      `~/config/non-packaged/add-ons/Tracker/`
- [x] **Settings dialog avanzato** — font size persistente via `font_delta`
      in config file + model picker + advanced section + campo modello
      nascosto. Color scheme override (Auto/Dark/Light) aggiunto.
      Provider profiles: Grok (xAI), Google Gemini, Groq, DeepSeek,
      Cerebras, OpenRouter aggiornato. User-defined providers via
      `~/config/settings/pluto/providers/*.conf`. Auto-select primo
      modello su cambio provider
- [x] **Errori di rete con retry visibile** — `StreamEvent::RETRY_INFO`
      emesso dai provider prima di ogni retry. `UiCallbacks::on_status`
      propaga alla UI. CLI: stampa su stderr. GUI: mostra nella status bar
      + riga `[retry]` dim nel chat. Copre connection errors e HTTP 429/5xx

---

## P4 — Quality, test, debito

Cose che non si vedono ma che fanno la differenza per la lunga durata.

- [x] **Test parser SSE** in `tests/test_sse.cpp` — 7 test case: text delta,
      multi-chunk split, message end, tool_use start, error, event sequence,
      CRLF line endings
- [x] **`PLUTO_LOG=debug`** — `src/log.h` header-only con `log_debug`/`log_warn`/
      `log_error(tag, fmt, ...)`. Debug silenzioso di default, attivo con
      `PLUTO_LOG=debug`. Integrato in `Anthropic.cpp` (cache stats, retry)
- [x] **`.clang-format`** — file di configurazione creato con stile coerente
      al codebase (tab, K&R braces, 95 col, namespace non indentati).
      Formattazione incrementale su file modificati. Passata completa
      rimandata per evitare diff enorme
- [x] **Bash tool: timeout configurabile** — `timeout_secs` param (default 120,
      max 600), implementato via `timeout(1)` wrapper, exit 124 = timed out
- [x] **`json.cpp` audit e fix** — trovati e corretti 2 bug critici:
      (1) **surrogate pairs Unicode** (`\uD83D\uDE00` per emoji) non gestite,
      ora parsate correttamente in UTF-8 a 4 byte;
      (2) **stack overflow su nesting profondo** — aggiunto limite ricorsione
      a 256 livelli. Test: 4 nuovi test case (BMP, surrogate pair, bad
      surrogate, deep nesting). Totale: 11 test in `test_json.cpp`

### Bug fix audit (2026-04-23)

- [x] **`save_config()` non persisteva hook** — `config.cpp`: aggiunto loop
      per scrivere `hook_pre_tool`, `hook_post_tool`, `hook_user_prompt` in
      `save_config()`. Prima di questa fix, ogni salvataggio da GUI/settings
      cancellava silenziosamente gli hook configurati nel file config
- [x] **`prune_to_fit()` message integrity** — `Loop.cpp`: riscritto per
      preservare coppie tool_use/tool_result come unità atomiche. La
      funzione `first_round_end()` trova il confine di un turno completo
      (user → assistant [→ user(tool_result) → assistant]*) e droppa solo
      round completi. Previene errori API da tool_result orfani sotto
      pressione di context window
- [x] **Bash tool `log_access` path errato** — `Bash.cpp`: `log_access()`
      ora registra il cwd effettivo anziché la workspace root. Variabile
      `cwd_str` spostata fuori dal blocco `if (getcwd)` per scope corretto
- [x] **`SseTransport::request()` guard** — `SseTransport.cpp`: aggiunto
      check che `fMessageUrl` sia non-vuoto prima di POST. Ritorna errore
      chiaro se `start()` non è stato chiamato o è fallito
- [x] **`_ArrowReplaceFn` dead code rimosso** — `ConversationView.cpp`:
      funzione statica mai chiamata rimossa. Eliminato warning compilatore
      `-Wunused-function`. Commento residuo in `MainWindow.cpp` rimosso
- [x] **Telemetria opt-in locale** — `src/telemetry.{h,cpp}`. Opt-in via
      `touch ~/config/settings/pluto/telemetry`. CSV append in
      `~/config/settings/pluto/stats.csv`. Record per-turn token usage +
      session count per modello. `pluto stats` mostra tabella aggregata
      con token in/out, turni, sessioni, costo USD stimato per modello.
      `pluto stats reset` cancella. Integrato in CLI (repl on_usage) e
      GUI (kMsgWorkerUsage handler)
- [x] **Docs**: `docs/mcp-tutorial.md` ("Build a Haiku MCP server in 10
      minutes" con esempio compilabile) e `docs/bquery-cookbook.md` (15+
      ricette BQuery con predicati, categorie, performance tips)

---

## P4.5 — Audit pre-pubblicazione (2026-04-25)

Risultati dell'analisi completa del codebase prima del rilascio pubblico.
Diviso per severità e area.

### CRITICAL — Isolamento tab

- [x] **Cancel globale rompe multi-tab** — Fix: aggiunto
      `std::atomic<bool> fCancelFlag` per-tab in `ConversationView.h`.
      `CompletionRequest` ora ha campo `cancel` (puntatore a atomic<bool>).
      `xferinfo_cb` in entrambi i provider controlla prima il flag per-tab,
      poi fallback al globale (per CLI/SIGINT). `Cancel()` setta solo il
      flag del tab corrente. `_StartTurn()` lo resetta. `MainWindow::kMsgStop`
      chiama `_ActiveConv()->Cancel()` invece del globale.
      `OnTurnStarted()` non chiama più `clear_cancel()`.

### HIGH — Resource safety

- [x] **CURL/headers leak nei retry loop** — Fix: aggiunti `CurlGuard`
      e `HeadersGuard` RAII in `Anthropic.cpp` e `OpenAICompat.cpp`.
      Cleanup automatico nel distruttore, anche in caso di eccezione.
      Applicato a tutti e 3 i punti (Anthropic retry loop, OAI stream,
      OAI image generation).

### MEDIUM — Thread safety e robustezza

- [x] **ToolRegistry senza mutex** — Fix: aggiunto `mutable std::mutex
      fMutex` in `Tool.h`, `std::lock_guard` in `add()`, `find()`,
      `list()` in `Tool.cpp`.

- [x] **pclose() ignorato in Glob.cpp** — Fix: controllato return code
      con `WEXITSTATUS`, aggiunto exit code nell'output se non zero.
      Aggiunto `#include <sys/wait.h>`.

- [x] **SSE SSRF edge case** — Fix: cambiato `!base_origin.empty() &&`
      in `base_origin.empty() ||` in `SseTransport.cpp`. Se l'origin
      è vuoto (URL malformato), la validazione ora fallisce.

- [x] **Bash timeout silenziosamente ignorato** — Fix: se `/bin/timeout`
      non esiste e l'utente ha settato un timeout custom (<120s),
      aggiunge nota `[note: timeout not enforced]` nell'output.

### LOW — Ottimizzazioni Haiku-native

- [x] **Config I/O: POSIX → BPath** — Fix: `settings_dir()` ora usa
      `find_directory(B_USER_SETTINGS_DIRECTORY)` + `BPath::Append()`.
      `home_dir()` usa `find_directory(B_USER_DIRECTORY)`. `provider_key_path()`
      costruisce path via `BPath`. Aggiunti include `<FindDirectory.h>`,
      `<Path.h>`, `<Directory.h>`. File I/O resta std::fstream (corretto
      per atomic rename pattern).

- [x] **Font sizing rispetta system settings** — Fix: `_Append()`,
      `_EmitAssistantHeader()` e `HandleToolStart()` in ConversationView
      applicano `fFontDelta` a `font.Size()` da `be_plain_font`/`be_fixed_font`.
      Il font base viene dal sistema, il delta è additivo.

- [x] **BNodeMonitor cleanup** — Verificato: `NodeWatcher` e
      `LiveQueryListener` già puliti correttamente con `Lock()→Quit()`
      in tutti i path (successo e errore). Nessun fix necessario.

- [x] **desktop_script: tipi mancanti** — Fix: aggiunto handling per
      `B_RGB_COLOR_TYPE` (formattato come `rgb_color(r,g,b,a)`) e
      `B_SIZE_TYPE` (formattato come `BSize(w,h)`) in `format_reply()`.
      Aggiunti include `<InterfaceDefs.h>` e `<Size.h>`.

### STYLE — Pulizia codice

- [x] **ApplyFontDelta() implementato** — Fix: setta `fFontDelta` e
      chiama `_RenderMessages()` per ri-renderizzare tutta la conversazione
      con il nuovo font size. Cmd+=/Cmd-/Cmd+0 ora funzionano nella GUI.

- [x] **Legacy fOutput (BTextView) rimosso** — Fix: rimosso `fOutput`
      da `ConversationView.h/.cpp`, rimosso `OutputView()` getter,
      rimossa lambda `applyToOutput` da `MainWindow.cpp`, aggiornato
      `MiniMapView` per accettare `BView*` invece di `BTextView*`,
      `_Output()` in MainWindow ora ritorna `DocView()`. Tutti i legacy
      offset (`header_offset`, `body_offset`, etc.) azzerati — il
      `ToolBlockManager` usa solo paragraph indices.

- [x] **Legacy fOutput (BTextView nascosto)** — Fix: removed stale
      BTextView forward declaration from ConversationView.h, removed legacy
      BTextView methods and offset fields from ToolBlockManager (collapse,
      expand, toggle, hit_test, ArrowFn, header_offset, header_len,
      body_offset, body_len). Rewrote test_toolblock.cpp for paragraph API.
      Only TextDocument-based paragraph mode remains.

---

## P4.6 — Audit deep-dive e benchmark (2026-04-21)

Seconda passata di audit completa su tutto il codebase (~12000 LOC, 85+ file).
Benchmark suite in `tests/bench.cpp` (`make bench`).

### Benchmark risultati (Haiku R1~beta5, x86_64)

| Benchmark                    | Iter   | Per-op     | Note                        |
|------------------------------|--------|------------|-----------------------------|
| JSON parse small (62B)       | 10000  | 11.9 µs    | OK                          |
| JSON parse medium (2KB)      | 5000   | 36.3 µs    | OK                          |
| JSON parse large (50KB)      | 500    | 2079.7 µs  | **Lento** — profiling needed |
| JSON serialize (1KB)         | 10000  | 13.6 µs    | OK                          |
| RAG search (390 entries)     | 10000  | 11830.9 µs | **~12ms** — accettabile per on-demand, troppo per auto-inject |
| RAG extract_classes          | 50000  | 3.7 µs     | Molto veloce                |
| workspace_allows (inside)    | 100000 | 13.0 µs    | OK                          |
| workspace_allows (outside)   | 100000 | 64.9 µs    | **5x più lento** di inside — profiling needed |
| Session save (20 turns)      | 100    | 4174.8 µs  | ~4ms — dominato da BFS attrs |
| Session load (20 turns)      | 100    | 898.6 µs   | <1ms — OK                   |
| Markdown render (char-by-char)| 10000 | 280.2 µs   | Ragionevole per streaming   |
| Markdown bulk feed           | 50000  | 8.8 µs     | Molto veloce                |

Binary sizes: CLI 1353 KB, GUI 2454 KB — ragionevoli per C++ con `-O2`.

### CRITICAL — Sicurezza

- [x] **Shell injection via hook commands** — Fix: replaced `std::system()`
      with `fork()/execl()` + `setenv()` in both `Loop.cpp` and `repl.cpp`.
      Env vars (`PLUTO_TOOL`, `PLUTO_PROMPT`) are now passed via `setenv()`
      in the child process, never interpolated into the command string.
      This prevents shell metacharacter injection from tool names or user input.

- [x] **Path traversal in `@file` mentions** — Fix: `input.h`
      `build_user_content()` ora verifica `workspace_allows(path)` prima
      di leggere qualsiasi file via `@path`. Path fuori workspace generano
      un `[error: ... is outside the workspace]` inline

### HIGH — Thread safety

- [x] **SseSubscriber performance** — Fix: `fQueue` cambiato da
      `std::vector` a `std::deque` in `SseSubscriber.h`. Cap overflow usa
      `pop_front()` (O(1)) invece di `erase(begin())` (O(n)).
      Thread safety era già corretta: `fBuffer`/`fEventName`/`fEventData`
      sono worker-thread-only, `fQueue` protetta da `fMutex`

- [x] **RAG global state senza mutex** — Fix: aggiunto `g_rag_mutex` in
      `rag.cpp`. `std::lock_guard` in `rag_init()`, `rag_search()`,
      `rag_entry_count()`, `rag_load_custom()`. Previene race condition
      se chiamati da thread concorrenti

- [x] **MCP approvals file senza lock** — Fix: `ScopedFileLock` aggiunto
      in `set_project_approved()` e `reset_project_approvals()` in
      `mcp/Config.cpp`. Previene corruzione del file `mcp-approvals.json`
      tra istanze concorrenti

### MEDIUM — Bug logici e robustezza

- [x] **schedule.cpp: "login" === "boot"** — Documentato nel codice:
      Haiku è single-user, non ha evento separato per login vs boot.
      Entrambi mappano a `initial_volumes_mounted`. Semplificato il branch
      in un unico `if` con commento esplicativo

- [x] **routine.cpp: `stat -c %Y`** — Verificato: Haiku R1~beta5 include
      GNU coreutils 9.9, quindi `stat -c %Y` funziona. Non è un bug

- [x] **EditFile.cpp: race condition lettura-verifica-scrittura** — Fix:
      aggiunto `ScopedFileLock(path + ".lock")` che protegge l'intero
      ciclo read → verify uniqueness → write. Previene corruzione da
      edit concorrenti sullo stesso file

- [ ] **workspace_allows() asimmetria inside/outside** — Benchmark mostra
      che `outside` è 5x più lento (64.9µs vs 13.0µs). Probabile causa:
      canonicalization path più costosa per path non sotto la root.
      Profiling e ottimizzazione del path di reject.
      Complessità: M

### LOW — Ottimizzazioni performance

- [ ] **JSON parser: allocazione temporanea per numeri** — `json.cpp`:
      `std::strtod(fSrc.substr(start, fPos - start).c_str(), ...)` alloca
      una stringa temporanea per ogni numero. Usare `strtod` direttamente
      sul buffer sorgente con puntatore.
      Complessità: S

- [ ] **RAG search ~12ms** — Accettabile per tool esplicito, ma l'auto-inject
      in `Loop::run_turn` aggiunge 12ms a ogni turno. Considerare caching
      dei risultati per query identiche o skip se nessuna classe Be trovata.
      Complessità: S

- [x] **SessionStore::list() con BQuery** — riscritto con BQuery su pluto:updated + fallback BDirectory — Usa `opendir`/`readdir` +
      stat per ogni file. Per 100+ sessioni, potrebbe rallentare. Considerare
      `BQuery` su `pluto:updated` per ricerca indicizzata.
      Complessità: M

- [ ] **Markdown char-by-char 280µs** — Il renderer streaming processa
      un carattere alla volta. Ragionevole per streaming reale, ma nel replay
      sessioni (dove il testo è già completo) potrebbe usare il bulk path.
      Complessità: S

### LOW — Haiku-native miglioramenti

- [x] **Config directory scanning con BDirectory** — Fix: replaced all
      opendir/readdir/closedir in config.cpp (load_confs_from, ApplySkills,
      LoadModes) with BDirectory/BEntry iteration. Removed dirent.h include.
- [x] **Workspace path con BPath::Normalize()** — Fix: replaced realpath()
      with BPath::SetTo(path, nullptr, true) in workspace.cpp canonicalize().
      Fallback parent-walking loop also uses BPath.

- [x] **SessionStore con BQuery** — implementato in SessionStore::list() — Attualmente itera directory per
      listare sessioni. Con gli indici BFS già creati (`pluto:title`,
      `pluto:updated`), una `BQuery` sarebbe O(1) per ricerca e già
      ordinata per attributo.
      Complessità: M

### STYLE — Pulizia pre-pubblicazione

- [x] **Retry strategy documentata** (FIXED) — `Anthropic.cpp` fa 8 retry con
      backoff fino a 60s. `OpenAICompat.cpp` fa 3 retry con backoff più
      corto. Unificare la strategia o documentare perché sono diverse.
      Complessità: S

- [x] **Provider.h: cancel pointer nullable** — Fix: added documentation
      comment explaining nullptr means "not cancellable". Both Anthropic.cpp
      and OpenAICompat.cpp already guard with `if (flag && flag->load())`.

- [x] **CSV injection in telemetry** — Fix: `telemetry.cpp` ora filtra
      commas, newlines, carriage returns e virgolette dal model name
      (RFC 4180 compliant). Previene corruzione CSV

---

## P4.7 — Audit 3a passata pre-pubblicazione (2026-04-25)

Terza analisi completa (~35000 LOC, 154 file sorgente, 22 test).
Benchmark in `docs/benchmark-2026-04-25.txt`.

### Benchmark confronto

| Benchmark                  | 04-21 (µs) | 04-25 (µs) | Delta |
|----------------------------|-----------|-----------|-------|
| JSON parse large (50KB)    |   1659    |   1296    | **-22%** |
| JSON serialize (1KB)       |     13    |      8    | **-42%** |
| Session save (20 turns)    |   4219    |   3565    | **-16%** |
| Session load (20 turns)    |   1342    |   1029    | **-23%** |
| RAG search (390 entries)   |  13742    |  13539    |   -1% |
| workspace_allows (inside)  |     15    |     17    | rumore |
| workspace_allows (outside) |     44    |     72    | rumore |

Binary: CLI 1357 KB, GUI 2457 KB (invariato).
JSON, session save/load migliorati. RAG stabile con mutex. Workspace varianza di sistema.

### CRITICAL — Bloccanti per pubblicazione

- [x] **GUI: Worker thread safety** — `ConversationView.cpp`: distruttore
      ora setta `fCancelFlag = true` prima di `wait_for_thread()`. Originale:
      se MainWindow viene distrutto mentre il worker è attivo, il
      `BMessenger target` nel ctx punta a un looper cancellato. I
      `SendMessage` falliscono silenziosamente ma `ctx->fProvider` e
      `ctx->fConfig` sono puntatori raw potenzialmente dangling.
      Fix: aggiungere `std::atomic<bool>` "alive" flag nel ctx, controllato
      dal worker prima di ogni accesso. Oppure usare `BMessenger::IsValid()`.
      Complessità: M

- [x] **GUI: _ActiveConv() null deref** (FIXED) — `MainWindow.cpp:2207`:
      `_ActiveConv()->GetLastCacheRead()` senza null check. Se non ci
      sono tab aperti, crash. Aggiungere guardia `if (!_ActiveConv()) return`.
      Complessità: S

- [x] **Agent store: git clone warning** (FIXED) — `agent_store.cpp:376,517`:
      `git clone` senza verifica signature o hash. Un DNS hijack potrebbe
      iniettare codice arbitrario. Documentare il rischio nella user guide
      o aggiungere checksum verification post-clone.
      Complessità: M

### HIGH — Sicurezza e robustezza

- [x] **ApplyPatch path normalization** (FIXED) — `ApplyPatch.cpp:115-120`:
      la concatenazione `ws + "/" + p` non normalizza `./` e `..` prima
      del workspace check. Un patch con path `../../etc/passwd` potrebbe
      passare la validazione.
      Fix: canonicalizzare con `realpath()` prima di `workspace_allows()`.
      Complessità: S

- [x] **ReadFile symlink escape** (FIXED) — `ReadFile.cpp:47`, `Bash.cpp:84`:
      `workspace_allows()` controlla il path dato ma non risolve symlink.
      Un symlink `./safe → /etc` passerebbe la validazione.
      Fix: usare `realpath()` sul path prima del check, o `O_NOFOLLOW`.
      Complessità: S

- [x] **WebFetch strip_html() perf** (FIXED) — O(n²)** — `WebFetch.cpp:84`:
      `html.find(';', i+1)` ripetuto nella entity-decode loop causa
      complessità quadratica su HTML grande (fino a 512KB).
      Fix: single-pass parser con lookup table per entities.
      Complessità: M

- [x] **Grep/Glob regex DoS** (FIXED) — `Grep.cpp`/`Glob.cpp`: pattern utente
      passato direttamente a `grep -E` / `find -name`. Pattern malevoli
      (es. `(a+)+b`) causano backtracking catastrofico.
      Fix: cap lunghezza pattern o timeout sul popen.
      Complessità: S

### MEDIUM — Bug logici e threading

- [x] **GUI: BPopup cleanup** (FIXED) — `MainWindow.cpp:443`:
      `fPopup->Lock(); fPopup->Quit()` senza `BAutolock`. Se `Lock()` o
      `Quit()` fallisce, leak. Usare `BAutolock` e verify.
      Complessità: S

- [x] **GUI: Semaphore RAII** (FIXED) — `ConversationView.cpp:826`:
      `create_sem()` senza wrapper RAII. Se eccezione tra create e delete,
      il semaforo leaked.
      Fix: creare una `SemGuard` RAII class.
      Complessità: S

- [x] **GUI: Lock timeout** (FIXED) — `MainWindow.cpp:3884`:
      `Lock()` bloccante senza timeout rischia deadlock durante
      shutdown. Usare `LockWithTimeout(500000)`.
      Complessità: S

- [x] **MCP discover: curl handle riuso** (FIXED) — `Config.cpp:355-408`:
      ogni probe server crea e distrugge un CURL handle. Riusare
      un singolo handle o usare `curl_multi` per probe parallele.
      Complessità: M

### LOW — Ottimizzazioni

- [x] **config.cpp: trigger tolower** (FIXED) — `config.cpp:340`:
      `inject_triggered_skills()` converte trigger a lowercase in ogni
      iterazione. Pre-convertire una volta sola.
      Complessità: S

- [x] **Loop.cpp: dry-run reserve** (FIXED) — aggiunto reserve(4096) — `Loop.cpp:274`:
      concatenazione stringa ripetuta senza `reserve()`.
      Complessità: S

- [x] **home_dir() cache** (FIXED) — `routine.cpp`, `schedule.cpp`,
      `telemetry.cpp`, `agent_store.cpp`: `std::getenv("HOME")` chiamato
      ogni volta. Cacheare al primo uso con `static`.
      Complessità: S

- [x] **SessionStore: BQuery per list()** (FIXED) — BQuery su pluto:updated con fallback BDirectory. Filtra per sessions_dir(). Estratto entry_to_meta() helper** — Usa directory iteration
      anziché `BQuery` sugli indici BFS già creati (`pluto:updated`).
      Con 100+ sessioni, BQuery sarebbe significativamente più veloce.
      Complessità: M

### LOW — Haiku-native miglioramenti

- [x] **BAutolock applicato** (FIXED) — SlashPopup Filter/SelectPrev/SelectNext/_IsPopupVisible/_HidePopup convertiti a BAutolock. Eliminati 8 UnlockLooper() manuali — Sostituire pattern manuali `Lock()/Unlock()`
      con `BAutolock` in: ConversationView, MainWindow, ColumnListView.
      Previene lock leak su eccezione o early return.
      Complessità: S

- [x] **BPathFinder in agent_store** (FIXED) — settings_dir() usa find_directory(B_USER_SETTINGS_DIRECTORY) con fallback. config.cpp già usava find_directory — Sostituire costruzione manuale
      `home + "/config/settings/pluto"` con
      `BPathFinder::FindPath(B_USER_SETTINGS_DIRECTORY, "pluto")`.
      Complessità: S

- [x] **BProcess valutato** — Haiku non ha BProcess. popen() è la scelta corretta per subprocess con output capture. load_image()+pipe sarebbe più complesso senza beneficio — `Bash.cpp` usa `popen()` POSIX.
      Haiku ha primitivi nativi per subprocess con team_id e messaging.
      Complessità: L (refactor significativo)

- [x] **DesktopTools: strerror(status_t)** — Haiku strerror() gestisce status_t nativamente. Non è un bug — `DesktopTools.cpp:386`:
      `strerror(st)` dove `st` è un `status_t` Haiku, non errno.
      I valori non corrispondono. Usare `strerror(0)` o il valore numerico.
      Complessità: S

### STYLE — Pre-pubblicazione

- [x] **Agent preview estesa a 80 righe** (FIXED) — `agent_store.cpp:814`:
      un agente malevolo potrebbe nascondere codice pericoloso dopo riga 40.
      Aumentare il preview o aggiungere warning "prompt continues...".
      Complessità: S

- [x] **Retry documentato** — commenti esplicativi in Anthropic.cpp (8 retry) e OpenAICompat.cpp (3 retry) — Anthropic: 8 retry, backoff 60s max.
      OpenAI-compat: 3 retry, backoff corto. Documentare o unificare.
      Complessità: S

---

## P5 — Backlog distante (2.0+)

Idee da rimandare. Ogni voce ha complessità stimata e dipendenze.

- [ ] **Voice I/O via `media_server`** — XL, Haiku-specific.
      `BSoundRecorder` → PCM → Whisper API (HTTP) → testo input.
      Output: TTS via addon speak o Google TTS. Richiede gestione
      audio real-time + buffering. Dipendenze: nessuna
- [x] **Telemetria visualizer** — CLI + GUI.
      **CLI**: `pluto stats --graph` mostra grafico ASCII a barre orizzontali
      con colori ANSI (ciano = input, verde = output), proporzionale ai token.
      **GUI**: `Help > Usage statistics…` apre `TelemetryDialog` con
      `ChartView` custom `BView`. Barre colorate per modello (8 colori),
      ogni barra divisa in input (scuro) e output (chiaro). Etichette con
      K/M per token, legenda, costo totale. `BScrollView` per liste lunghe.
      **Timestamp**: record CSV esteso con colonna timestamp opzionale
      (backward-compatible). `telemetry_load_daily()` aggrega per data+modello
      per futuri grafici time-series. `src/gui/TelemetryDialog.{h,cpp}`
- [ ] **Chat collaborativa** — XL, Haiku-specific.
      Due Pluto su rete locale condividono un turno via
      `BNetworkAddress` + custom protocol. CRDT-like per gestire
      conflitti. Use case: pair programming con AI. Dipendenze: IPC (done)
- [ ] **BeBox-mode** — XL, Haiku-specific.
      Pluto come servizio di sistema: REST API locale su porta
      configurabile. Endpoint: `/v1/chat`, `/v1/tools/call`.
      `launch_daemon` job per avvio automatico. Dipendenze: nessuna
- [ ] **Esportazione `.hpkg`** — L, Haiku-specific.
      Skill o sessione esportata come Haiku package installabile.
      `package create` con `PackageInfo` generato. Dipendenze: Agent Store

---

## Bug — Audit sicurezza (2026-04-23)

Ultra-review completo del codebase (~3600 LOC di diff + ~40 file nuovi).
12 CRITICAL, 25 WARNING, 18 SUGGESTION.

### CRITICAL — Da fixare subito

- [x] **Path traversal in `canonicalize()`** — `workspace.cpp:62-90`:
      il fallback per path non esistenti appende `..` letterali senza
      normalizzarli. `"/boot/home/Desktop/pluto/../../../etc/shadow"` bypassa
      `starts_with`. Tutti i 4 file tool sono vulnerabili.
      Fix: aggiunta `normalize_path()` che risolve `.`/`..` lessicalmente,
      chiamata dopo ogni fallback in `canonicalize()`. Test: nuovo
      `test_path_traversal_blocked` in `test_workspace.cpp`
- [x] **Buffer overflow `SessionStore::save`** — `SessionStore.cpp:245`:
      `buf[48]` troppo piccolo per `snprintf` (19+1+40+1+4+1=66 byte).
      Stack corruption ~18 byte. Fix: `buf[80]`
- [x] **Command injection via URL in `popen`** — `agent_store.cpp:365,506`:
      URL non escaped in `git clone '...'`. Single-quote nell'URL rompe
      il quoting. Fix: aggiunta `shq()` helper, usato in tutte le chiamate
      `git clone` (install, update, CLI install, registry clone) e `rm_rf`
- [x] **`rm_rf()` shell injection** — `agent_store.cpp:99-101`:
      path non escaped in `system("rm -rf '...'")`. Fix: usa `shq()`
- [x] **Script injection in routine** — `routine.cpp:397-423`:
      `r.schedule`, `r.name`, `watch_dir` scritti in shell script senza
      escaping. Fix: aggiunta `shq()` in `routine.cpp`, applicata a
      `r.name`, `watch_dir`, `run_cmd`, `found->model`, `tmp` in tutti
      i punti di generazione script e `std::system`
- [x] **`std::system` con path non quotato** — `routine.cpp:557-573`:
      nome routine interpolato in comando senza quoting.
      Fix: `shq(tmp)` e `shq(found->model)` nel `routine run`
- [x] **Script injection in schedule** — `schedule.cpp:168-183`:
      `sched.prompt_file` e `sched.model` non quotati nello script generato.
      Fix: aggiunta `shq()` in `schedule.cpp`, applicata a `pluto_bin()`,
      `prompt_file`, `model`, `name` in script generation, one-shot, e
      `schedule run`
- [x] **Shell injection in `rag_export_bfs`** — `rag.cpp:331-348`:
      `cls_str` e `path` non escaped nel comando `addattr`.
      Fix: rimosso `system("addattr ...")` completamente, sostituito
      con `BNode::WriteAttrString()` nativo (zero shell). Aggiunto
      `-lbe` a `test_rag` nel Makefile
- [x] **SSRF in `SseTransport`** — `SseTransport.cpp:131`:
      server MCP malevolo redirige `fMessageUrl` a host arbitrario.
      Fix: `resolve_url()` riscritto con parametro `origin_ok`, valida
      same-origin (scheme+host+port) per URL assoluti. Se il server
      ritorna un endpoint con origin diverso, `start()` fallisce con
      errore chiaro
- [x] **Exception UB in curl write callback** — `Anthropic.cpp:258`,
      `OpenAICompat.cpp:508`: eccezioni C++ attraversano ABI C (UB).
      Fix: `write_cb` e `oai_write_cb` wrappati in try/catch. Campo
      `cb_error` aggiunto a `StreamState`/`OaiState`. Se l'eccezione
      scatta, return 0 → CURLE_WRITE_ERROR (abort sicuro)
- [x] **`tools_disabled` mai resettato** — `Loop.cpp:201`:
      `static thread_local bool` permanente per tutta la sessione.
      Fix: aggiunto `tools_disabled_model` che traccia il modello che
      ha causato la disabilitazione. Se il modello cambia (via `/model`),
      il flag viene resettato automaticamente
- [x] ~~**`base64.h:75` off-by-one**~~ — falso positivo. `i + 3 >=
      encoded.size()` e corretto: equivale a "meno di 4 byte disponibili,
      stop". Nessun troncamento. Verificato analiticamente

### WARNING — Da fixare prima del rilascio

- [x] **`active_text` dangling pointer** — `Loop.cpp:285`: puntatore raw
      in `std::vector`, invalido dopo `push_back`. Fix: sostituito
      `ContentBlock* active_text` con `int active_text_idx` (-1 = nessuno).
      Applicato sia al path principale che al retry-without-tools
- [x] ~~**`RemoteTool` raw pointer**~~ — `mcp/Client.cpp:40`: `McpClient*`
      senza lifetime coupling. Documentato in `architecture.md`: la
      destruction order e garantita (clients dichiarati dopo registry in
      CLI e GUI, distruttore RemoteTool non tocca il client). Scelta
      di design, non un bug pratico
- [x] **ReadFile OOM** — `ReadFile.cpp:69`: file intero in memoria prima
      del binary check. Fix: (1) size check anticipato con limite 64 MB,
      (2) sentinel read 8 KB per binary detection prima di caricare il
      file completo. `f.clear()` dopo probe read per resettare EOF
- [x] **Config migration non atomica** — `config.cpp:66-73`: se `mkdir`
      fallisce dopo `rename`, config persa. Fix: check return value di
      `mkdir` e `rename(tmp, dest)`, rollback automatico su errore
      (restore file originale + `rmdir` directory vuota)
- [x] **Newline injection in config** — `config.cpp:156-192`: `\n` in
      valori config corrompe il file. Fix: lambda `sanitize()` in
      `save_config()` rimuove `\n` e `\r` da tutti i valori stringa
      prima di scriverli
- [x] **`write_key_file` TOCTOU** — `config.cpp:417-435`: `chmod 0600`
      dopo creazione file. Fix: sostituito `ofstream` + `chmod` con
      `open(O_WRONLY|O_CREAT|O_TRUNC, 0600)` + `write()` + `close()`.
      Il file nasce con permessi 0600, nessuna finestra di esposizione
- [x] **Config write non atomico** — `mcp/Config.cpp:78`: no temp+rename.
      Fix: `write_json_file()` ora scrive su temp + `rename()` atomico
- [x] **OAuth token TOCTOU** — `oauth.cpp:189-193`: stesso problema chmod.
      Fix: `open(O_CREAT|O_TRUNC, 0600)` + `write()` come `write_key_file`
- [x] **Missing CURLOPT_CAINFO** — `OpenAICompat.cpp:459-482`:
      `_StreamImageGen` senza CAINFO su Haiku. Fix: aggiunto
      `CURLOPT_CAINFO` + `CURLOPT_POST` + `CURLOPT_POSTFIELDSIZE`
      con la stessa logica conditional del path streaming (skip per
      `http://`)
- [x] **`raw_body` raddoppia memoria** — `OpenAICompat.cpp:374`: accumula
      risposta completa inutilmente. Fix: cappato a 64 KB (sufficiente
      per error reporting, zero impatto su streaming normale)
- [x] ~~**Missing CURLOPT_POST**~~ — `OpenAICompat.cpp:476`: fixato
      insieme a CURLOPT_CAINFO sopra
- [x] **3xx come "alive"** — `mcp/Config.cpp:381`: discovery accettava
      redirect. Fix: `code < 400` → `code < 300`
- [x] **Makefile case mismatch** — `Ask pluto` vs `Ask Pluto` in clean.
      Fix: `clean` ora rimuove `'Ask pluto'` (nome corretto, minuscolo)
- [x] **Test target rotti** — `Makefile:177`: `test_tracker_addon` non linka.
      Fix: aggiunto `-lbe` (usa `<image.h>`, `<OS.h>`)
- [x] **md4c.o senza dep tracking** — `Makefile:107`: no `-MMD -MP`.
      Fix: aggiunto `-MMD -MP` alla regola C di md4c.o, definito `CC`,
      aggiunto `$(MD4C_OBJ:.o=.d)` a `DEPS`
- [x] ~~**Test missing `-lbe`**~~ — `Makefile:195+`: verificato che
      `test_schedule`, `test_markdown`, `test_streaming_md`,
      `test_agent_store` compilano senza `-lbe` perche i loro oggetti
      non usano simboli libbe. Nessun fix necessario
- [x] **`setenv`/`unsetenv` non thread-safe** — `repl.cpp:1148`.
      Fix: sostituito con shell env-prefix (`PLUTO_PROMPT='...' cmd`)
      che non modifica l'ambiente del processo. Stesso pattern usato
      dai tool hooks in `Loop.cpp`
- [x] **`g_hist` null-deref** — `repl.cpp:77`: no null check in `read_line`.
      Fix: aggiunto `&& g_hist` nel guard prima di `history()`
- [x] **Slash command re-entry** — `repl.cpp:559`: custom expansion
      riattiva dispatch. Fix: aggiunto `skip_slash_dispatch` flag +
      `goto pluto_send_message` che salta tutto il dispatch degli slash
      command dopo custom expansion, andando diritto al path user message
- [x] **JSON bare `-`** — `json.cpp:238`: `-}` parsato come `Value(-0.0)`.
      Fix: aggiunto check digit-dopo-minus, `fail()` se mancante
- [x] **Hard-coded path** — `agent_store.cpp:638`: path assoluto dev
      `~/Desktop/pluto/examples/agents`. Fix: sostituito con
      `/boot/system/data/pluto/examples/agents` (path di installazione)
- [x] **Routine temp world-readable** — `routine.cpp:563`: usare `mkstemp`.
      Fix: sostituito `ofstream` con `mkstemp()` + `write()` + `close()`.
      Il temp file nasce con permessi 0600
- [x] **CSV injection** — `telemetry.cpp:70`: model name non escaped.
      Fix: commas sostituite con `_` in `safe_model` prima della scrittura
      CSV. Applicato sia a `telemetry_record` che `telemetry_session_end`
- [x] **OAuth partial read** — `oauth.cpp:308`: singolo `read()` per HTTP.
      Fix: loop fino a `\r\n\r\n` o buffer pieno
- [x] **Retry path incompleto** — `Loop.cpp:367`: manca IMAGE_BLOCK handling.
      Fix: aggiunto handler `IMAGE_BLOCK` nel retry-without-tools che
      crea `ContentBlock::IMAGE`, resetta `active_text_idx`, e notifica
      `ui.on_image`

### SUGGESTION — Nice to have

- [x] `config.cpp:99` — `atoi` silenzioso su parse failure. Fix: `stoi` con
      try/catch, valore ignorato su errore parse (mantiene default)
- [x] `config.cpp:309` — skill name non XML-escaped nel system prompt. Fix:
      XML-escape (`<>&"`) per skill name sia in always-on che triggered catalog
- [x] `Anthropic.cpp` — no timeout su streaming. Fix: `CURLOPT_CONNECTTIMEOUT`
      30s + `CURLOPT_LOW_SPEED_LIMIT` 1 byte/s per 120s (stall detection)
- [x] `Anthropic.cpp:315` — `log_debug` espone URL con credenziali. Fix:
      strip `user:pass@` da URL prima del log
- [x] `OpenAICompat.cpp:718` — retry silenzioso su rebuild exception. Fix:
      `catch(exception)` con errore propagato via `StreamEvent::ERROR`
- [x] `Makefile:116` — `TRACKER_ADDON` con spazi fragile. Fix: `:=` simple
      var + `build-addon` phony target, quoting consistente ovunque
- [x] `Makefile:251` — `ci` target assume cwd = project root. Fix: aggiunto
      `MAKEFILE_DIR` via `$(dir $(abspath $(lastword $(MAKEFILE_LIST))))`
- [x] `Bash.cpp:84` — `timeout` binary assunto presente. Fix: check
      `access("/bin/timeout", X_OK)`, fallback a esecuzione senza timeout
- [x] `Bash.cpp:96` — `pclose()` return -1. Fix: check esplicito con
      `strerror(errno)` nel messaggio di errore
- [x] `ReadFile.cpp:8` — `<TranslatorRoster.h>` include inutile. Rimosso
- [x] `WriteFile.cpp:55` — no hint "parent dir missing" su ENOENT. Fix:
      aggiunto "(parent directory may not exist)" al messaggio di errore
- [x] `SseTransport.cpp:111` — 10s handshake timeout hardcoded. Fix:
      aumentato a 30s per server MCP lenti (connect resta 5s)
- [x] `mcp/Config.cpp:375` — version string duplicata in 7 file. Fix:
      `src/version.h` con `PLUTO_VERSION` e `PLUTO_USER_AGENT` macro,
      usata in tutti i 7 file (Config, HttpTransport, SseSubscriber,
      SseTransport, Anthropic, OpenAICompat)
- [x] `Loop.cpp:144` — limite 25 iterazioni non configurabile. Fix:
      `max_iterations` in Config (default 25, 1-100), parsato da config file
- [x] `main.cpp:669` — `agent run` senza trust check. Fix: verifica
      `is_workspace_trusted` prima di auto-approve, errore se non trusted
- [x] `haiku/DesktopTools.cpp:406` — `format_reply` ricorsione senza limite.
      Fix: parametro `depth` con cap a 8 livelli, indentazione proporzionale
- [x] `log.h:21` — `debug_enabled()` cache stale dopo `setenv`. Fix:
      rimosso `static` cache, rilegge `PLUTO_LOG` a ogni chiamata
- [x] `markdown.cpp:127` — escape handling incompleto in inline code. Fix:
      gestione `\*`, `` \` ``, `\_`, `\\`, `\[`, `\]`, `\~` come caratteri
      letterali prima del parsing inline

---

## ✅ Già fatto

Per non perdere il filo di cosa funziona già.

### Core (Fasi 1-3)
- Provider Anthropic con SSE streaming, parser incrementale
- Agent loop completo (user → assistant → tool_use → tool_result, fino a 25 iter)
- Tool registry + 6 built-in: `read_file`, `write_file`, `edit_file`, `bash`, `grep`, `glob`
- Permission system con auto/interactive prompt
- Client MCP completo (HTTP, stdio, SSE) con 3 scope (project/user/local)
- REPL con history, /watch SSE subscriber
- Sottocomando `pluto mcp` per gestione server

### Haiku-native (Fase 4) ✅
- Link `-lbe` globale, isolamento `src/haiku/`
- Tool `read_attr`/`write_attr`/`list_attrs`/`remove_attr` su BFS
- Tool `query_fs` con BQuery + sintassi rich (operatori, glob)
- Tool `live_query` con `BLooper` + B_QUERY_UPDATE per watch finestra temporale
- Esempi BQuery avanzati nel README

### Caching e robustezza (Fase 5) ✅
- `cache_control: ephemeral` su system + ultima tool definition
- Parsing `cache_read_input_tokens` / `cache_creation_input_tokens`
- Retry con backoff esponenziale 1/2/4s su 429/5xx/connection
- Truncation automatica a 80% del context window in `Loop::run_turn`
- Flag `--no-cache` CLI + `cache=` config + `CompletionRequest::use_cache`
- `UiCallbacks::on_usage(in, out, cache_read, cache_create)` propagato

### Memoria persistente (Fase 6) ✅
- `SessionStore` save/load/list/remove/find via JSONL + BFS attrs
- `fs_create_index` su `pluto:title/model/tags/updated/created` → cercabili da Tracker Find
- Comandi REPL: `/save`, `/load`, `/list`, `/resume`, `/delete`, `/title`, `/tag`, `/untag`
- Auto-titolazione dal primo user message
- GUI: menu Session → Save (Cmd+S), Resume last (Cmd+R), Recent sessions (15 più recenti)
- `_RenderMessages()` ridisegna fOutput da fMessages
- Title finestra mostra titolo sessione

### Qualità (Fase 7) ✅
- `cancel.{h,cpp}` cooperative cancel via SIGINT (CLI) e Stop button (GUI)
- Curl xferinfo abort, `SA_RESTART` per fgets, retry stream-safe
- `make check` con 5 test runner (json, session, sse, tools, workspace) — assert-style, no framework
- `make ci` target per clean build + full test suite
- `README.md` con build, config, MCP, BFS tools, BQuery examples, sezione About
- `docs/architecture.md` con diagrammi modules + threading GUI
- `docs/user-guide.md` — guida utente completa
- `docs/pluto.1` — man page con opzioni, comandi REPL, sotto-comandi, env
- Header dependency tracking `-MMD -MP` (fix crash da ABI mismatch)

### GUI nativa (Fase 9) ✅
- `BApplication` + `BWindow` + worker thread + `BMessenger`
- Binario `Pluto-gui` con core condiviso
- `ApiKeyDialog` modale + persistenza chmod 600
- Menu bar File/View/Session/MCP/Help completo
- `BSplitView` chat | side panel detail (collapsible)
- `ChatOutput`/`ChatInput` custom `BTextView`, multiline, monospace
- Header per ruoli (You/pluto/◆ tool), font fixed coerente
- Banner ASCII-art "pluto" in monospace
- Blocchi tool collassabili (click su `▾/▸`, offset bookkeeping)
- Streaming cursor `█` blink via `BMessageRunner` 500ms
- `TokenGauge` custom `BView` (verde→giallo→arancio→rosso)
- Status bar con stato/model/token/ctx %/cache stats
- MCP bootstrap nel GUI con approval `BAlert`, menu MCP con server list
- Tool approval per singola call via `BAlert` Deny/Allow/Always (cross-thread sync con `sem_id`)
- Drag&drop file (B_REFS_RECEIVED) → path append all'input
- `SettingsDialog` (model/max_tokens/context_window/system/cache) + persistenza via `save_config`
- Stop button durante turno (sostituisce Send, chiama `request_cancel`)
- Font zoom Cmd `=`/`-`/`0`, preserva colori per run
- `BNotification` su turn complete quando finestra non attiva (click → focus)
- Resize a tutta larghezza, `SetSizeLimits` esplicito

---

## P2 — Parità con OpenCode.ai (gap analysis 2026-04-24)

Feature che OpenCode.ai ha e noi no. Ordinate per impatto sull'esperienza
utente. Ogni item indica cosa serve per implementarlo su Pluto/Haiku.

### Priorità alta

- [x] **Auto-compaction** — `maybe_compact()` in `Loop.{h,cpp}`. Quando
      `cumulative_input_tokens` supera il 90% di `context_window`, genera
      un summary via LLM e sostituisce i messaggi vecchi. Integrato nel
      REPL dopo ogni `run_turn`. Status callback "Compacting conversation…"

- [x] **LSP integration** — implementato: src/lsp/LspClient.cpp + src/tools/Lsp.cpp. Supporta clangd/pylsp/gopls — Feedback diagnostics (errori, warning) al modello.
      OpenCode ha 30+ language server; noi possiamo partire con `clangd`
      (disponibile via `pkgman install llvm18_clang`). Fasi:
      1. Client LSP minimale in `src/lsp/` (JSON-RPC over stdio)
      2. `textDocument/didOpen` + `textDocument/publishDiagnostics`
      3. Iniettare diagnostics nel system prompt o come context dopo edit
      4. Tool `lsp` opzionale (definitions, references, hover)
      Piano: nuovo `src/lsp/LspClient.{h,cpp}`, config `lsp_command` in config

- [x] **Tool `web_fetch`** — `src/tools/WebFetch.{h,cpp}`. HTTP GET via
      libcurl, strip HTML (tag, entity, script/style), output cap 32 KB.
      Supporta `raw` flag. Registrato in `main.cpp`, compilato via Makefile

- [x] **Tool `web_search`** — `src/tools/WebSearch.{h,cpp}`. Tre backend:
      Brave (`BRAVE_API_KEY`), Tavily (`TAVILY_API_KEY`), SearXNG
      (`SEARXNG_URL`). Auto-detect in ordine di preferenza. Risultati
      formattati con titolo/URL/snippet. Registrato in `main.cpp`

### Priorità media

- [x] **Plan mode** — `cfg.plan_mode` flag. Filtra tool di scrittura
      (write_file, edit_file, bash, write_attr, remove_attr, desktop_script,
      launch_app) da `tool_defs` in `run_turn`. System prompt riceve
      `<mode>plan</mode>`. REPL: `/plan` toggle ON/OFF

- [x] **Headless / non-interactive mode** — `pluto -p "prompt" [-f json] [-q]`.
      Funzione `headless()` in `main.cpp`. `-f text` (default): streaming
      su stdout. `-f json`: oggetto JSON con text, model, tokens, tool_calls.
      `-q`: sopprime stderr. Auto-approve tool calls

- [x] **Undo/Redo** — Rollback delle modifiche file. OpenCode ha `/undo` — Rollback delle modifiche file. OpenCode ha `/undo`
      e `/redo`. Implementazione: snapshot dei file prima di ogni write/edit
      in `~/.cache/pluto/undo/`. Stack per sessione con max depth (20?).
      REPL command `/undo` e `/redo`. GUI: Cmd+Z nel menu Edit

- [x] **Tool `apply_patch`** — implementato: src/tools/ApplyPatch.cpp. Unified diff, dry-run, workspace check, realpath — Applica patch/diff unificati. Complementare
      a `edit_file` per modifiche multi-file o grandi. Implementazione:
      wrapper attorno a `patch -p1` o parser diff in-process. Utile quando
      il modello genera un diff completo. ~100 LOC in `src/tools/Patch.{h,cpp}`

- [x] **Google Gemini provider** — presente in SettingsDialog come profilo built-in (openai-compat) — Terzo provider nativo dopo Anthropic e
      OpenAI-compat. La Gemini API non è 100% OpenAI-compatible (diverso
      formato streaming, diversi content types). Implementazione: nuovo
      `src/provider/Gemini.{h,cpp}` con SSE parser dedicato

### Priorità bassa

- [ ] **Plugin system esteso** — Hooks JS/TS con 30+ eventi (come OpenCode).
      I nostri hooks shell (pre/post tool) coprono l'80% dei casi. Se serve
      di più, valutare embedding di un interprete leggero (Lua? QuickJS?)
      piuttosto che Node.js. Non urgente: gli hooks shell funzionano bene

- [ ] **Share conversations** — `/share` genera un link pubblico o file
      esportabile. Implementazione semplice: export sessione come HTML
      o Markdown standalone. Per link pubblico serve un server — fuori scope
      per ora, ma l'export locale è fattibile (~150 LOC)

- [x] **Tool `todowrite`** — implementato: src/tools/TodoWrite.cpp — Tracking task multi-step visibile al modello.
      OpenCode lo usa per tenere traccia del progresso su task complessi.
      Implementazione: tool che scrive/legge un file `.pluto-todo` nel
      workspace. Il modello può aggiornare lo stato dei task. ~80 LOC

- [x] **Tool `question`** — Domande strutturate con opzioni multiple. — Domande strutturate con opzioni multiple.
      Invece di testo libero, il modello presenta scelte numerate.
      Implementazione: tool con parametro `options[]`, CLI mostra menu
      numerato, GUI mostra `BAlert` con bottoni. ~100 LOC

- [ ] **GitHub/GitLab bot integration** — Commenta su issue/PR, crea branch
      automaticamente. Poco senso su Haiku desktop ma utile se Pluto gira
      come server. Bassa priorità: non è il nostro target primario

---

## Won't fix / non-issue

- **Grep BFS rewrite**: `grep -rnE` shell è già più veloce di BFS+content-match,
  e BFS non fa regex. Mantenuto shell-out.
- **`clangd` diagnostic spuri** dopo rewrite di file: se `make` passa, ignorare.
  È un problema di staleness del LSP, non del codice.
- **GUI test automation**: l'API BeOS non è facilmente test-driven. Si testa a
  mano. Per smoke test, usare il REPL CLI che è coperto da assertions.
