# BQuery Cookbook — predicate recipes for pluto

> Practical recipes for pluto's `query_fs` and `live_query` tools.
> Every predicate here works directly as the `predicate` argument.

---

## Quick reference

**Operators:** `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `()`

**Wildcards in strings:** `*` (any chars), `?` (single char)

**Indexed attributes** (near-zero cost): `name`, `size`, `last_modified`,
`BEOS:TYPE`, plus any custom index created with `mkindex`.

**Non-indexed attributes** work too but scan every file on the volume.

---

## 1. Find files by name

### All C++ source files
```
name == "*.cpp"
```

### All headers in a specific pattern
```
name == "Main*.h"
```

### Files with "test" anywhere in the name
```
name == "*test*"
```

---

## 2. Find files by type

### All plain text files
```
BEOS:TYPE == "text/plain"
```

### All C/C++ source code
```
BEOS:TYPE == "text/x-source-code"
```

### All images (any format)
```
BEOS:TYPE == "image/*"
```

### All PDF documents
```
BEOS:TYPE == "application/pdf"
```

### Directories only
```
BEOS:TYPE == "application/x-vnd.Be-directory"
```

---

## 3. Find files by size

### Files larger than 1 MB
```
size > 1048576
```

### Files smaller than 4 KB
```
size < 4096
```

### Empty files
```
size == 0
```

### Files between 10 KB and 100 KB
```
size > 10240 && size < 102400
```

---

## 4. Find files by date

### Modified in the last 24 hours
```
last_modified > %now-1day%
```

### Modified in the last hour
```
last_modified > %now-1hour%
```

### Modified in the last 7 days
```
last_modified > %now-7day%
```

### Modified today (since midnight)
```
last_modified > %today%
```

### Not modified in the last 30 days (stale files)
```
last_modified < %now-30day%
```

> **Note:** The `%now-Nunit%` and `%today%` placeholders are expanded by
> pluto's `query_fs` tool before passing to `BQuery`. Units: `day`, `hour`,
> `min`, `sec`.

---

## 5. Combine predicates

### Large C++ files modified recently
```
name == "*.cpp" && size > 10240 && last_modified > %now-7day%
```

### Images or PDFs
```
BEOS:TYPE == "image/*" || BEOS:TYPE == "application/pdf"
```

### Source code but not headers
```
BEOS:TYPE == "text/x-source-code" && name != "*.h"
```

### Small text files, not in trash
```
BEOS:TYPE == "text/plain" && size < 4096 && name != "*Trash*"
```

---

## 6. pluto session queries

pluto saves sessions with BFS extended attributes. These are indexed
automatically, so queries are fast.

### Find sessions by title
```
pluto:title == "*refactor*"
```

### Find sessions using a specific model
```
pluto:model == "claude-opus-4-6"
```

### Sessions updated in the last 3 days
```
pluto:updated > %now-3day%
```

### Sessions with a specific tag
```
pluto:tags == "*bug-fix*"
```

### Sessions with more than 20 turns
```
pluto:turns > 20
```

### Large sessions (lots of tokens)
```
pluto:tokens_in > 50000
```

---

## 7. Custom attribute queries

If you've created custom indexes with `mkindex`, you can query them at
near-zero cost.

### Create a custom index
```sh
mkindex -t string pluto:priority
```

### Tag a file with a custom attribute
```
write_attr path=/boot/home/project/TODO.md name=pluto:priority value=high type=string
```

### Query it
```
pluto:priority == "high"
```

### Combine with other predicates
```
pluto:priority == "high" && last_modified > %now-7day%
```

---

## 8. Live query recipes

`live_query` watches for changes matching a predicate. Use it to monitor
file system events in real time.

### Watch for new source files (10 second window)
```json
{
  "predicate": "name == \"*.cpp\"",
  "duration_secs": 10
}
```

### Watch for new images appearing on the Desktop
```json
{
  "predicate": "BEOS:TYPE == \"image/*\"",
  "duration_secs": 30
}
```

### Watch for session attribute changes
```json
{
  "predicate": "pluto:title == \"*\"",
  "duration_secs": 15
}
```

The tool returns initial matches plus any `+` (added), `-` (removed), or
`*` (modified) events during the watch window.

---

## 9. Volume-specific queries

By default, queries search the boot volume. Use the `volume` parameter to
search other mounted volumes.

### Search on a USB drive
```json
{
  "predicate": "name == \"*.jpg\"",
  "volume": "/USB Drive"
}
```

### Search on a network share
```json
{
  "predicate": "BEOS:TYPE == \"text/plain\"",
  "volume": "/Network"
}
```

---

## 10. Performance tips

1. **Always prefer indexed attributes** — `name`, `size`, `last_modified`,
   and `BEOS:TYPE` are indexed by default. Queries on these are O(log n).

2. **Create indexes for custom attributes** you query often:
   ```sh
   mkindex -t string pluto:tag
   mkindex -t int32 pluto:priority_num
   ```

3. **Use `limit` parameter** — if you only need the first few results:
   ```json
   {"predicate": "name == \"*.md\"", "limit": 10}
   ```

4. **Non-indexed attribute queries** scan every file on the volume. They
   work but can be slow on large volumes. Add an indexed attribute filter
   to narrow the scan:
   ```
   BEOS:TYPE == "text/plain" && my_custom_attr == "value"
   ```
   Here `BEOS:TYPE` is indexed and narrows the scan before checking
   `my_custom_attr`.

5. **String wildcards** — `*` at the start of a pattern (`*foo`) is slower
   than at the end (`foo*`) because it can't use the index prefix. Prefer
   prefix matches when possible.

---

## Quick copy-paste reference

| What | Predicate |
|---|---|
| All .cpp files | `name == "*.cpp"` |
| PDFs | `BEOS:TYPE == "application/pdf"` |
| Files > 1 MB | `size > 1048576` |
| Modified today | `last_modified > %today%` |
| Modified last hour | `last_modified > %now-1hour%` |
| Session by title | `pluto:title == "*keyword*"` |
| Session by model | `pluto:model == "claude-opus-4-6"` |
| Empty files | `size == 0` |
| Images | `BEOS:TYPE == "image/*"` |
| Source, not headers | `BEOS:TYPE == "text/x-source-code" && name != "*.h"` |

---

*No other OS has a query-aware filesystem. That's the point.*
