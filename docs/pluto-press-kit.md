# Pluto — Native AI Assistant for Haiku OS

**Version**: 0.6.0 | **License**: MIT | **Language**: C++17 | **Platform**: Haiku R1/beta5+

---

## What is Pluto

Pluto is the first native AI client for Haiku OS. Written entirely in C++17
with no heavyweight dependencies (no Electron, Node.js, or Python), it
provides direct access to Claude, GPT, Gemini, and many other language
models through a command-line interface, a native GUI application, and a
Tracker file manager add-on.

It is not a simple API wrapper: Pluto leverages Haiku's unique features —
extended-attribute filesystem (BFS), real-time queries, desktop scripting,
and inter-application messaging — to create an AI experience deeply
integrated into the operating system.

---

## Three Interfaces

### 1. Pluto CLI — Interactive Terminal
A full REPL with line editing (libedit), persistent history, and real-time
token streaming. AI-generated text appears character by character, like a
live conversation.

### 2. Pluto GUI — Native Windowed Application
A graphical interface built with native Be APIs (BWindow, BView, BSplitView):
- **Tabbed conversations** — up to 10 simultaneous chats, each with its own thread
- **Markdown rendering** — headings, bold, italic, code blocks with syntax highlighting for C++, Python, Shell, and JavaScript
- **Image preview** — drag an image into the window to have the AI analyze it
- **Context gauge** — color-coded bar (green/yellow/orange/red) showing context window usage
- **Mini-map** — conversation overview with position indicator
- **Collapsible tool blocks** — expandable tool output with a single click
- **Adaptive theming** — automatically follows the system light/dark theme
- **Inline diffs** — red/green side-by-side display of file modifications

### 3. Ask Pluto — Tracker Add-on
Right-click any file in the Tracker file manager, select "Ask pluto": a
terminal opens with the file already attached, ready for AI analysis.
Perfect for code reviews, explanations, or quick conversions.

---

## Supported AI Providers

Pluto is not tied to a single vendor. It natively supports 15+ providers:

| Provider | Protocol |
|----------|----------|
| **Anthropic** (Claude) | Native API with SSE |
| **OpenAI** (GPT) | OpenAI-compatible |
| **Google** (Gemini) | OpenAI-compatible |
| **OpenRouter** | OpenAI-compatible |
| **Ollama** (local) | OpenAI-compatible |
| **LM Studio** (local) | OpenAI-compatible |
| **Cerebras** | OpenAI-compatible |
| **Groq** | OpenAI-compatible |
| **DeepSeek** | OpenAI-compatible |
| **Mistral AI** | OpenAI-compatible |
| **Together AI** | OpenAI-compatible |
| **Hugging Face** | OpenAI-compatible |
| **Azure OpenAI** | OpenAI-compatible |
| **AWS Bedrock** | OpenAI-compatible |
| **GitHub Copilot** | OpenAI-compatible |

Any OpenAI-compatible server can be added via custom profiles. Local models
(Ollama, LM Studio) work completely offline.

---

## 23 Built-in Tools

Pluto does not just chat: it can act on the system. The AI has access to
23 tools, including 11 exclusive to Haiku:

### General Tools
- **read_file** / **write_file** / **edit_file** / **apply_patch** — file I/O with visual diffs
- **bash** — sandboxed shell command execution
- **grep** / **glob** — content and filename search
- **web_fetch** / **web_search** — web content retrieval
- **wiki_lookup** — project knowledge base search
- **lsp** — code intelligence via clangd/pylsp/gopls

### Haiku-Native Tools
- **read_attr** / **write_attr** / **list_attrs** / **remove_attr** — BFS extended attribute manipulation
- **query_fs** — filesystem queries with full predicate syntax
- **live_query** — real-time file monitoring via BQuery
- **desktop_script** — send BMessage to running applications
- **clipboard** — system clipboard read/write
- **node_monitor** — directory change watching
- **beapi_lookup** — on-demand BeAPI documentation lookup

This means the AI can search for files by attribute, modify properties of
running applications, or monitor a folder for new files — all through
natural language.

---

## Built-in BeAPI Knowledge Base

Pluto includes a database of 390 code snippets covering 202 Be API classes.
When the user mentions a class like BWindow or BQuery, relevant snippets are
automatically injected into the context, making the AI immediately
knowledgeable about Haiku's APIs without external documentation.

Users can add their own custom snippets, and the database is searchable
directly from Tracker via indexed BFS attributes.

---

## MCP — Model Context Protocol

Pluto implements the MCP protocol with three transports (HTTP, SSE, stdio),
allowing external servers to expose additional tools. MCP servers can be
configured at three levels — local (per-project), project, and user — with
security approval on first use and SSRF protection.

---

## Automation and Workflows

- **Routines** — recurring workflows with templates, triggers (manual, on_boot, on_file_change, periodic), integrated with Haiku's launch_daemon
- **Schedules** — periodic or event-based agent execution via launch_daemon
- **Agent Store** — package manager for AI agents: install from local paths or Git repos with automatic security review
- **Custom commands** — user-defined slash commands in Markdown files with variable substitution
- **Hook system** — pre/post tool execution and user prompt hooks via shell scripts
- **Skill system** — keyword-triggered or always-on competencies injected into the system prompt

---

## Multi-Instance Communication

Multiple Pluto instances can communicate: `/peers` lists active instances,
`/ask` sends prompts from one instance to another. Useful for distributed
workflows or delegating specific tasks.

---

## Persistent Sessions

Conversations are saved in JSONL format with indexed BFS attributes
(`pluto:title`, `pluto:model`, `pluto:tags`, `pluto:updated`). This means
sessions are searchable directly from Tracker's Find window, like any other
file on Haiku.

---

## Security

Version 0.6.0 includes a comprehensive security audit with 34 bugs fixed:
- Path traversal prevention in workspace sandbox
- Shell injection protection in all shell-out paths
- SSRF prevention in MCP transport
- Atomic writes for configuration, API keys, and OAuth tokens
- Workspace trust with audit logging
- OAuth 2.0 + PKCE authentication for Claude Max/Pro accounts

---

## By the Numbers

| Metric | Value |
|--------|-------|
| Lines of code | ~33,000 |
| Source files | 149 |
| Test suites | 21 (~250 test cases) |
| CLI binary size | ~1.5 MB |
| GUI binary size | ~2.7 MB |
| Tracker add-on size | ~18 KB |
| CLI idle memory | ~50 MB |
| GUI idle memory | ~80 MB |
| CLI startup time | < 100 ms |
| Build time | < 5 seconds |
| Supported AI providers | 15+ |
| Built-in tools | 23 |
| BeAPI snippets | 390 |

---

## Dependencies

Pluto has minimal dependencies, all already present on Haiku:
- **libbe** — Haiku native APIs
- **libcurl** — HTTP client
- **libcrypto / libssl** — cryptography for OAuth
- **libedit** — command-line editing
- **md4c** — Markdown parser (compiled as C)

No external runtimes, interpreters, or frameworks required. Builds in under
5 seconds.

---

## Installation

Available as an `.hpkg` package for HaikuDepot or as a zip archive with an
install script. The HaikuPorts recipe is included in the repository.

---

## Why Pluto Matters

Pluto demonstrates that Haiku OS is a viable platform for modern application
development. In a landscape dominated by Electron-based AI clients consuming
hundreds of megabytes, Pluto offers a radical alternative: a native binary
of just over a megabyte that integrates deeply with the operating system,
leveraging unique features like extended filesystem attributes and desktop
scripting that have no equivalents on other operating systems.

It is also a demonstration of how C++17, used with the Be APIs, can produce
modern, secure, and maintainable software — without the compromises typical
of cross-platform solutions.

---

**Repository**: https://codeberg.org/atomozero/Pluto
**License**: MIT
