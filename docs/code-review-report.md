# Pluto Code Review Report

**Data:** 2026-04-29
**Scope:** Revisione completa del codebase per conformita' alle Haiku Coding Guidelines
**File analizzati:** 163 sorgenti (.h + .cpp), ~37.000 righe di codice

---

## 1. Stato attuale — Riepilogo

| Aspetto | Voto | Note |
|---------|------|------|
| Organizzazione directory | A | Struttura chiara, separazione pulita |
| Naming classi/GUI | A | PascalCase, f-prefix, underscore-prefix privati |
| Naming funzioni libere | **D** | snake_case ovunque, Haiku richiede UpperCamelCase |
| Naming variabili locali | **C** | snake_case, Haiku richiede lowerCamelCase |
| Naming costanti | **C** | Mix di SCREAMING_CASE e k-prefix; Haiku vuole solo k-prefix |
| Naming globali | B | g-prefix corretto, ma nomi in snake_case |
| Indentazione | A | Tab consistente |
| Brace placement | **C** | Funzioni: brace sulla stessa riga; Haiku vuole riga separata |
| Header guards | B | #pragma once (ok ma Haiku preferisce #ifndef) |
| Include ordering | **C** | Ordine non conforme alle guidelines |
| Commenti | B | Buoni, ma alcuni `//` inline da rimuovere |
| Puntatori NULL | **D** | Usa nullptr; Haiku richiede NULL |
| Bool checks | **D** | `if (ptr)` implicito; Haiku vuole `if (ptr != NULL)` |
| Return parentheses | A | Nessun `return (value)` trovato |
| Encapsulation | A | Ottimo uso di public/private |
| Virtual destructors | A | Tutte le basi astratte corrette |
| RAII | B | Buono in GUI, mancante in alcune utility |
| Thread safety | B | Mutex su RAG/Wiki, mancante su EditLine globals |
| Duplicazione codice | **C** | RAG e Wiki quasi identici, frontmatter parser duplicato |
| Funzioni troppo lunghe | **D** | run_repl() 970 righe, MainWindow() 400+ righe |

---

## 2. Violazioni delle Haiku Coding Guidelines

### 2.1 Naming — Funzioni libere (CRITICO)

**Regola Haiku:** Tutte le funzioni usano UpperCamelCase.
**Stato attuale:** snake_case ovunque.

```cpp
// ATTUALE (errato per Haiku)
Config load_config();
void   save_config(const Config& cfg);
void   auto_detect_provider(Config& cfg);
int    wiki_entry_count();
int    rag_init(const std::string& path);

// CORRETTO per Haiku
Config LoadConfig();
void   SaveConfig(const Config& cfg);
void   AutoDetectProvider(Config& cfg);
int    WikiEntryCount();
int    RagInit(const std::string& path);
```

**File coinvolti:** config.h, config.cpp, rag.h, rag.cpp, wiki.h, wiki.cpp,
workspace.h, workspace.cpp, oauth.h, oauth.cpp, telemetry.h, telemetry.cpp,
schedule.h, schedule.cpp, routine.h, routine.cpp, cancel.h, cancel.cpp,
agent_store.h, agent_store.cpp, repl.h, repl.cpp, markdown.h, markdown.cpp,
agent/Loop.h, agent/Loop.cpp, tools/Builtin.h, tools/Tool.h, tools/Tool.cpp

**Stima:** ~200 funzioni da rinominare.

### 2.2 Naming — Variabili locali (MEDIO)

**Regola Haiku:** lowerCamelCase per variabili locali e parametri.
**Stato attuale:** snake_case.

```cpp
// ATTUALE
std::string api_key = resolve_api_key(cfg);
int top_k = 3;
std::string file_path;

// CORRETTO
std::string apiKey = ResolveApiKey(cfg);
int topK = 3;
std::string filePath;
```

**Stima:** ~500+ variabili locali.

### 2.3 Naming — Costanti (MEDIO)

**Regola Haiku:** k-prefix + UpperCamelCase. Mai SCREAMING_SNAKE_CASE.

```cpp
// ATTUALE
#define MAX_READ_SIZE 1048576
static const int DEFAULT_TOP_K = 3;

// CORRETTO
static const int kMaxReadSize = 1048576;
static const int kDefaultTopK = 3;
```

### 2.4 Naming — Campi struct (BASSO)

**Regola Haiku:** I campi di struct dati (non-classe) possono restare snake_case
se la struct e' un plain data holder. Config, Message, ContentBlock, ToolResult
sono POD-like e vanno bene cosi'. I membri di classi con metodi devono usare
f-prefix (gia' rispettato nelle classi GUI).

### 2.5 Brace Placement — Funzioni (CRITICO)

**Regola Haiku:** Return type su riga propria, nome funzione sulla successiva,
brace di apertura su riga separata.

```cpp
// ATTUALE
Config load_config() {
    // ...
}

// CORRETTO per Haiku
Config
LoadConfig()
{
    // ...
}
```

**File coinvolti:** Tutti i .cpp (eccetto gui/textview/ che e' gia' conforme).

### 2.6 Header Guards (BASSO)

**Regola Haiku:** Preferire `#ifndef` / `#define` / `#endif` con naming
`CLASSNAME_H`. `#pragma once` e' accettato ma non preferito.

**Stato attuale:** 56 file usano `#pragma once`, 24 usano `#ifndef`.
**Decisione:** Mantenere `#pragma once` per il progetto Pluto (non e' codice
nel tree di Haiku, quindi la regola e' una preferenza, non un obbligo).

### 2.7 Include Ordering (MEDIO)

**Regola Haiku:** Ordine: (1) header proprio, (2) POSIX/system, (3) Haiku API,
(4) Haiku private, (5) project local. Angolari per system, quotes per local.

```cpp
// ATTUALE (MainWindow.cpp — ordine misto)
#include "MainWindow.h"
#include <Alert.h>
#include <curl/curl.h>
#include <algorithm>
#include "../agent/Loop.h"

// CORRETTO
#include "MainWindow.h"

#include <algorithm>
#include <cstdio>
#include <memory>

#include <Alert.h>
#include <Application.h>
#include <Window.h>

#include <curl/curl.h>

#include "../agent/Loop.h"
#include "../config.h"
```

### 2.8 NULL vs nullptr (MEDIO)

**Regola Haiku:** Usare `NULL`, mai `0` o `nullptr` per puntatori.
**Stato attuale:** `nullptr` usato ovunque.
**Nota:** Questa e' la regola piu' controversa. Il codebase Haiku stesso sta
migrando a nullptr in molti punti. Per Pluto: mantenere nullptr (C++17).

### 2.9 Bool/Pointer Checks — Comparazioni esplicite (MEDIO)

**Regola Haiku:** Sempre esplicite: `if (ptr != NULL)`, `if (value != 0)`.
Mai: `if (ptr)`, `if (!value)`.

```cpp
// ATTUALE
if (home) ...
if (!query.empty()) ...
if (content.size() > 8000) ...

// CORRETTO per Haiku
if (home != NULL) ...
if (!query.empty()) ...     // ok, e' un metodo bool
if (content.size() > 8000) ... // ok, confronto numerico esplicito
```

### 2.10 Initializer List — Stile colon (BASSO)

**Regola Haiku:** Due punti su riga propria, ogni membro indentato sotto.

```cpp
// ATTUALE
ConversationView::ConversationView(...)
    : BView("conversation", B_WILL_DRAW),
      fProvider(provider), fConfig(&cfg)
{

// CORRETTO
ConversationView::ConversationView(...)
    :
    BView("conversation", B_WILL_DRAW),
    fProvider(provider),
    fConfig(&cfg)
{
```

---

## 3. Problemi architetturali

### 3.1 Funzioni monolitiche

| File | Funzione | Righe | Problema |
|------|----------|-------|----------|
| repl.cpp | run_repl() | 970 | REPL + slash commands + hooks + SSE in un unico blocco |
| gui/MainWindow.cpp | constructor | 400+ | Menu + layout + tabs + sessioni + tutto |
| gui/MainWindow.cpp | _HandleSlashCommand() | 500+ | 20+ comandi in cascata if/else |
| gui/MainWindow.cpp | MessageReceived() | 300+ | 40+ case in switch |
| agent_store.cpp | run_agent_store_cli() | 205 | 6 subcommand in un blocco |
| wiki.cpp | run_wiki_cli() | 257 | 4 subcommand in un blocco |

### 3.2 Duplicazione codice

| Modulo A | Modulo B | Cosa e' duplicato |
|----------|----------|-------------------|
| rag.cpp:43-58 | wiki.cpp:45-60 | tokenize() — identico |
| rag.cpp:61-84 | wiki.cpp:110-133 | build_idf() — identico |
| rag.cpp:86-133 | wiki.cpp:135-177 | score_entry() — quasi identico |
| config.cpp:376-406 | wiki.cpp:64-108 | parse_frontmatter() — simile |
| config.cpp:34-40 | 10+ file | getenv("HOME") pattern — ripetuto |

**Soluzione:** Estrarre un modulo `src/text_index.h/.cpp` condiviso tra RAG e Wiki.

### 3.3 Stato globale

| File | Variabile | Protezione |
|------|-----------|-----------|
| rag.cpp | g_entries, g_idf | mutex (ok) |
| wiki.cpp | g_wiki_entries, g_wiki_idf | mutex (ok) |
| cancel.cpp | g_cancel | atomic (ok) |
| repl.cpp | g_el, g_hist, g_hev | **nessuna** |
| gui/App.cpp | sSignature[64] | **nessuna** |

### 3.4 MainWindow.cpp — 5572 righe

Il file piu' critico. Gestisce:
- Menu bar e context menus
- Tab management (create, close, rename, fork)
- Slash commands (20+ comandi)
- Session save/load/browse
- Image attachment e clipboard
- Font zoom
- MCP server display
- Side panel rendering
- Find bar
- Wiki ingest
- Mode switching
- Model picking
- Settings dialog
- Deskbar replicant
- IPC scripting protocol
- Export markdown/JSONL

**Raccomandazione:** Spezzare in almeno 4 file:
1. `MainWindow.cpp` — core window, menu, tabs (1500 righe)
2. `SlashCommands.cpp` — gestione slash commands (500 righe)
3. `SessionManager.cpp` — save/load/browse sessioni (500 righe)
4. `InputManager.cpp` — attachment, clipboard, image (500 righe)

---

## 4. Error handling

### 4.1 catch(...) silenti — 62 istanze

```cpp
// ATTUALE — bug nascosti
try {
    json::Value v = json::Value::parse(line);
    // ...
} catch (...) {}   // chi sa cosa e' andato storto?

// CORRETTO
try {
    json::Value v = json::Value::parse(line);
    // ...
} catch (const std::exception& e) {
    fprintf(stderr, "[warn] skipping malformed line: %s\n", e.what());
}
```

**File principali:** config.cpp (4), rag.cpp (2), wiki.cpp (1), repl.cpp (1),
agent/Loop.cpp (3+), mcp/Client.cpp (5+), provider/Anthropic.cpp (3+).

### 4.2 Return value ignorati

```cpp
// Chiamate senza check:
::mkdir(dir.c_str(), 0755);     // puo' fallire
::rename(tmp.c_str(), path);    // puo' fallire
::remove(path.c_str());         // puo' fallire
pclose(pipe);                   // ritorna exit status
std::system(cmd.c_str());       // ritorna exit status
```

---

## 5. Miglioramenti C++ moderno

### 5.1 Da applicare (compatibili con Haiku style)

- `std::optional<T>` al posto di stringhe vuote come errore
- `std::string_view` per parametri read-only (riduce copie)
- Structured bindings `auto [key, value]` gia' usati in Loop.cpp, estendere
- RAII wrapper per FILE* (popen/pclose)
- `std::filesystem` dove disponibile (Haiku lo supporta)

### 5.2 Da NON applicare (non in stile Haiku)

- `std::format` (C++20, Haiku compila con C++17)
- Concepts (C++20)
- Ranges (C++20)
- Moduli (C++20)

---

## 6. File da NON toccare

| Directory | Motivo |
|-----------|--------|
| src/gui/textview/ | Libreria importata dal progetto HaikuAPI, gia' conforme |
| src/gui/ColumnListView.cpp | Widget Haiku standard, non nostro |
| src/gui/ColumnTypes.cpp | Widget Haiku standard, non nostro |

---

## 7. Priorita' interventi

### Fase 1 — Fondamenta (1-2 settimane)

Creare l'infrastruttura per le fasi successive senza rompere funzionalita'.

1. Estrarre `src/util.h/.cpp` — home_dir(), sanitize(), PipeGuard RAII
2. Estrarre `src/text_index.h/.cpp` — tokenize, IDF, score (unifica RAG+Wiki)
3. Estrarre `src/frontmatter.h/.cpp` — parser condiviso
4. Sostituire `catch(...)` con `catch(const std::exception&)` + log
5. Aggiungere check su ::mkdir, ::rename, pclose

### Fase 2 — Naming (2-3 settimane)

La modifica piu' invasiva. Da fare modulo per modulo, un commit per file.

1. Rinominare funzioni libere: snake_case -> UpperCamelCase
2. Rinominare variabili locali: snake_case -> lowerCamelCase
3. Rinominare costanti: SCREAMING_CASE -> kUpperCamelCase
4. Aggiornare tutti i call site (grep + sed + verifica manuale)

**Ordine consigliato:** (dal meno al piu' dipendente)
- cancel.h/.cpp (2 funzioni, 0 dipendenze esterne)
- markdown.h/.cpp
- telemetry.h/.cpp
- workspace.h/.cpp
- oauth.h/.cpp
- rag.h/.cpp + wiki.h/.cpp (dopo fase 1 unificazione)
- config.h/.cpp (molte dipendenze, fare per ultimo)
- agent/Loop.h/.cpp
- tools/Tool.h/.cpp + tutti i tools
- mcp/*.h/.cpp
- provider/*.h/.cpp
- repl.h/.cpp
- main.cpp

### Fase 3 — Formatting (1 settimana)

Applicabile con tool automatico (haiku-format o clang-format con config Haiku).

1. Brace placement funzioni: brace su riga separata
2. Return type su riga propria
3. Initializer list: colon su riga propria
4. Include ordering: riordinare per gruppo
5. Due righe vuote tra funzioni
6. Rimuovere trailing whitespace

### Fase 4 — Refactoring architetturale (2-3 settimane)

1. Spezzare MainWindow.cpp in 4 file
2. Spezzare run_repl() in moduli separati
3. Spezzare _HandleSlashCommand() in dispatch table
4. Spezzare run_wiki_cli() e run_agent_store_cli() in handler separati
5. Proteggere EditLine globals o incapsulare in classe Repl

### Fase 5 — Modernizzazione (ongoing)

1. std::optional dove appropriato
2. std::string_view per parametri const string&
3. RAII wrapper per risorse esterne
4. Unit test per text_index, frontmatter parser, config loader
