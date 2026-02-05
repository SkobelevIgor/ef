# Quickstart: Autocomplete Implementation

**Feature**: 002-autocomplete
**Date**: 2026-02-05

## Overview

This guide walks through implementing autocomplete for the FE editor. The feature adds a dropdown suggestion list when typing in Insert mode.

## File Changes Summary

| File | Action | Description |
|------|--------|-------------|
| `editor/autocomplete.go` | CREATE | New file with all autocomplete logic |
| `editor/input.go` | MODIFY | Add `Autocomplete` field to `InputState` |
| `editor/editor.go` | MODIFY | Integrate autocomplete in Insert mode handling |
| `editor/screen.go` | MODIFY | Add dropdown rendering |

## Implementation Steps

### Step 1: Create autocomplete.go

Create `editor/autocomplete.go` with:

1. **Data structures**: `AutocompleteState`, `Suggestion`
2. **Word extraction**: `ExtractWords()` function
3. **Matching logic**: `MatchSubsequence()`, `ScoreSuggestion()`, `FindSuggestions()`
4. **State management**: `NewAutocompleteState()`, `Next()`, `Prev()`, `Selected()`

Key implementation details:

```go
// Tokenizer delimiters
var delimiters = "()[]{}.,;:!?<>=+-*/%&|^~\"'` \t\n\r"

// Check if rune is a delimiter
func isDelimiter(r rune) bool {
    return strings.ContainsRune(delimiters, r)
}
```

### Step 2: Modify input.go

Add field to `InputState`:

```go
type InputState struct {
    // ... existing fields ...

    // Autocomplete state
    Autocomplete *AutocompleteState
}
```

### Step 3: Modify editor.go

#### 3a. Track current word being typed

Add helper to detect word prefix at cursor:

```go
func (e *Editor) getCurrentWordPrefix() (prefix string, startCol int) {
    buf := e.activeBuffer()
    if buf.CursorCol == 0 {
        return "", 0
    }
    line := buf.Lines[buf.CursorRow]
    // Scan backwards from cursor to find word start
    startCol = buf.CursorCol
    for startCol > 0 && !isDelimiter(line[startCol-1]) {
        startCol--
    }
    prefix = string(line[startCol:buf.CursorCol])
    return prefix, startCol
}
```

#### 3b. Trigger autocomplete after character insert

In `handleInsertMode()`, after inserting a character:

```go
// After character insert...
e.triggerAutocomplete()
```

```go
func (e *Editor) triggerAutocomplete() {
    prefix, startCol := e.getCurrentWordPrefix()
    if len(prefix) < 2 {
        e.inputState.Autocomplete = nil
        return
    }

    buf := e.activeBuffer()
    words := ExtractWords(buf.Lines, buf.CursorRow, startCol)
    suggestions := FindSuggestions(words, prefix, 10)

    if len(suggestions) == 0 {
        e.inputState.Autocomplete = nil
        return
    }

    e.inputState.Autocomplete = NewAutocompleteState(prefix, startCol, suggestions)
}
```

#### 3c. Handle autocomplete keys

In `handleInsertMode()`, check for autocomplete keys FIRST:

```go
func (e *Editor) handleInsertMode(ev *tcell.EventKey) bool {
    ac := e.inputState.Autocomplete

    // Handle autocomplete keys when dropdown is visible
    if ac != nil && ac.Active {
        switch ev.Key() {
        case tcell.KeyTab:
            e.acceptAutocomplete()
            return false
        case tcell.KeyEscape:
            e.inputState.Autocomplete = nil
            return false
        case tcell.KeyCtrlN:
            ac.Next()
            return false
        case tcell.KeyCtrlP:
            ac.Prev()
            return false
        }
    }

    // ... rest of existing Insert mode handling ...
}
```

#### 3d. Accept suggestion

```go
func (e *Editor) acceptAutocomplete() {
    ac := e.inputState.Autocomplete
    if ac == nil || len(ac.Suggestions) == 0 {
        return
    }

    selected := ac.Selected()
    if selected == nil {
        return
    }

    buf := e.activeBuffer()
    line := buf.Lines[buf.CursorRow]

    // Replace prefix with full word
    completion := selected.Word[len(ac.Prefix):]
    newLine := make([]rune, len(line)+len(completion))
    copy(newLine[:buf.CursorCol], line[:buf.CursorCol])
    copy(newLine[buf.CursorCol:], []rune(completion))
    copy(newLine[buf.CursorCol+len(completion):], line[buf.CursorCol:])

    buf.Lines[buf.CursorRow] = newLine
    buf.CursorCol += len(completion)
    buf.Modified = true

    e.inputState.Autocomplete = nil
    e.scheduleAutoSave()
}
```

#### 3e. Dismiss on cursor movement

In any cursor movement function, add:

```go
e.inputState.Autocomplete = nil
```

### Step 4: Modify screen.go

Add dropdown rendering after main content:

```go
func (s *Screen) Render(buffers []*Buffer, activePane int, mode Mode, inputState *InputState) {
    // ... existing rendering ...

    // Render autocomplete dropdown if active
    if inputState.Autocomplete != nil && inputState.Autocomplete.Active {
        s.renderAutocomplete(inputState.Autocomplete, cursorX, cursorY, height)
    }

    s.screen.Show()
}

func (s *Screen) renderAutocomplete(ac *AutocompleteState, cursorX, cursorY, screenHeight int) {
    if len(ac.Suggestions) == 0 {
        return
    }

    // Calculate dropdown dimensions
    maxWidth := 0
    for _, sug := range ac.Suggestions {
        if len(sug.Word) > maxWidth {
            maxWidth = len(sug.Word)
        }
    }
    width := maxWidth + 2  // padding
    height := len(ac.Suggestions)

    // Determine position (below or above cursor)
    spaceBelow := screenHeight - cursorY - 1
    var startY int
    if spaceBelow >= height {
        startY = cursorY + 1
    } else {
        startY = cursorY - height
        if startY < 0 {
            startY = 0
            height = cursorY
        }
    }

    // Draw dropdown
    bgStyle := tcell.StyleDefault.Background(tcell.ColorDarkBlue).Foreground(tcell.ColorWhite)
    selStyle := tcell.StyleDefault.Background(tcell.ColorBlue).Foreground(tcell.ColorWhite)

    for i, sug := range ac.Suggestions[:height] {
        style := bgStyle
        if i == ac.SelectedIdx {
            style = selStyle
        }

        y := startY + i
        // Draw suggestion text
        for x, r := range sug.Word {
            s.screen.SetContent(cursorX+x, y, r, nil, style)
        }
        // Pad remaining width
        for x := len(sug.Word); x < width; x++ {
            s.screen.SetContent(cursorX+x, y, ' ', nil, style)
        }
    }
}
```

## Testing Checklist

- [ ] Type 2 characters → dropdown appears
- [ ] Type 1 character → no dropdown
- [ ] Tab → accepts highlighted suggestion
- [ ] Ctrl+n → moves highlight down (wraps)
- [ ] Ctrl+p → moves highlight up (wraps)
- [ ] Escape → dismisses dropdown
- [ ] Arrow keys → dismisses dropdown, moves cursor
- [ ] No matches → dropdown doesn't appear
- [ ] Prefix match appears first in list
- [ ] Dropdown positions above when near bottom of screen

## Build & Test

```bash
rm -f ef && go build -o ef .
./fe testfile.txt
```

Enter Insert mode (`i`), type 2+ characters of an existing word, verify dropdown behavior.
