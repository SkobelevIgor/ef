# Implementation Plan: Code Simplification

**Branch**: `009-code-simplification` | **Date**: 2026-03-01 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/009-code-simplification/spec.md`

## Summary

Consolidate duplicated logic, extract shared utilities, reduce repetitive patterns, and complete the key mapping timeout feature across the editor codebase. All changes are internal refactoring — no user-facing behavior changes. Research identified 10 concrete simplification targets with exact file locations and caller maps.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (terminal rendering)
**Storage**: N/A (in-memory buffer)
**Testing**: e2e test runner (Python-based in tests/e2e/), unit tests in editor/buffer_test.go
**Target Platform**: macOS/Linux terminal
**Project Type**: Single project
**Performance Goals**: No degradation from current performance
**Constraints**: Zero behavioral changes; all e2e tests must pass unchanged
**Scale/Scope**: ~6,300 lines across 31 Go files in `editor/` package

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | PASS | No changes to I/O or rendering model |
| II. Vim-Style Modal Editing | PASS | No changes to mode behavior or key bindings |
| III. Data Integrity | PASS | No changes to save/undo/redo semantics |
| Code Quality | PASS | Refactoring improves code quality — Go idioms preserved |
| Minimal Dependencies | PASS | No new dependencies added |
| Binary rebuild | PASS | Will rebuild after changes: `go build -o ef .` |

**Post-Design Re-check**: All gates still pass. New files (`indent.go`, `utils.go`) stay within `editor/` package. Key mapping timeout completion uses only existing `tcell` event infrastructure.

## Project Structure

### Documentation (this feature)

```text
specs/009-code-simplification/
├── plan.md              # This file
├── research.md          # Phase 0 output - research findings
├── checklists/
│   └── requirements.md  # Spec quality checklist
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
editor/
├── utils.go             # NEW — shared utility functions (normalizeRange, VisualColumn, isWhitespace, ClampPosition)
├── indent.go            # NEW — extracted indentation logic (ReindentLines, smartIndentForNewLine, helpers)
├── buffer.go            # MODIFIED — remove duplicate visual column calcs, whitespace helpers, indentation code
├── screen.go            # MODIFIED — replace local getVisualColumn/getVisualLineWidth with utils.go calls
├── pane.go              # MODIFIED — use normalizeRange in GetSelection
├── editor.go            # MODIFIED — complete key mapping timeout
├── editor_normal.go     # MODIFIED — auto-reset wrapper for inputState
├── editor_paste.go      # MODIFIED — split performPaste into sub-functions
├── editor_search.go     # MODIFIED — extract navigateToMatch helper
├── editor_undo.go       # MODIFIED — add mirror documentation comments
├── editor_marks.go      # MODIFIED — use ClampPosition utility
├── editor_insert.go     # MODIFIED — move indentation functions to indent.go
├── editor_visual.go     # MODIFIED — inputState.Reset() calls absorbed by auto-reset
├── input.go             # UNCHANGED — InputState definition stays
└── constants.go         # UNCHANGED — MappingTimeout constant stays
```

**Structure Decision**: Existing single-package structure preserved. Two new files added: `utils.go` for small shared utilities and `indent.go` for extracted indentation logic. This keeps buffer.go from being 870 lines and groups related indentation logic together.

## Design Decisions

### D1: utils.go — Shared Utilities

**File**: `editor/utils.go`

Contains:
```go
// VisualColumn calculates visual column position accounting for tab expansion
func VisualColumn(line []rune, charCol int, tabStop int) int

// VisualLineWidth calculates visual width of entire line
func VisualLineWidth(line []rune, tabStop int) int

// NormalizeRange ensures start position is before end position
func NormalizeRange(startRow, startCol, endRow, endCol int) (int, int, int, int)

// ClampPosition clamps row/col to valid buffer bounds
func ClampPosition(lines [][]rune, row, col int) (int, int)

// isWhitespace returns true for space and tab
func isWhitespace(r rune) bool
```

**Callers updated**:
- `Buffer.GetVisualColumn` → thin wrapper calling `VisualColumn(line, charCol, b.Config.TabStop)`
- `Buffer.GetVisualLineWidth` → thin wrapper calling `VisualLineWidth(line, b.Config.TabStop)`
- screen.go `getVisualColumn`/`getVisualLineWidth` → replaced with direct `VisualColumn`/`VisualLineWidth` calls
- `GetSelection`, `DeleteRange`, `GetRange` → call `NormalizeRange`
- `jumpToMark`, undo handler → call `ClampPosition`
- All whitespace helpers → use `isWhitespace` predicate

### D2: indent.go — Indentation Module

**File**: `editor/indent.go`

Functions moved from buffer.go:
- `ReindentLines` (buffer.go:735-837)
- `smartIndentForNewLine` (buffer.go:233-268)
- `GetPrevNonEmptyLineIndent` (buffer.go:686-702)
- `getIndentLevel` (helper)
- `makeIndent` (helper)
- `countRune` (helper)

Functions moved from editor_insert.go:
- `reindentPastedRange` (editor_insert.go:9-18)
- `endInsertSession` (editor_insert.go:48-73)
- `reindentInsertSession` (editor_insert.go:85-133)

Total: ~300 lines consolidated into one file.

### D3: inputState Auto-Reset

**Pattern**: In `handleNormalModeRune`, the function currently returns `bool`. Change semantics:
- Return `true` = "I handled this, do NOT auto-reset" (state-accumulating commands)
- Return `false` = "I handled this, auto-reset is fine"

At the call site in `handleNormalMode`, add:
```go
if !e.handleNormalModeRune(ch) {
    e.inputState.Reset()
}
```

**Opt-out commands** (return `true`): digits 0-9, `f`/`F`, `:`, `m`, `` ` ``, `d`/`y`

All other ~30 commands remove their explicit `e.inputState.Reset()` call and return `false`.

### D4: performPaste Split

Split into:
- `pasteWholeLines(before bool) (int, int)` — lines 15-38
- `pasteSingleLine(insertPos int)` — lines 51-66
- `pasteMultiLine(insertPos int) (int, int)` — lines 68-107

`performPaste` becomes a thin dispatcher.

### D5: Search Navigation Helper

```go
func (e *Editor) navigateToMatch(cursorRow, cursorCol int) {
    search := e.inputState.Search
    search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, cursorRow, cursorCol)
    if search.CurrentIndex >= 0 {
        match := search.Matches[search.CurrentIndex]
        pane := e.activePane()
        pane.CursorRow = match.Row
        pane.CursorCol = match.Col
    }
}
```

Replaces 3 identical patterns at editor_search.go:30-35, 110-116, 275-278.

### D6: Key Mapping Timeout Completion

**Missing pieces to implement**:

1. **Custom event type**:
```go
type EventMappingTimeout struct {
    when      time.Time
    snapshot  time.Time // mapKeyTime at goroutine launch
}
```

2. **Goroutine posts event** (replace empty goroutine at editor.go:477):
```go
snapshot := e.mapKeyTime
go func() {
    time.Sleep(MappingTimeout)
    e.screen.PostEvent(&EventMappingTimeout{
        when:     time.Now(),
        snapshot: snapshot,
    })
}()
```

3. **Main loop handles event** (in Run() event loop):
```go
case *EventMappingTimeout:
    if ev.snapshot == e.mapKeyTime && e.pendingMapKeys != "" {
        e.flushPendingMapKeys()
        e.pendingMapKeys = ""
        e.mapKeyTime = time.Time{}
    }
```

The snapshot comparison prevents stale goroutines from flushing keys that have already been consumed.

### D7: Undo/Redo — Comments Only

Add documentation comment block above both functions explaining the mirror relationship. No structural changes — the risk/benefit doesn't favor merging.

### D8: Dead Code Removal

Remove `firstNonWhitespace()` from buffer.go:725-733 — zero callers in the codebase.

## Implementation Order

Ordered to minimize risk and maximize testability at each step:

1. **utils.go creation** — New file, no existing code modified yet. Write tests.
2. **Visual column consolidation** — Update callers in buffer.go, screen.go, pane.go to use utils.go.
3. **normalizeRange adoption** — Update GetSelection, DeleteRange, GetRange.
4. **isWhitespace + whitespace helpers** — Refactor helpers in buffer.go.
5. **ClampPosition + cursor clamping** — Update jumpToMark, undo handler.
6. **Dead code removal** — Remove `firstNonWhitespace`.
7. **indent.go extraction** — Move functions, no logic changes.
8. **performPaste split** — Refactor into sub-functions.
9. **Search navigation helper** — Extract navigateToMatch.
10. **inputState auto-reset** — Change handleNormalModeRune return semantics.
11. **Key mapping timeout** — Complete implementation.
12. **Undo/redo comments** — Add documentation.
13. **Final build + e2e tests** — Verify everything works.
