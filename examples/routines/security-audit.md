---
name: security-audit
schedule: manual
tools: read_file, grep, glob, bash
notify: true
enabled: true
---
Today is {{date}}.

Perform a security audit of the current project. Check for:

1. **Injection vulnerabilities**
   - `grep` for `system()`, `popen()`, `exec*()` calls without input sanitization
   - String concatenation in shell commands
   - SQL-like query construction without escaping

2. **Path traversal**
   - File operations that accept user input without canonicalization
   - Missing `realpath()` or path validation before file access

3. **Credential exposure**
   - Hardcoded API keys, passwords, tokens in source code
   - Secrets in comments or debug output
   - Keys written to world-readable files

4. **Buffer issues**
   - Fixed-size buffers with unchecked input (`char buf[N]` + `sprintf`)
   - Off-by-one in string operations

5. **Resource leaks**
   - `popen()` without `pclose()`
   - `new` without `delete` or smart pointer
   - File descriptors not closed on error paths

For each finding, report:
- File and line number
- Severity (critical / high / medium / low)
- Description of the vulnerability
- Suggested fix (code snippet)
