---
name: haiku-app-scaffold
schedule: manual
tools: write_file, bash, beapi_lookup, read_file
notify: true
enabled: true
---
Today is {{date}}.

Create a new Haiku application scaffold. Ask me for:
1. App name (e.g. "MyEditor")
2. App signature (e.g. "application/x-vnd.Me-MyEditor")
3. Window type (single window / multi-document)
4. Features needed (menu bar, toolbar, status bar, file panels)

Then generate:

- `src/App.h` / `src/App.cpp` — BApplication subclass
- `src/MainWindow.h` / `src/MainWindow.cpp` — BWindow with BLayoutBuilder
- `src/main.cpp` — entry point
- `Makefile` — with proper Haiku flags (-lbe, -ltracker if needed)
- `app.rdef` — resource definition with app signature and version

Use `beapi_lookup` to find correct API patterns for each component.

The generated code should:
- Follow Haiku coding guidelines (UpperCamelCase, tabs, f-prefix members)
- Use BLayoutBuilder (no manual BRect positioning)
- Handle B_QUIT_REQUESTED properly
- Include a basic menu bar with File > Quit (Cmd+Q)
