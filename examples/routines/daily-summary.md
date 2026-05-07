---
name: daily-summary
schedule: manual
model: claude-sonnet-4-6
notify: true
enabled: true
---
Today is {{date}}.

Give me a brief summary of recent activity in this project directory:

1. List files modified in the last 24 hours (use `bash` with `find . -mtime -1 -type f`)
2. For each modified file, show a one-line description of what changed
3. Highlight anything that looks like a bug fix, new feature, or refactor
4. End with a short "status" paragraph (1-3 sentences)

Keep it concise — no more than 20 lines total.
