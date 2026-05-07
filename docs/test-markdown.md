# Test Markdown Renderer

## Links

Visita [il sito di Haiku](https://www.haiku-os.org) per saperne di più.
Documentazione Be API: [BeBook](https://www.haiku-os.org/docs/api/)
Link inline con testo uguale all'URL: [https://example.com](https://example.com)

## Tabella semplice

| Comando | Descrizione | Shortcut |
| --- | --- | --- |
| Save | Salva sessione | Cmd+S |
| Resume | Riprendi ultima | Cmd+R |
| Find | Cerca nel testo | Cmd+F |
| Quit | Esci | Cmd+Q |

## Tabella con numeri

| Modello | Input $/M | Output $/M |
| --- | --- | --- |
| Opus 4.5 | 15.00 | 75.00 |
| Sonnet 4.6 | 3.00 | 15.00 |
| Haiku 4.5 | 0.80 | 4.00 |

## Formattazione mista

Ecco un **testo grassetto** con *corsivo* e `codice inline`.

Un link dentro una frase: usa [pluto](https://github.com/atomozero/pluto) per chattare con Claude.

Lista con link:
- [Haiku OS](https://www.haiku-os.org) — il sistema operativo
- [HaikuPorts](https://github.com/haikuports) — i pacchetti
- [pluto](https://github.com/atomozero/pluto) — questo progetto

## Task list

- [x] Renderer markdown
- [x] Tabelle
- [x] Link
- [ ] Immagini inline
- [ ] Syntax highlighting

## Code block

```cpp
int main() {
    BApplication app("application/x-vnd.test");
    BAlert* alert = new BAlert("test", "Hello Haiku!", "OK");
    alert->Go();
    return 0;
}
```

## Blockquote con link

> pluto is a production of the [Movement for the Liberation of Walter OS](https://example.com).
> *Because every OS deserves its own AI client.*

---

Fine del test.
