# Quickstart: Search Mode Implementation

**Feature**: 001-search-mode
**Date**: 2026-02-04

## Overview

This guide provides the implementation approach for adding Search Mode to the fe editor.

## File Changes Summary

| File | Change Type | Description |
|------|-------------|-------------|
| `editor/search.go` | **NEW** | SearchState struct, SearchMatch, search logic |
| `editor/input.go` | Modify | Add Search field to InputState |
| `editor/editor.go` | Modify | Add F4 handler, search mode handling, navigation, lastSearchQuery field |
| `editor/screen.go` | Modify | Add search bar rendering |
| `editor/buffer.go` | Modify | Add case-insensitive search methods |

## Implementation Order

### Phase 1: Core Data Structures

1. **Create `editor/search.go`**:
```go
package editor

import "strings"

// SearchMatch represents a single occurrence
type SearchMatch struct {
    Row    int
    Col    int
    Length int
}

// SearchState tracks the active search session
type SearchState struct {
    Active        bool
    Query         string
    Confirmed     bool
    CursorZeroRow int
    CursorZeroCol int
    Matches       []SearchMatch
    CurrentIndex  int
    IsReplaceMode bool
    ReplaceText   string
    NoMatches     bool
}

// NewSearchState creates a new search state with cursor position
func NewSearchState(cursorRow, cursorCol int) *SearchState {
    return &SearchState{
        Active:        true,
        CursorZeroRow: cursorRow,
        CursorZeroCol: cursorCol,
        CurrentIndex:  -1,
    }
}

// ParseQuery parses the search input for replace syntax
func ParseQuery(input string) (query, replacement string, isReplace bool) {
    if strings.HasPrefix(input, "replace::") {
        parts := strings.SplitN(input[9:], "::", 2)
        if len(parts) == 2 {
            return parts[0], parts[1], true
        }
    }
    return input, "", false
}
```

2. **Extend `editor/input.go`**:
```go
type InputState struct {
    // ... existing fields ...

    // Search mode state
    Search *SearchState
}
```

### Phase 2: Search Logic

3. **Add to `editor/buffer.go`**:
```go
// FindAllMatches finds all case-insensitive occurrences of query
func (b *Buffer) FindAllMatches(query string) []SearchMatch {
    if query == "" {
        return nil
    }

    var matches []SearchMatch
    queryLower := strings.ToLower(query)

    for row, line := range b.Lines {
        lineLower := strings.ToLower(string(line))
        col := 0
        for {
            idx := strings.Index(lineLower[col:], queryLower)
            if idx == -1 {
                break
            }
            matches = append(matches, SearchMatch{
                Row:    row,
                Col:    col + idx,
                Length: len(query),
            })
            col += idx + 1
        }
    }
    return matches
}
```

### Phase 3: Input Handling

4. **Add to `editor/editor.go`**:
```go
// In handleKey(), add F4 handler:
case tcell.KeyF4:
    e.enterSearchMode()
    return false

// New method (with query preservation support):
func (e *Editor) enterSearchMode() {
    buf := e.activeBuffer()
    search := NewSearchState(buf.CursorRow, buf.CursorCol)

    // Restore previous query if available
    if e.lastSearchQuery != "" {
        search.Query = e.lastSearchQuery
        search.Confirmed = true  // Enable immediate n/N navigation
        search.Matches = buf.FindAllMatches(search.Query)
        search.NoMatches = len(search.Matches) == 0
        if len(search.Matches) > 0 {
            // Find first match after current cursor
            search.CurrentIndex = FindFirstMatchAfterCursor(
                search.Matches, buf.CursorRow, buf.CursorCol)
        }
    }

    e.inputState.Search = search
}

// New handler:
func (e *Editor) handleSearchMode(ev *tcell.EventKey) bool {
    search := e.inputState.Search
    buf := e.activeBuffer()

    switch ev.Key() {
    case tcell.KeyEscape:
        e.exitSearchMode(false)
    case tcell.KeyEnter:
        if !search.Confirmed {
            e.confirmSearch()
        } else if search.IsReplaceMode {
            e.replaceCurrentMatch()
        }
    case tcell.KeyBackspace, tcell.KeyBackspace2:
        if len(search.Query) > 0 {
            search.Query = search.Query[:len(search.Query)-1]
            e.updateSearchMatches()
        }
    case tcell.KeyRune:
        if search.Confirmed {
            // Handle n/N navigation
            switch ev.Rune() {
            case 'n':
                e.nextMatch()
            case 'N':
                e.prevMatch()
            }
        } else {
            // Accumulate query
            search.Query += string(ev.Rune())
            e.updateSearchMatches()
        }
    }
    return false
}
```

### Phase 4: Rendering

5. **Modify `editor/screen.go`**:
```go
func (s *Screen) Render(buffers []*Buffer, activePane int, mode Mode, inputState *InputState) {
    s.screen.Clear()
    width, height := s.screen.Size()

    // Check if search is active
    searchActive := inputState.Search != nil && inputState.Search.Active
    contentStartY := 0

    if searchActive {
        s.renderSearchBar(inputState.Search, width)
        contentStartY = 1
        height-- // Reduce available height for content
    }

    // Rest of existing rendering with adjusted startY...
}

func (s *Screen) renderSearchBar(search *SearchState, width int) {
    style := tcell.StyleDefault.Background(tcell.ColorDarkBlue).Foreground(tcell.ColorWhite)

    // Clear the row
    for x := 0; x < width; x++ {
        s.screen.SetContent(x, 0, ' ', nil, style)
    }

    // Draw prompt and query
    prompt := "Search: "
    if search.IsReplaceMode {
        prompt = "Replace: "
    }

    x := 0
    for _, ch := range prompt {
        s.screen.SetContent(x, 0, ch, nil, style)
        x++
    }
    for _, ch := range search.Query {
        s.screen.SetContent(x, 0, ch, nil, style)
        x++
    }

    // Show feedback
    if search.NoMatches && search.Query != "" {
        feedback := " (No matches)"
        for _, ch := range feedback {
            if x < width {
                s.screen.SetContent(x, 0, ch, nil, style.Foreground(tcell.ColorRed))
                x++
            }
        }
    }
}
```

### Phase 5: Navigation and Replace

6. **Add navigation methods to `editor/editor.go`**:
```go
func (e *Editor) nextMatch() {
    search := e.inputState.Search
    if len(search.Matches) == 0 {
        return
    }
    search.CurrentIndex = (search.CurrentIndex + 1) % len(search.Matches)
    e.navigateToCurrentMatch()
}

func (e *Editor) prevMatch() {
    search := e.inputState.Search
    if len(search.Matches) == 0 {
        return
    }
    search.CurrentIndex--
    if search.CurrentIndex < 0 {
        search.CurrentIndex = len(search.Matches) - 1
    }
    e.navigateToCurrentMatch()
}

func (e *Editor) replaceCurrentMatch() {
    search := e.inputState.Search
    if search.CurrentIndex < 0 || search.CurrentIndex >= len(search.Matches) {
        return
    }

    buf := e.activeBuffer()
    match := search.Matches[search.CurrentIndex]

    // Record for undo
    oldText := buf.GetRange(match.Row, match.Col, match.Row, match.Col+match.Length-1)

    // Delete old text
    buf.DeleteRange(match.Row, match.Col, match.Row, match.Col+match.Length)

    // Insert replacement
    for _, ch := range search.ReplaceText {
        buf.InsertChar(ch)
    }

    // Push to history
    e.history.Push(&Change{
        Type:    ChangeReplace,
        Row:     match.Row,
        Col:     match.Col,
        Text:    [][]rune{[]rune(search.ReplaceText)},
        OldText: oldText,
    })

    // Recalculate and move to next
    e.updateSearchMatches()
    e.nextMatch()
    e.scheduleAutoSave()
}
```

## Testing Checklist

- [ ] F4 opens search bar from Normal mode
- [ ] F4 opens search bar from Insert mode
- [ ] Typing shows incremental first match highlight
- [ ] Enter confirms search and enables n/N
- [ ] n moves to next match (wraps at end)
- [ ] N moves to previous match (wraps at start)
- [ ] Escape before Enter returns to CursorZero
- [ ] Escape after Enter leaves cursor at match end
- [ ] "No matches" displays for invalid query
- [ ] Case-insensitive matching works
- [ ] `replace::foo::bar` syntax works
- [ ] Enter replaces current match
- [ ] n skips match in replace mode
- [ ] Each replacement is individually undoable
- [ ] Empty replacement deletes matched text
- [ ] F4 after confirmed search reopens with previous query
- [ ] Reopened search allows immediate n/N navigation
- [ ] Typing in reopened search modifies the query

## Build Command

After implementation:
```bash
rm -f ef && go build -o ef .
```
