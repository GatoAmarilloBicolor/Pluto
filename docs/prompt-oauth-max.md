# Prompt: implementa autenticazione OAuth per piano Claude Max in pluto

## Contesto

pluto è un client Claude nativo per Haiku OS, scritto in C++17. Ha due
binari (`pluto` CLI e `pluto-gui` GUI) che condividono lo stesso core.
Oggi l'autenticazione usa solo API key diretta (env var o file).

Voglio aggiungere il supporto al piano Claude Max ($100/$200 al mese su
claude.ai) che usa OAuth 2.0 con PKCE — lo stesso flusso di Claude Code.
L'utente deve poter fare `pluto login`, autenticarsi nel browser, e poi
usare pluto normalmente senza API key.

## Specifiche tecniche OAuth (da reverse engineering di Claude Code)

### Endpoint e parametri

```
Client ID:          9d1c250a-e61b-44d9-88ed-5944d1962f5e
Authorization URL:  https://claude.com/cai/oauth/authorize
Token URL:          https://platform.claude.com/v1/oauth/token
Profile URL:        https://api.anthropic.com/api/oauth/profile
API base URL:       https://api.anthropic.com
```

### Flusso Authorization Code + PKCE

**Step 1 — Authorization request** (aperta nel browser):

```
GET https://claude.com/cai/oauth/authorize
  ?client_id=9d1c250a-e61b-44d9-88ed-5944d1962f5e
  &response_type=code
  &redirect_uri=http://localhost:{PORT}/callback
  &scope=user:inference+user:profile
  &state={RANDOM_STATE}
  &code_challenge={SHA256_BASE64URL(code_verifier)}
  &code_challenge_method=S256
  &code=true
```

`code_verifier` è una stringa random di 43-128 caratteri [A-Za-z0-9-._~].
`code_challenge` è `base64url(sha256(code_verifier))` senza padding `=`.

**Step 2 — Callback** (mini HTTP server locale riceve):

```
GET http://localhost:{PORT}/callback?code={AUTH_CODE}&state={STATE}
```

Verifica che `state` corrisponda. Estrai `code`.

**Step 3 — Token exchange**:

```
POST https://platform.claude.com/v1/oauth/token
Content-Type: application/json

{
  "grant_type": "authorization_code",
  "code": "{AUTH_CODE}",
  "client_id": "9d1c250a-e61b-44d9-88ed-5944d1962f5e",
  "code_verifier": "{CODE_VERIFIER}",
  "redirect_uri": "http://localhost:{PORT}/callback",
  "state": "{STATE}"
}
```

Risposta:

```json
{
  "access_token": "...",
  "refresh_token": "...",
  "expires_in": 3600,
  "token_type": "Bearer"
}
```

**Step 4 — Refresh** (quando `access_token` scade):

```
POST https://platform.claude.com/v1/oauth/token
Content-Type: application/json

{
  "grant_type": "refresh_token",
  "refresh_token": "{REFRESH_TOKEN}",
  "client_id": "9d1c250a-e61b-44d9-88ed-5944d1962f5e",
  "scope": "user:profile user:inference"
}
```

### Header API con OAuth (differenza rispetto ad API key)

Quando si usa OAuth, l'header di autenticazione cambia:

```
Authorization: Bearer {access_token}
anthropic-beta: oauth-2025-04-20
anthropic-version: 2023-06-01
content-type: application/json
```

Quando si usa API key (invariato):

```
x-api-key: {api_key}
anthropic-version: 2023-06-01
content-type: application/json
```

## Architettura attuale di pluto (file rilevanti)

### `src/config.h` — struttura Config

```cpp
struct Config {
    std::string provider = "anthropic";
    std::string model = "claude-opus-4-5";
    std::string base_url;
    std::string api_key_env = "ANTHROPIC_API_KEY";
    std::string system_prompt;
    int max_tokens = 4096;
    int context_window = 200000;
    bool cache = true;
    bool dry_run = false;
    std::vector<std::string> hooks_pre_tool;
    std::vector<std::string> hooks_post_tool;
    std::vector<std::string> hooks_user_prompt;
};
```

Funzioni: `load_config()`, `save_config()`, `resolve_api_key()`, `save_api_key()`.
File config: `~/config/settings/pluto` (key=value).
File API key: `~/config/settings/pluto-api-key` (chmod 600).

### `src/provider/Anthropic.h` — provider

```cpp
class AnthropicProvider : public Provider {
public:
    AnthropicProvider(std::string api_key,
                      std::string base_url = "https://api.anthropic.com");
    void stream(const CompletionRequest& req, StreamCallback cb) override;
private:
    std::string fApiKey;
    std::string fBaseUrl;
};
```

In `Anthropic.cpp`, la funzione `stream()` costruisce gli header così:

```cpp
std::string auth = "x-api-key: " + fApiKey;
headers = curl_slist_append(headers, auth.c_str());
headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
headers = curl_slist_append(headers, "content-type: application/json");
headers = curl_slist_append(headers, "accept: text/event-stream");
```

### `src/main.cpp` — entry point CLI

La risoluzione credenziali avviene così:

```cpp
std::string api_key = pluto::resolve_api_key(cfg);
if (api_key.empty()) {
    std::fprintf(stderr, "%s not set\n", cfg.api_key_env.c_str());
    return 2;
}
auto provider = make_provider(cfg, api_key);
```

La factory `make_provider` crea un `AnthropicProvider(key)`.

### `src/gui/ApiKeyDialog.h/.cpp` — dialog API key nella GUI

Dialog modale che chiede la API key e la salva in
`~/config/settings/pluto-api-key` (chmod 600).

### Build system

- `Makefile` con `CORE_SRCS`, `CLI_SRCS`, `GUI_SRCS`
- `LDLIBS = -lcurl -lbe`
- Librerie disponibili: `libcurl`, `libbe`, `libcrypto` (OpenSSL), `libssl`
- Haiku apre URL con: `be_roster->Launch(&type, msgs)` oppure `open` shell command

## Cosa implementare

### 1. Nuovo modulo `src/oauth.{h,cpp}` (~250-350 LOC)

Responsabilità:
- **PKCE helpers**: `generate_code_verifier()` (64 char random base64url),
  `generate_code_challenge(verifier)` (SHA256 + base64url, usa OpenSSL
  `EVP_Digest`), `generate_state()` (16 byte random hex)
- **`OAuthTokens` struct**: `access_token`, `refresh_token`,
  `expires_at` (time_t)
- **`oauth_tokens_path()`**: `~/config/settings/pluto/oauth-tokens`
  (chmod 600)
- **`save_oauth_tokens(tokens)`**: serializza come JSON, chmod 600
- **`load_oauth_tokens()`**: deserializza, ritorna `std::optional<OAuthTokens>`
  o tokens vuoto se file mancante
- **`delete_oauth_tokens()`**: rimuove il file (per logout)
- **`oauth_login()`**: flusso completo:
  1. Genera code_verifier, code_challenge, state
  2. Avvia un mini HTTP server su una porta libera (socket bind su 127.0.0.1:0,
     read port con `getsockname`). Deve gestire UN solo request GET
  3. Costruisci l'auth URL con tutti i parametri
  4. Stampa l'URL a terminale + tenta di aprirlo nel browser con
     `open {url}` (Haiku) come fallback
  5. Accetta la connessione, parsa il GET, estrai `code` e `state`
  6. Rispondi con una pagina HTML minimale "Login successful, close this tab"
  7. Chiudi il server socket
  8. Verifica che `state` corrisponda
  9. POST al token endpoint con curl per scambiare code → tokens
  10. Salva i token con `save_oauth_tokens`
  11. Ritorna `true` se tutto ok
- **`oauth_ensure_valid_token(tokens)`**: controlla `expires_at`, se
  scaduto (con 5 minuti di margine) fa il refresh via curl POST.
  Aggiorna il file. Ritorna access_token valido o stringa vuota su errore
- **`oauth_get_profile(access_token)`**: GET al profile endpoint,
  ritorna un struct con `display_name`, `organization_type`
  (per verificare che sia effettivamente max/pro)

Note implementative:
- Per il mini HTTP server usa socket POSIX raw (`socket`, `bind`, `listen`,
  `accept`, `read`, `write`, `close`). Non serve un framework HTTP — devi
  solo parsare una singola riga `GET /callback?code=...&state=... HTTP/1.1`
- Per SHA256 usa `#include <openssl/evp.h>` + `EVP_Digest(data, len, hash, &hlen, EVP_sha256(), NULL)`
- Per base64url: `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_`, no padding
- Per random: `#include <openssl/rand.h>` + `RAND_bytes()`
- Per le chiamate HTTP (token exchange, refresh, profile) riusa `libcurl`
  come già fatto in `Anthropic.cpp` ma con POST/GET semplici (no streaming)
- JSON parsing/serialization: usa `src/json.h` già esistente nel progetto

### 2. Modifiche a `src/config.h` e `src/config.cpp`

Aggiungi a `Config`:

```cpp
// OAuth: if non-empty, this is an OAuth session (Max/Pro plan).
// Mutually exclusive with API key auth at runtime.
std::string oauth_access_token;   // transient, not saved to config file
bool use_oauth = false;           // set at startup if oauth tokens exist
```

Aggiungi funzione `resolve_credentials()` che sostituisce `resolve_api_key()`:

```cpp
struct Credentials {
    enum Type { API_KEY, OAUTH } type = API_KEY;
    std::string token;   // API key or OAuth access token
};

Credentials resolve_credentials(const Config& cfg);
```

Logica:
1. Controlla se esiste `~/config/settings/pluto/oauth-tokens`
2. Se sì → carica, fai ensure_valid (refresh se necessario), ritorna OAUTH + access_token
3. Se no → fai il vecchio `resolve_api_key()`, ritorna API_KEY + key
4. Se entrambi vuoti → ritorna vuoto (errore)

Mantieni `resolve_api_key()` come funzione interna per backward compat.

### 3. Modifiche a `src/provider/Anthropic.h` e `src/provider/Anthropic.cpp`

Il provider deve sapere se usa OAuth o API key per generare gli header giusti.

Modifica il costruttore:

```cpp
enum class AuthType { API_KEY, OAUTH };

AnthropicProvider(std::string token, AuthType auth_type = AuthType::API_KEY,
                  std::string base_url = "https://api.anthropic.com");
```

In `stream()`, cambia la generazione header:

```cpp
struct curl_slist* headers = nullptr;
if (fAuthType == AuthType::OAUTH) {
    std::string auth = "Authorization: Bearer " + fToken;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "anthropic-beta: oauth-2025-04-20");
} else {
    std::string auth = "x-api-key: " + fToken;
    headers = curl_slist_append(headers, auth.c_str());
}
headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
headers = curl_slist_append(headers, "content-type: application/json");
headers = curl_slist_append(headers, "accept: text/event-stream");
```

### 4. Modifiche a `src/main.cpp`

Aggiungi sub-command `pluto login` e `pluto logout`:

```cpp
// Subcommand: pluto login
if (argc >= 2 && std::string(argv[1]) == "login") {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    bool ok = pluto::oauth_login();
    curl_global_cleanup();
    if (ok) {
        std::printf("Login successful! pluto will use your Claude account.\n");
        return 0;
    } else {
        std::fprintf(stderr, "Login failed.\n");
        return 1;
    }
}

// Subcommand: pluto logout
if (argc >= 2 && std::string(argv[1]) == "logout") {
    pluto::delete_oauth_tokens();
    std::printf("Logged out. pluto will use API key if set.\n");
    return 0;
}
```

Modifica la risoluzione credenziali nel main:

```cpp
auto creds = pluto::resolve_credentials(cfg);
if (creds.token.empty()) {
    std::fprintf(stderr,
        "No credentials. Use 'pluto login' for Claude Max/Pro,\n"
        "or set %s for API key access.\n", cfg.api_key_env.c_str());
    return 2;
}

auto auth_type = (creds.type == pluto::Credentials::OAUTH)
    ? pluto::AuthType::OAUTH
    : pluto::AuthType::API_KEY;

auto provider = std::make_unique<pluto::AnthropicProvider>(
    creds.token, auth_type);
```

Aggiorna `make_provider()` di conseguenza per accettare `Credentials`.

Aggiorna `usage()` per documentare `login` e `logout`.

### 5. Modifiche a `src/gui/ApiKeyDialog.{h,cpp}`

Aggiungi un bottone "Login with Claude account" sotto il campo API key.
Al click:

1. Lancia `oauth_login()` in un thread separato (come il worker thread
   della MainWindow)
2. Mostra un `BAlert` "Waiting for browser login... check your browser"
3. Al completamento, chiudi il dialog e notifica la MainWindow con un
   `BMessage` che porta le nuove credenziali

Se l'utente ha già un token OAuth salvato, il dialog lo indica con un
label "✓ Logged in as Claude Max" e un bottone "Logout".

### 6. Modifiche al `Makefile`

Aggiungi `src/oauth.cpp` a `CORE_SRCS`.
Aggiungi `-lcrypto` a `LDLIBS` (per OpenSSL SHA256 + RAND_bytes):

```makefile
LDLIBS = -lcurl -lbe -lcrypto
```

### 7. Aggiornamenti documentazione e help

In `src/main.cpp` `usage()`, aggiungi:

```
  pluto login          authenticate with Claude Max/Pro account
  pluto logout         remove saved OAuth tokens
```

Aggiungi una riga al banner del REPL quando si usa OAuth:

```
pluto (anthropic / claude-opus-4-5)  15 tools  (Claude Max)
```

### 8. Test: `tests/test_oauth.cpp`

Test unitari per le funzioni pure (non le chiamate di rete):

```cpp
// Test PKCE:
// - code_verifier ha lunghezza 64 e solo caratteri validi
// - code_challenge è base64url senza padding
// - code_challenge è deterministico dato lo stesso verifier
// - stato generato è hex di 32 char

// Test token storage:
// - save → load roundtrip
// - load da file mancante → vuoto
// - delete rimuove il file
// - file ha permessi 0600 dopo save

// Test URL parsing:
// - extract_query_param("...?code=abc&state=xyz", "code") == "abc"
// - extract_query_param("...?code=abc&state=xyz", "state") == "xyz"
// - extract_query_param("...?code=abc", "state") == ""
```

Aggiungi a `TEST_BINS` e `check` nel Makefile.

## Vincoli

- **C++17 puro**, nessuna dipendenza nuova oltre a `libcrypto` (OpenSSL,
  già disponibile su Haiku base)
- **Non rompere** il flusso API key esistente — deve continuare a
  funzionare esattamente come prima se l'utente non fa `pluto login`
- **Priorità OAuth > API key**: se esistono token OAuth validi, usali
  anche se c'è una API key configurata. L'utente può fare `pluto logout`
  per tornare alla API key
- **Thread safety**: `oauth_login()` e `oauth_ensure_valid_token()` possono
  essere chiamati da thread diversi (GUI worker). Il file token va
  letto/scritto con un lock o atomicamente (scrivi su .tmp + rename)
- **Errori chiari**: se il login fallisce, dire perché (timeout, rete,
  server error, state mismatch). Se il refresh fallisce, suggerire
  `pluto login` di nuovo
- **Timeout**: il mini HTTP server per il callback deve avere un timeout
  di 120 secondi. Se l'utente non completa il login nel browser entro
  quel tempo, stampa un errore e chiudi
- Usa `json.h` del progetto per tutto il JSON, non includere librerie esterne
- Apri il browser su Haiku con: `BRoster().Launch("text/html", 1, &argv_url)`
  oppure fallback `system("open <url>")`. Testa entrambi
- File token: `~/config/settings/pluto/oauth-tokens`, formato JSON,
  chmod 0600. Struttura:
  ```json
  {
    "access_token": "...",
    "refresh_token": "...",
    "expires_at": 1713400000
  }
  ```

## Ordine di implementazione suggerito

1. `src/oauth.{h,cpp}` — PKCE helpers + token storage (testabili subito)
2. `tests/test_oauth.cpp` + Makefile update — verifica le parti pure
3. `src/oauth.cpp` — mini HTTP server + login flow + refresh
4. `src/config.{h,cpp}` — `Credentials` + `resolve_credentials()`
5. `src/provider/Anthropic.{h,cpp}` — `AuthType` + header switch
6. `src/main.cpp` — sub-command `login`/`logout` + wiring credenziali
7. `src/gui/ApiKeyDialog.cpp` — bottone login
8. Test manuale end-to-end: `pluto login` → usa → `pluto logout` → API key funziona

## Esempio sessione attesa

```
$ pluto login
Opening browser for Claude login...
  → https://claude.com/cai/oauth/authorize?client_id=...&...
Waiting for callback (120s timeout)...
Login successful! pluto will use your Claude account.

$ pluto
pluto (anthropic / claude-opus-4-5)  15 tools  (Claude Max)  type /help or /quit
> hello
Hello! How can I help?

$ pluto logout
Logged out. pluto will use API key if set.
```
