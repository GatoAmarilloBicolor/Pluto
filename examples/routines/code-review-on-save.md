---
name: code-review-on-save
schedule: on_file_change .
tools: read_file, grep, bash
notify: true
enabled: false
---
The following files were just modified: {{changed_files}}

Review each file for:
- Obvious bugs (null pointer, off-by-one, resource leak)
- Missing error handling
- Style inconsistencies with the surrounding code
- Security issues (injection, buffer overflow, hardcoded secrets)

For each issue found, report:
- File and approximate location
- What's wrong
- Suggested fix (one line)

If everything looks good, just say "No issues found."
Keep the review under 30 lines.
