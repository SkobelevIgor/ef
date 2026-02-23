- If something from user's input is not clear, switch to planning mode and intervew user with questions before implementation.
- Once you finish any changes, delete old binary and rebuild binary with `go build -o ef .` and move binary to ignore directory.

## Active Technologies
- Go 1.x (existing project) + github.com/gdamore/tcell/v2 (existing) (001-search-mode)
- N/A (in-memory buffer) (001-search-mode)
- Go 1.21+ + cell/v2 (terminal rendering) (001-search-mode)
- In-memory (file content in Buffer.Lines) (001-search-mode)
- Go 1.21+ (existing project) + github.com/gdamore/tcell/v2 (existing) (002-autocomplete)
- N/A (in-memory buffer - `Buffer.Lines`) (002-autocomplete)
- Go 1.21+ + github.com/gdamore/tcell/v2 (existing) (003-marks-navigation)
- N/A (in-memory per-buffer marks, non-persistent) (003-marks-navigation)
- N/A (in-memory global registry) (004-global-marks)
- Go 1.21+ + github.com/gdamore/tcell/v2 (terminal rendering) (005-multi-file-splits)
- In-memory buffers with file persistence via existing Save/Load (006-shared-buffer)
- Python 3.x (script, not compiled) + Python standard library only (os, sys, shutil, tempfile, importlib, uuid) (007-e2e-runner)
- N/A (filesystem-only temp directories) (007-e2e-runner)

## E2E Scenario Implementation Notes (pexpect paste simulation)

### Autocomplete consuming Enter
- ef's autocomplete intercepts Enter when active (prefix >= 2 chars matching another word in buffer). Instead of inserting a newline, Enter accepts the suggestion.
- To dismiss autocomplete without accepting, send Left+Right arrow keys: `\x1b[D\x1b[C`. This clears `e.inputState.Autocomplete` while staying in insert mode.
- After dismissing, the next `\r` correctly inserts a newline.

### Auto-indentation after Enter
- `InsertNewlineWithIndent()` copies leading whitespace from the current line to the new line.
- After each `\r`, send backspaces (`\x08`) equal to the number of leading whitespace characters of the line just typed to clear the auto-indent before typing the next line.
- The `o` command also copies indent from the current line — clear it with backspaces before typing.

### Newline handling
- Use `\r` (0x0D) for Enter, NOT `\n` (0x0A). tcell maps `\r` to KeyEnter; `\n` does nothing.
- Bracketed paste is NOT supported (no `EnablePaste()` call on tcell screen).

### Tab/space handling per filetype
- `.efconfig` controls `expandtab`. Go: `expandtab: false` (tabs), Python: `expandtab: true` (spaces).
- `InsertChar()` inserts characters as-is with no conversion. If paste source uses spaces but the filetype expects tabs, convert in the scenario script before sending.
- When converting spaces to tabs: use `num_spaces // tabstop` tabs, drop remainder spaces (partial tab stops).

### General pattern for line-by-line paste
```python
for i, line in enumerate(lines):
    child.send(line)
    if i < len(lines) - 1:
        child.send("\x1b[D\x1b[C")  # dismiss autocomplete
        time.sleep(0.05)
        child.send("\r")             # newline
        time.sleep(0.05)
        indent = len(line) - len(line.lstrip()) if line.strip() else 0
        if indent > 0:
            child.send("\x08" * indent)  # clear auto-indent
            time.sleep(0.05)
```

### Key escape sequences
- F10 (save & quit): `\x1b[21~`
- Escape: `\x1b`
- Left arrow: `\x1b[D`, Right arrow: `\x1b[C`
- Backspace: `\x08`

## Recent Changes
- 001-search-mode: Added Go 1.x (existing project) + github.com/gdamore/tcell/v2 (existing)
