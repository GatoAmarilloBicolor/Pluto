---
name: project-status
schedule: manual
notify: false
enabled: true
---
Today is {{date}}.

Analyze the current state of this project:

1. Count source files and estimate total LOC (`find src -name '*.cpp' -o -name '*.h' | wc -l` and `cat src/**/*.cpp src/**/*.h | wc -l`)
2. Check if `make` builds without errors
3. Run `make check` and report test results
4. Look at the last 5 git commits (`git log --oneline -5`)
5. Check for any TODO/FIXME/HACK comments (`grep -rn 'TODO\|FIXME\|HACK' src/ | head -10`)

Produce a project health report card:
- Build: PASS/FAIL
- Tests: N/N passed
- Recent activity: brief summary
- Open items: count of TODOs
- Overall: healthy / needs attention / broken
