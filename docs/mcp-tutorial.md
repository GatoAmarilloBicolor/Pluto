# Build a Haiku MCP server in 10 minutes

> This tutorial shows you how to write a minimal MCP server in C++ that
> exposes one tool to pluto. By the end you'll have a working server that
> pluto auto-discovers and Claude can call.

---

## What is MCP?

The **Model Context Protocol** (MCP) lets AI clients like pluto call tools
hosted by external servers. The protocol uses **JSON-RPC 2.0** over HTTP.

An MCP server needs to handle exactly three methods:

| Method | Purpose |
|---|---|
| `initialize` | Handshake — exchange protocol version and capabilities |
| `tools/list` | Return the list of tools the server offers |
| `tools/call` | Execute a tool and return the result |

That's it. No auth, no sessions, no complex state.

---

## The plan

We'll build a server called **timekeeper** that exposes one tool:

- **`current_time`** — returns the current date and time on Haiku

pluto will auto-discover it on `localhost:8090` and Claude will be able to
ask "What time is it?" and get a live answer from your machine.

---

## Step 1: The skeleton

Create a new directory and a single source file:

```sh
mkdir -p ~/Desktop/timekeeper
cd ~/Desktop/timekeeper
```

Create `main.cpp`:

```cpp
// timekeeper — minimal MCP server for Haiku
// Build: g++ -std=c++17 -o timekeeper main.cpp -lnetwork
// Run:   ./timekeeper    (listens on http://localhost:8090/mcp)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

// ---- JSON helpers (minimal, no library needed) ----

static std::string json_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    out += "\"";
    return out;
}

// Extract a string value for a key from a JSON object (quick & dirty).
static std::string json_get(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

// Extract the "id" field (can be number or string).
static std::string json_get_id(const std::string& json) {
    size_t pos = json.find("\"id\"");
    if (pos == std::string::npos) return "null";
    pos = json.find(':', pos + 4);
    if (pos == std::string::npos) return "null";
    // Skip whitespace.
    while (pos + 1 < json.size() && json[pos + 1] == ' ') ++pos;
    if (json[pos + 1] == '"') {
        // String id.
        size_t start = pos + 2;
        size_t end = json.find('"', start);
        return "\"" + json.substr(start, end - start) + "\"";
    }
    // Numeric id.
    size_t start = pos + 1;
    size_t end = start;
    while (end < json.size() && (json[end] >= '0' && json[end] <= '9'))
        ++end;
    return json.substr(start, end - start);
}

// ---- MCP protocol handlers ----

static std::string handle_initialize(const std::string& id) {
    return
        "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
        "\"result\":{"
            "\"protocolVersion\":\"2024-11-05\","
            "\"serverInfo\":{\"name\":\"timekeeper\",\"version\":\"1.0\"},"
            "\"capabilities\":{\"tools\":{}}"
        "}}";
}

static std::string handle_tools_list(const std::string& id) {
    return
        "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
        "\"result\":{\"tools\":["
            "{"
                "\"name\":\"current_time\","
                "\"description\":\"Get the current date and time on this Haiku machine\","
                "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}"
            "}"
        "]}}";
}

static std::string handle_tools_call(const std::string& id,
                                     const std::string& body) {
    std::string tool = json_get(body, "name");

    if (tool == "current_time") {
        std::time_t t = std::time(nullptr);
        std::tm tm;
        localtime_r(&t, &tm);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &tm);

        return
            "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
            "\"result\":{\"content\":["
                "{\"type\":\"text\",\"text\":" + json_string(buf) + "}"
            "]}}";
    }

    // Unknown tool.
    return
        "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
        "\"error\":{\"code\":-32601,"
        "\"message\":\"unknown tool: " + tool + "\"}}";
}

// Route a JSON-RPC request to the right handler.
static std::string dispatch(const std::string& body) {
    std::string method = json_get(body, "method");
    std::string id = json_get_id(body);

    if (method == "initialize")
        return handle_initialize(id);
    if (method == "notifications/initialized")
        return {};  // notification, no response
    if (method == "tools/list")
        return handle_tools_list(id);
    if (method == "tools/call")
        return handle_tools_call(id, body);

    return
        "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
        "\"error\":{\"code\":-32601,"
        "\"message\":\"unknown method\"}}";
}

// ---- HTTP server (one connection at a time, good enough) ----

static void send_response(int fd, const std::string& json) {
    std::string http =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    write(fd, http.c_str(), http.size());
}

int main() {
    const int PORT = 8090;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1 only

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::fprintf(stderr, "bind: %s\n", strerror(errno));
        return 1;
    }
    listen(srv, 4);
    std::printf("timekeeper MCP server listening on http://localhost:%d/mcp\n", PORT);

    while (true) {
        int client = accept(srv, nullptr, nullptr);
        if (client < 0) continue;

        // Read the full HTTP request (simple: read until we have the body).
        char buf[8192] = {};
        int n = read(client, buf, sizeof(buf) - 1);
        if (n <= 0) { close(client); continue; }

        std::string request(buf, n);

        // Find the body (after \r\n\r\n).
        size_t body_start = request.find("\r\n\r\n");
        if (body_start == std::string::npos) { close(client); continue; }
        std::string body = request.substr(body_start + 4);

        // Handle CORS preflight.
        if (request.substr(0, 7) == "OPTIONS") {
            std::string cors =
                "HTTP/1.1 204 No Content\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: POST\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "\r\n";
            write(client, cors.c_str(), cors.size());
            close(client);
            continue;
        }

        std::string response = dispatch(body);
        if (!response.empty())
            send_response(client, response);

        close(client);
    }
}
```

---

## Step 2: Build and run

```sh
g++ -std=c++17 -o timekeeper main.cpp -lnetwork
./timekeeper
```

You should see:

```
timekeeper MCP server listening on http://localhost:8090/mcp
```

---

## Step 3: Test manually

In another Terminal:

```sh
# Initialize
curl -s http://localhost:8090/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1"}}}'

# List tools
curl -s http://localhost:8090/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'

# Call the tool
curl -s http://localhost:8090/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"current_time","arguments":{}}}'
```

---

## Step 4: Connect to pluto

### Option A: Auto-discovery

pluto probes well-known ports at startup. Port 8090 is not in the default
list, but you can add it. Or just configure manually:

### Option B: Add to MCP config

```sh
pluto mcp add timekeeper --url http://localhost:8090/mcp --scope user
```

Or create `~/config/settings/pluto/mcp.json`:

```json
{
  "mcpServers": {
    "timekeeper": {
      "type": "http",
      "url": "http://localhost:8090/mcp"
    }
  }
}
```

### Verify

```sh
pluto mcp list
```

You should see:

```
  timekeeper   http   http://localhost:8090/mcp   (user)
```

Start pluto and you'll see in stderr:

```
[mcp/timekeeper] 1 tools
```

---

## Step 5: Use it

Start pluto and ask:

```
> What time is it on this machine?
```

Claude will call `timekeeper__current_time` and show you the live time.

---

## Adding more tools

To add a tool, you need to:

1. **Add it to `handle_tools_list`** — include a new entry in the `tools`
   array with `name`, `description`, and `inputSchema`.

2. **Handle it in `handle_tools_call`** — add a branch for your tool name,
   parse the arguments from the JSON body, execute the logic, and return a
   result with `content` array.

### Example: system uptime tool

Add to `handle_tools_list`:

```cpp
",{"
    "\"name\":\"uptime\","
    "\"description\":\"Get system uptime in seconds\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}"
"}"
```

Add to `handle_tools_call`:

```cpp
if (tool == "uptime") {
    bigtime_t us = system_time();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1f seconds", us / 1000000.0);
    return
        "{\"jsonrpc\":\"2.0\",\"id\":" + id + ","
        "\"result\":{\"content\":["
            "{\"type\":\"text\",\"text\":" + json_string(buf) + "}"
        "]}}";
}
```

---

## Input schemas

The `inputSchema` field tells the model what arguments your tool accepts.
It uses JSON Schema:

```json
{
  "type": "object",
  "properties": {
    "path": {
      "type": "string",
      "description": "File path to read"
    },
    "lines": {
      "type": "integer",
      "description": "Max lines to return (default: all)"
    }
  },
  "required": ["path"]
}
```

The model sees this schema and constructs the right JSON arguments when
calling your tool.

---

## Key points

- **Bind to 127.0.0.1** — never expose MCP servers to the network.
  pluto only connects to localhost.

- **Return `content` array** — tool results must be
  `{"content": [{"type": "text", "text": "..."}]}`. This is MCP spec.

- **Handle notifications** — `notifications/initialized` has no `id` field
  and expects no response. Don't error on it.

- **Protocol version** — use `2024-11-05`. pluto sends this in
  `initialize` and expects it back.

- **No framework needed** — as you've seen, a working MCP server is ~200
  lines of C++ with zero dependencies beyond POSIX sockets.

---

## Next steps

- Add BFS attribute tools (see Pippo's `BefsCapabilities.cpp` for
  inspiration)
- Add SSE transport for streaming events to pluto's `/watch` command
- Register your server in `.mcp.json` for per-project auto-loading
- See `docs/bquery-cookbook.md` for query predicate recipes

---

*A production of the Movement for the Liberation of Walter OS.*
