# Bug Report: media_addon_server crash in AudioMixer::Connected()

## Contesto

Pluto (client AI nativo Haiku) ha un mini player nel side panel che lancia
file audio tramite `be_roster->Launch(&ref)`. Quando l'utente clicca ▶ su
un MP3, il sistema apre MediaPlayer, che si connette all'AudioMixer, che
crasha con un Segment Violation.

## Come riprodurre

1. Aprire Pluto GUI (`Pluto`)
2. Trascinare un file MP3 sulla finestra (es. `virtual-void.mp3`)
3. Nel side panel, cliccare il bottone ▶ (Play)
4. MediaPlayer si apre ma `media_addon_server` crasha

## Crash report

```
Debug information for team /boot/system/servers/media_addon_server (188):
Haiku revision: hrev59642 Apr 23 2026 06:35:05 (x86_64)

Thread 234: Audio Mixer control
  state: Exception (Segment violation)

Stack:
  AudioMixer::Connected() + 0x126    <-- SEGFAULT
  BBufferConsumer::HandleMessage()
  BMediaNode::WaitForMessage()
  BMediaEventLooper::ControlLoop()

Istruzione crash:
  0xfae37c5e56: movl 0x28(%rax), %esi
  rax = 0x0000000000000008  <-- puntatore invalido (NULL + offset 8)

Modulo: /boot/system/add-ons/media/mixer.media_addon
```

## Analisi preliminare

- `rax = 0x8` suggerisce che un puntatore a struct era NULL, e il codice
  sta accedendo a un campo a offset 0x28 di un oggetto ottenuto da offset
  0x28 di quel NULL → dereference di `((SomeStruct*)NULL)->field_at_0x8`
  poi `->field_at_0x28`
- Il crash è nel codice di sistema (`mixer.media_addon`), non in Pluto
- Succede quando MediaPlayer si connette all'AudioMixer per la riproduzione
- Il mixer riceve una `Connected()` callback ma il nodo interno
  (probabilmente un `MixerOutput` o `MixerChannel`) non è inizializzato

## Domande da investigare

1. Il crash è riproducibile senza Pluto? (aprire un MP3 direttamente da
   Tracker dovrebbe avere lo stesso effetto)
2. È un bug noto in hrev59642? Controllare il bug tracker Haiku:
   https://dev.haiku-os.org/search?q=AudioMixer+Connected+crash
3. Il crash dipende dal formato audio? Provare con WAV, OGG, FLAC
4. Il crash dipende dall'hardware audio? (HD Audio driver in questo caso)
5. C'è una race condition nel mixer quando un nuovo client si connette
   mentre un altro è già attivo?

## Workaround possibili per Pluto

- Verificare se `media_server` è sano prima di lanciare il player
- Usare `BMediaRoster::Sniff()` per testare il formato prima del play
- Fallback: aprire il file con `open` shell command invece di `be_roster`
- Opzione utente: "Open with..." che permette di scegliere il player

## File coinvolti

- Crash report: `/boot/home/Desktop/media_addon_server-188-debug-25-04-2026-20-02-15.report`
- Pluto player code: `src/gui/MainWindow.cpp` (kMsgPlayerPlay handler)
- File audio test: il file MP3 usato durante il test

## Prompt per sessione di debug

```
Analizza questo crash report del media_addon_server di Haiku OS.
Il crash avviene in AudioMixer::Connected() quando MediaPlayer prova
a riprodurre un file MP3 lanciato da Pluto.

Il crash report è in /boot/home/Desktop/media_addon_server-188-debug-25-04-2026-20-02-15.report

Investigare:
1. Leggi il crash report e analizza lo stack trace completo
2. Cerca nel codice sorgente di Haiku (src/add-ons/media/plugins/mixer/)
   la funzione AudioMixer::Connected() per capire cosa accede a offset 0x28
3. Verifica se il bug è noto nel tracker Haiku (dev.haiku-os.org)
4. Prova a riprodurre il crash aprendo un MP3 direttamente da Tracker
   (senza Pluto) per confermare che è un bug di sistema
5. Se è un bug Haiku, prepara una segnalazione per dev.haiku-os.org
6. Se dipende da come Pluto lancia il file, suggerisci un workaround

Il codice Pluto che lancia il player è in src/gui/MainWindow.cpp,
cerca kMsgPlayerPlay — usa be_roster->Launch(&ref) con una entry_ref.
```
