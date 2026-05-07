---
name: git-commit-helper
schedule: manual
tools: bash, read_file, grep
notify: false
enabled: true
---
Today is {{date}}.

Help me prepare a git commit for the current changes.

1. Run `git status` to see what's modified/staged.
2. Run `git diff --staged` (if anything is staged) or `git diff` to see changes.
3. Analyze the changes and write a commit message following this format:

```
type(scope): concise description

- Detail 1
- Detail 2
```

Types: feat, fix, refactor, style, docs, test, chore

4. Show me the proposed commit message and ask for confirmation.
5. If I confirm, run `git add` for the relevant files and `git commit`.

Rules:
- Never include generated files, .o, binaries, or .pluto/ in the commit.
- If changes span multiple concerns, suggest splitting into multiple commits.
- Keep the first line under 72 characters.
