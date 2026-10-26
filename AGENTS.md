# Pluto — Agent Instructions

## Build & Test Commands
```bash
make                    # all three binaries (CLI + GUI + Tracker add-on + System One)
make Pluto_cli          # CLI only
make Pluto              # GUI only
make build-addon        # Tracker add-on only
make systemone_server   # System One MCP server only
make check              # run all 21 test suites
make bench              # performance benchmarks
make ci                 # clean build + all tests (CI pipeline)
make package            # build .hpkg package
make clean              # remove all build artifacts
```

## Project Structure
- **Four binaries, shared core** (~40 compilation units):
  - `Pluto_cli` — streaming REPL (`src/main.cpp`, `src/repl.cpp`)
  - `Pluto` — native Haiku GUI (`src/gui/*`)
  - `Ask pluto` — Tracker add-on (`src/haiku/TrackerAddOn.cpp`)
  - `systemone_server` — System One MCP server (`src/mcp/systemone_server.cpp`)
- **Core modules** (`src/`): `agent/Loop`, `provider/`, `tools/`, `haiku/`, `mcp/`, `storage/`, `ipc/`, `config`, `rag`, `wiki`, `routine`, `schedule`
- **Tests**: 21 self-contained test binaries in `tests/` (no framework)

## Platform & Dependencies
- **Haiku-only** (R1/beta5+). Will not build/run on Linux/macOS/Windows.
- Requires: `gcc`, `libcurl`, `libedit`, `libbe` (Haiku kernel API)
- Build uses Haiku private headers: `-I/boot/system/develop/headers/private/...`

## Key Conventions
- **Config**: `~/config/settings/pluto/config` (provider, model, tokens, cache)
- **API keys**: `getenv(ANTHROPIC_API_KEY)` or `~/config/settings/pluto-api-key` (chmod 600)
- **Per-provider keys**: `~/config/settings/pluto/keys/<ENV_VAR_NAME>`
- **Slash commands** (GUI & CLI): `/model`, `/tools`, `/cost`, `/compact`, `/init`, `/wiki`, `/plan`, `/routine`, `/reset`, `/help`, `/systemone`
- **Custom commands**: `~/config/settings/pluto/commands/*.md`
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

## Gotchas
- Resource file `pluto.rsrc` built from `pluto.rdef` + `pluto.hvif` (icon) via `rc`/`xres`/`resattr`/`addattr`
- Tracker add-on name has a space: `Ask pluto` — quoted in Makefile
- Config persistence sanitizes newlines to prevent injection; API key written atomically (temp file + rename)
- Cancellation: atomic flag + SIGINT handler (`SA_RESTART`), polled in curl progress callback
- **API key resolution order**: env var → per-provider key file (`~/config/settings/pluto/keys/`) → legacy file
- Anthropic provider skips auth header when token empty (allows local-compat models)
- Auto-detect provider preserves user-set remote base_url for open-weight models
- **System One** auto-registers as built-in MCP server via stdio transport (no config needed)
