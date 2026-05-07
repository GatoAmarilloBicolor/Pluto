# Pluto — AI Assistant nativo per Haiku OS

**Versione**: 0.5.0 | **Licenza**: MIT | **Linguaggio**: C++17 | **Piattaforma**: Haiku R1/beta5+

---

## Cos'e Pluto

Pluto e il primo client AI nativo per Haiku OS. Scritto interamente in C++17 senza dipendenze pesanti (niente Electron, Node.js o Python), offre accesso diretto a Claude, GPT, Gemini e altri modelli linguistici tramite un'interfaccia a riga di comando, un'applicazione grafica nativa e un add-on per il file manager Tracker.

Non e un semplice wrapper per API: Pluto sfrutta le caratteristiche uniche di Haiku — il filesystem con attributi estesi (BFS), le query in tempo reale, lo scripting del desktop e il sistema di messaggi tra applicazioni — per creare un'esperienza AI profondamente integrata nel sistema operativo.

---

## Le tre interfacce

### 1. Pluto CLI — Terminale interattivo
Un REPL completo con editing della riga di comando (libedit), cronologia persistente e streaming in tempo reale dei token. Il testo generato dall'AI appare carattere per carattere, come una conversazione dal vivo.

### 2. Pluto GUI — Applicazione nativa con finestre
Interfaccia grafica costruita con le API native Be (BWindow, BView, BSplitView):
- **Conversazioni a schede** — fino a 10 chat simultanee, ognuna con il proprio thread
- **Rendering Markdown** — titoli, grassetto, corsivo, blocchi di codice con syntax highlighting per C++, Python, Shell e JavaScript
- **Anteprima immagini** — trascina un'immagine nella finestra per farla analizzare dall'AI
- **Barra del contesto** — indicatore colorato (verde/giallo/arancione/rosso) dell'utilizzo della finestra di contesto
- **Mini-mappa** — panoramica della conversazione con indicatore della posizione
- **Blocchi strumenti collassabili** — output degli strumenti espandibili con un click
- **Tema adattivo** — segue automaticamente il tema chiaro/scuro di sistema
- **Diff inline** — visualizzazione side-by-side rosso/verde delle modifiche ai file

### 3. Ask Pluto — Add-on per Tracker
Click destro su qualsiasi file nel file manager Tracker, seleziona "Ask pluto": si apre un terminale con il file gia allegato, pronto per essere analizzato dall'AI. Perfetto per revisioni di codice, spiegazioni o conversioni rapide.

---

## Provider AI supportati

Pluto non e legato a un singolo fornitore. Supporta nativamente:

| Provider | Protocollo |
|----------|-----------|
| **Anthropic** (Claude) | API nativa con SSE |
| **OpenAI** (GPT) | OpenAI-compatible |
| **Google** (Gemini) | OpenAI-compatible |
| **OpenRouter** | OpenAI-compatible |
| **Ollama** (locale) | OpenAI-compatible |
| **LM Studio** (locale) | OpenAI-compatible |
| **Cerebras** | OpenAI-compatible |
| **Groq** | OpenAI-compatible |
| **DeepSeek** | OpenAI-compatible |
| **Fireworks** | OpenAI-compatible |
| **Perplexity** | OpenAI-compatible |
| **Together** | OpenAI-compatible |

Qualsiasi server compatibile con l'API OpenAI puo essere aggiunto tramite profili personalizzati. I modelli locali (Ollama, LM Studio) funzionano completamente offline.

---

## 14 strumenti integrati

Pluto non si limita a chattare: puo agire sul sistema. L'AI ha accesso a 14 strumenti, di cui 8 esclusivi per Haiku:

### Strumenti generici
- **read_file** / **write_file** / **edit_file** — lettura, scrittura e modifica di file con diff visuale
- **bash** — esecuzione di comandi shell in sandbox
- **grep** / **glob** — ricerca nel contenuto e nei nomi dei file

### Strumenti nativi Haiku
- **read_attr** / **write_attr** / **list_attrs** / **remove_attr** — manipolazione degli attributi estesi BFS (stringhe, interi, float, booleani)
- **query_fs** — query sul filesystem con predicati completi (`==`, `!=`, `<`, `>`, `&&`, `||`, wildcard), con supporto per volumi specifici
- **live_query** — monitoraggio in tempo reale dei file tramite BQuery live
- **desktop_script** — invio di BMessage alle applicazioni in esecuzione per leggere/modificare le loro proprieta
- **clipboard** — lettura e scrittura degli appunti di sistema
- **beapi_lookup** — consultazione della documentazione BeAPI on-demand

Questo significa che l'AI puo, ad esempio, cercare tutti i file con un certo attributo, modificare le proprieta di un'applicazione in esecuzione, o monitorare una cartella per nuovi file — tutto tramite linguaggio naturale.

---

## Knowledge base BeAPI integrata

Pluto include un database di 390 snippet di codice che coprono 202 classi delle Be API. Quando l'utente menziona una classe come BWindow o BQuery, gli snippet rilevanti vengono iniettati automaticamente nel contesto, rendendo l'AI immediatamente competente sulle API di Haiku senza bisogno di documentazione esterna.

Gli utenti possono aggiungere i propri snippet personalizzati e il database e ricercabile direttamente da Tracker tramite attributi BFS indicizzati.

---

## MCP — Model Context Protocol

Pluto implementa il protocollo MCP con tre trasporti (HTTP, SSE, stdio), permettendo di collegare server esterni che espongono strumenti aggiuntivi. I server MCP possono essere configurati a tre livelli — locale (per progetto), progetto e utente — con approvazione di sicurezza al primo utilizzo e protezione SSRF.

---

## Automazione e workflow

### Routine
Workflow ricorrenti con template (`{{date}}`, `{{clipboard}}`, `{{changed_files}}`), attivabili manualmente, all'avvio, alla modifica di file o periodicamente. Integrate con il launch_daemon di Haiku.

### Schedule
Esecuzione periodica o basata su eventi tramite launch_daemon, per agenti AI che lavorano in background.

### Agent Store
Gestore di pacchetti per agenti AI: installazione da percorsi locali o repository Git, con revisione di sicurezza automatica.

### Comandi personalizzati
Comandi slash definiti dall'utente in file Markdown con sostituzione di variabili.

### Hook system
Hook pre/post esecuzione strumenti e al prompt utente per automazione tramite shell script.

### Skill system
Competenze attivabili automaticamente tramite parole chiave nel prompt, sempre attive o on-demand.

---

## Comunicazione tra istanze

Piu istanze di Pluto possono comunicare tra loro: il comando `/peers` elenca le istanze attive, `/ask` invia prompt da un'istanza all'altra. Utile per workflow distribuiti o per delegare compiti specifici.

---

## Sessioni persistenti

Le conversazioni sono salvate in formato JSONL con attributi BFS indicizzati (`pluto:title`, `pluto:model`, `pluto:tags`, `pluto:updated`). Questo significa che le sessioni sono ricercabili direttamente dalla finestra Trova di Tracker, come qualsiasi altro file su Haiku.

---

## Sicurezza

La versione 0.5.0 include un audit di sicurezza completo con 34 bug risolti:
- Prevenzione path traversal nel sandbox del workspace
- Protezione da injection di comandi shell
- Prevenzione SSRF nel trasporto MCP
- Scritture atomiche per configurazione, chiavi API e token OAuth
- Trust per workspace con log di audit
- Autenticazione OAuth 2.0 + PKCE per account Claude Max/Pro

---

## Numeri

| Metrica | Valore |
|---------|--------|
| Righe di codice | ~28.000 |
| File sorgente | 149 |
| Suite di test | 21 (~250 test case) |
| Dimensione CLI | ~1.2 MB |
| Dimensione GUI | ~2.3 MB |
| Dimensione add-on Tracker | ~18 KB |
| Memoria a riposo (CLI) | ~50 MB |
| Memoria a riposo (GUI) | ~80 MB |
| Tempo di avvio CLI | < 100 ms |
| Tempo di compilazione | < 5 secondi |
| Provider AI supportati | 12+ |
| Strumenti integrati | 14 |
| Snippet BeAPI | 390 |

---

## Dipendenze

Pluto ha dipendenze minime, tutte gia presenti su Haiku:
- **libbe** — API native di Haiku
- **libcurl** — client HTTP
- **libcrypto / libssl** — crittografia per OAuth
- **libedit** — editing della riga di comando
- **md4c** — parser Markdown (compilato come C)

Non richiede runtime esterni, interpreti o framework. Si compila in meno di 5 secondi.

---

## Installazione

Disponibile come pacchetto `.hpkg` per HaikuDepot o come archivio zip con script di installazione. La recipe per HaikuPorter e inclusa nel repository.

---

## Perche Pluto e significativo

Pluto dimostra che Haiku OS e una piattaforma vitale per lo sviluppo di applicazioni moderne. In un panorama dominato da client AI basati su Electron che consumano centinaia di megabyte, Pluto offre un'alternativa radicale: un binario nativo di poco piu di un megabyte che si integra profondamente con il sistema operativo, sfruttando caratteristiche uniche come gli attributi estesi del filesystem e lo scripting del desktop che non hanno equivalenti su altri sistemi operativi.

E anche una dimostrazione di come il C++17, usato con le API Be, possa produrre software moderno, sicuro e manutenibile — senza i compromessi tipici delle soluzioni multipiattaforma.

---

## Screenshot guide — prompts consigliati

Di seguito i prompt da usare in Pluto per ottenere ciascuno degli 11 screenshot consigliati per l'articolo.

### 1. GUI — streaming conversation with markdown rendering

```
Write a C++ function for Haiku that creates a window with a BListView
and populates the list with files from the current directory. Comment the code
and briefly explain how it works.
```

Capture while the response is still streaming to show the animated cursor and context bar.

### 2. GUI — image analysis (multimodal)

Drag a Haiku desktop screenshot into the window, then type:

```
Describe what you see in this screenshot. What operating system is this?
Which applications are open? Do you notice anything interesting about the interface design?
```

### 3. Tracker add-on "Ask pluto"

Right-click `src/main.cpp` in Tracker, select "Ask pluto", then type:

```
Give me a summary of this file: how many lines it has, what it does,
which headers it includes, and what the main functions are.
```

Two screenshots: Tracker with the context menu open + the Terminal with the response.

### 4. CLI — REPL session with edit_file diff

```
Read src/main.cpp and add an MIT copyright comment at the top of the file
with year 2026 and author "Pluto contributors".
```

This forces `read_file` + `edit_file`, producing a red/green diff in the terminal.

### 5. BFS tools — filesystem query by attributes

```
Search the filesystem for all files of type "audio/mpeg" larger than 5MB.
Then take the first result and show me all its extended attributes.
```

Shows `query_fs` + `list_attrs` + `read_attr` — unique to Haiku, no other AI client can do this.

### 6. Desktop scripting

Open StyledEdit with a text file first, then in Pluto:

```
List all running applications on the desktop.
Then take StyledEdit and tell me what properties it supports via scripting.
```

Alternative for more visual impact:

```
Use desktop scripting to find out the window title of StyledEdit
and change the clipboard text to "Hello from Pluto AI!".
```

### 7. Sessions in Tracker Find

First save 3-4 conversations with these short prompts:

```
Explain what a BMessenger is in three lines.
```
```
How do you compile a Haiku project with a Makefile?
```
```
What is the difference between BWindow and BDirectWindow?
```

Then open Tracker Find (Alt+F) and search by `pluto:title` attribute. The screenshot is of Tracker's Find window, not Pluto.

### 8. Settings dialog

No prompt needed. Open the settings dialog with 2-3 providers configured (e.g. Anthropic + Ollama + OpenRouter) so the provider list looks populated.

### 9. Tabbed conversations

Open 3-4 tabs (Cmd+T) with different prompts:

**Tab 1**:
```
Write a haiku about programming.
```

**Tab 2**:
```
What is BFS and why is it unique?
```

**Tab 3**:
```
List 5 useful Haiku terminal commands.
```

**Tab 4**:
```
Write a C++ struct to represent an RGBA color.
```

### 10. Deskbar replicant

No prompt needed. Take a close-up screenshot of the Deskbar shelf area with the Pluto icon visible alongside other system icons.

### 11. Collapsible tool blocks (expanded vs collapsed)

```
Read the Makefile and tell me how many build targets there are.
Then read src/config.h and tell me how many configuration options are defined.
```

Click the arrow on one tool block to collapse it before taking the screenshot, so the contrast between expanded and collapsed is visible.

---

### Screenshot tips

- Use the **light theme** — renders better in print and on white article backgrounds
- Resolution at least **1920x1080**
- For the GUI, size the window to about **3/4 of the screen**
- For the CLI, use a readable font size **(14-16pt)**
- Wait for the response to **complete** before capturing, except screenshot 1 (streaming)

---

**Repository**: [github — da confermare con l'autore]
**Autore**: [da confermare con l'autore]
**Contatto**: [da confermare con l'autore]
