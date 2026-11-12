# Pluto — Agent Instructions

## Build & Test Commands
```bash
make                    # all binaries on Haiku (Pluto_cli + Pluto GUI + add-on + systemone_server)
make Pluto_cli          # CLI only (works on macOS with mock headers)
make Pluto              # GUI only (Haiku only)
make build-addon        # Tracker add-on only (Haiku only)
make systemone_server   # System One MCP server only
make check              # run all 21 test suites (Haiku only - requires libbe)
make bench              # performance benchmarks
make ci                 # clean build + all tests (CI pipeline)
make package            # build .hpkg package
make clean              # remove all build artifacts
```

## Platform & Cross-Compilation
- **Haiku-only runtime** (R1/beta5+). Will not build/run on Linux/macOS/Windows natively.
- **macOS cross-compilation**: The Makefile detects `uname -s`. On macOS/Linux it uses mock headers (`mock_headers/haiku`) and links `mock_libs` so `Pluto_cli` and `systemone_server` can compile. GUI (`Pluto`) and Tracker add-on (`Ask pluto`) are NOT built on macOS.
- Requires on Haiku: `gcc`, `libcurl`, `libedit`, `libbe` (Haiku kernel API)
- Build uses Haiku private headers: `-I/boot/system/develop/headers/private/...`
- On macOS: `g++ -std=c++17` with `mock_headers/override` and `mock_headers/haiku`

## Important Build Quirks
1. **`src/mcp/SseTransport.h` includes `../provider/CurlUtil.h`** (not `CurlUtil.h`) — the file lives in `src/provider/`. Do not change this back to `"CurlUtil.h"`.
2. **`SseTransport::Config` cannot use default parameter syntax** (`Config config = {}` or `Config{}` as default arg) on macOS due to aggregate-init limitations in the mock toolchain. The constructor takes `Config config` (no default). Call sites must pass `SseTransport::Config{}` explicitly (see `src/mcp/Transport.cpp:15`).
3. **`CircuitBreaker::mutex` must be `mutable`** because `state()` is `const` and locks it.
4. **CURLCode `curl_slist_append` can return NULL** — always check for OOM before using the returned list.

## Project Structure
- **Four binaries, shared core** (~40 compilation units):
  - `Pluto_cli` — streaming REPL (`src/main.cpp`, `src/repl.cpp`)
  - `Pluto` — native Haiku GUI (`src/gui/*`)
  - `Ask pluto` — Tracker add-on (`src/haiku/TrackerAddOn.cpp`) — name has a space, quoted in Makefile
  - `systemone_server` — System One MCP server (`src/mcp/systemone_server.cpp`)
- **Core modules** (`src/`): `agent/Loop`, `provider/`, `tools/`, `haiku/`, `mcp/`, `storage/`, `ipc/`, `config`, `rag`, `wiki`, `routine`, `schedule`
- **Tests**: 21 self-contained test binaries in `tests/` (no framework)

## Key Conventions
- **Config**: `~/config/settings/pluto/config` (provider, model, tokens, cache)
- **API keys**: `getenv(ANTHROPIC_API_KEY)` or `~/config/settings/pluto-api-key` (chmod 600)
- **Per-provider keys**: `~/config/settings/pluto/keys/<ENV_VAR_NAME>`
- **Slash commands** (GUI & CLI): `/model`, `/tools`, `/cost`, `/compact`, `/init`, `/wiki`, `/plan`, `/routine`, `/reset`, `/help`, `/systemone`
- **Custom commands**: `~/config/settings/pluto/commands/*.md` with `{{args}}` substitution
- **Sessions**: JSONL with BFS attributes (`pluto:title`, `pluto:model`, `pluto:tags`) — searchable in Tracker Find
- **Plan mode**: filters out write tools, injects `<mode>plan</mode>` system directive

## System One (Laya-based Decision Model)
- **Built-in MCP server**: `systemone_server` (stdio transport, auto-discovered)
- **9 tools exposed** to the LLM:
  - `systemone_classify` — single-label classification (routing, triage)
  - `systemone_classify_multi` — multi-label classification
  - `systemone_verify` — entailment check (ground RAG, validate summaries)
  - `systemone_guard` — safety gate (prompt injection, PII, toxic, spam)
  - `systemone_select_relevant` — context pruning (90% recall, 80% precision)
  - `systemone_same_intent` — semantic dedup (94% accuracy)
  - `systemone_triage` — guard + route + tags + urgency (saves 3-4 calls)
  - `systemone_answerable` — RAG gate (90% at threshold 0.3)
  - `systemone_consult` — decisions with persistent memory
  - `systemone_decide_program` — full pipeline (token-efficient)
- **Token savings**: 5-10x fewer tokens by delegating classification/verification to System One
- **Memory**: Persistent topic memory (`remember`/`recall`/`consult`) for rules/corrections

## Testing Notes
- `make check` builds and runs all test binaries sequentially; exits on first failure
- Test binaries are minimal, self-contained, linked against core objects + `-lbe`
- No test framework — each `test_*.cpp` is a standalone `main()`
- Run single test: `make test_json && ./test_json`
- **Tests require Haiku libbe** — they will not link/run on macOS/Linux

## Common Tasks
```bash
# Quick iteration on CLI
make Pluto_cli && ./Pluto_cli --one-shot "test query"

# Full validation before commit
make ci

# Install locally (from build dir)
./install.sh

# Run GUI
./Pluto

# Build System One server
make systemone_server
```

## Architecture Notes
- Agent loop (`src/agent/Loop`) is UI-agnostic; `UiCallbacks` abstracts output (`on_text`, `on_tool_start`, etc.)
- Providers: `AnthropicProvider` (SSE streaming) + `OpenAICompatProvider` (Ollama, OpenRouter, LM Studio, etc.)
- MCP client: HTTP/SSE/stdio transports, per-project/user/local scopes, SSRF protection on SSE endpoints
- Haiku-specific tools: `read_attr`, `write_attr`, `query_fs`, `desktop_script`, `clipboard`, `node_monitor` (require `libbe`)
- **Token tracking**: Per-tab token counters (`fTokensIn`/`fTokensOut`) with live context gauge in status bar
- **Cost estimation**: Multi-provider pricing (Anthropic, OpenAI, Google, DeepSeek, Groq, local models free)
- **Auto-compact**: Triggers at 90% context window; manual `/compact` command available
- **OAuth + API key fallback**: OAuth refresh failure falls back to API key with warning
- **System One delegation**: Main agent calls `systemone_*` tools for classification, verification, triage — saves 80%+ tokens on routing/guard tasks
- **System One** auto-registers as built-in MCP server via stdio transport (no config needed)

## Gotchas
- Resource file `pluto.rsrc` built from `pluto.rdef` + `pluto.hvif` (icon) via `rc`/`xres`/`resattr`/`addattr`
- Tracker add-on name has a space: `Ask pluto` — quoted in Makefile
- Config persistence sanitizes newlines to prevent injection; API key written atomically (temp file + rename)
- Cancellation: atomic flag + SIGINT handler (`SA_RESTART`), polled in curl progress callback
- **API key resolution order**: env var → per-provider key file (`~/config/settings/pluto/keys/`) → legacy file
- Anthropic provider skips auth header when token empty (allows local-compat models)
- Auto-detect provider preserves user-set remote base_url for open-weight models
- **System One** auto-registers as built-in MCP server via stdio transport (no config needed)

## Recent Changes (context for this session)
- Fixed inverted SSRF logic in SseTransport.cpp (was allowing private IPs, rejecting public)
- Fixed StdioTransport memory leak on execvp failure
- Added mutex for OAuth token refresh race condition
- Added JSON validation for tool inputs in OpenAICompat.cpp
- Added thread-safe request ID generation with overflow protection in MCP Client
- Added 64KB request size limit in systemone_server.cpp
- Fixed custom command template injection in repl.cpp (arg sanitization)
- Enhanced HTTP providers with retry policy, circuit breaker, and request/response logging
- Added BPE-style token estimation (src/token_estimator.h)
- Enhanced inline image/document rendering in GUI (zoom, pan, context menus)
- Enhanced tab handling: pinning, quick switch dialog (Cmd+Shift+T), state persistence, drag-and-drop reordering