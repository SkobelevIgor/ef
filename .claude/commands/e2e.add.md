---
description: Creates a new end-to-end test case.
---

## User Input

```text
$ARGUMENTS
```
You **MUST** consider the user input before proceeding (if not empty).

There are required `Name` and `Scenario` attributes. If user didn't add them, re-ask user to add required data.

## Execution steps

### 1. Create test case folder.

In `./tests/cases` directory with name in `test-case-name` format ( space between words, lowercase ). Eg.:
`/e2e.add Name: external changes added to history. Senario: ...` should create folder `./tests/cases/external-changes-added-to-history`.

- If name contains any symbols except digits and letters, re-ask to input new `Name`;
- If such directory already exists, re-ask user to input new `Name`;

### 2. Scenario creation.
Create a file `scenario.py` with entrypoint function `run` and implement steps described in `Scenario` using `pexpect` library.
- `run` function gets only one parameter - full path to executable `ef` build.
For seed data user can mention data files with format: FILE_<name>::<full path to it> - copy them to test folder as well and use reference in your test script.
- any assets should be created in test case folder, but can be used in any way. For example, you can create a file with some content and then check if it was changed after `ef` execution.
If validation fails throw `BaseException` with message, that describes assert failure.
- Script shouldn't remove any files from test case folder, but can create new ones.

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