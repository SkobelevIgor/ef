# Implementation Plan: Reindent Selection

**Branch**: `011-reindent-selection` | **Date**: 2026-03-01 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/011-reindent-selection/spec.md`

## Summary

Add `=` key binding in visual mode to reindent selected lines. Each selected line is left-trimmed and given the indentation of the line above it. Lines are processed top-to-bottom so each reindented line becomes the reference for the next. No smart/language-specific formatting — purely mechanical indentation matching using existing `getLeadingWhitespace()` and `stripLeadingWhitespace()` helpers.

## Technical Context

**Language/Version**: Go 1.21+ (existing project)
**Primary Dependencies**: github.com/gdamore/tcell/v2 (existing)
**Storage**: In-memory (`Buffer.Lines [][]rune`)
**Testing**: Manual terminal testing + existing Go test patterns
**Target Platform**: Terminal (any OS with terminal support)
**Project Type**: Single Go project
**Performance Goals**: Instant for up to 1000 lines (trivial — single pass over selected lines)
**Constraints**: No new dependencies; must follow existing visual mode operation patterns
**Scale/Scope**: 2 files modified, ~30 lines of new code

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | PASS | Keyboard-only operation via tcell, no GUI |
| II. Vim-Style Modal Editing | PASS | `=` in visual mode aligns with Vim conventions |
| III. Data Integrity | PASS | In-memory line mutation, auto-save scheduled after operation |
| Code Quality | PASS | Follows existing patterns (`>`, `<` indent/unindent) |
| Minimal Dependencies | PASS | No new dependencies |
| Binary rebuild | PASS | Will rebuild after changes |

All gates pass. No violations.

## Project Structure

### Documentation (this feature)

```text
specs/011-reindent-selection/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (via /speckit.tasks)
```

### Source Code (repository root)

```text
editor/
├── editor_visual.go     # ADD: '=' case in handleVisualMode() switch
└── buffer.go            # ADD: ReindentRange(startRow, endRow) method
```

**Structure Decision**: Follows existing single-package layout. The `=` handler goes in `editor_visual.go` (alongside `>` and `<`), and the buffer manipulation goes in `buffer.go` (alongside `IndentRange` and `UnindentRange`).

## Design

### New Buffer Method: `ReindentRange(startRow, endRow int)`

Located in `buffer.go`, follows the same signature pattern as `IndentRange` and `UnindentRange`.

**Algorithm**:
```
for row = startRow to endRow:
    if row == 0:
        prevIndent = [] (empty — no previous line)
    else:
        prevLine = b.Lines[row-1]
        if prevLine is empty:
            prevIndent = []
        else:
            prevIndent = getLeadingWhitespace(prevLine)

    stripped = stripLeadingWhitespace(b.Lines[row])
    b.Lines[row] = concat(prevIndent, stripped)

b.Modified = true
b.ModCount++
```

**Key details**:
- Uses existing `getLeadingWhitespace()` (buffer.go:268) to extract reference indentation
- Uses existing `stripLeadingWhitespace()` (buffer.go:577) to left-trim
- Empty reference line → 0 indentation (stripped only)
- Empty selected line → receives previous line's indentation prefix only
- Preserves tab/space characters from the reference line as-is

### Visual Mode Handler Addition

In `editor_visual.go`, add `'='` case after the `'<'` case (line ~146), following the exact same pattern as `>` and `<`:

```go
case '=':
    startRow, _, endRow, _ := pane.GetSelection()
    buf.ReindentRange(startRow, endRow)
    e.mode = ModeNormal
    pane.ClearSelection()
    e.inputState.Reset()
    e.scheduleAutoSave()
```

### Cursor Position After Operation

Cursor stays at `pane.CursorRow` / `pane.CursorCol` (unchanged). This matches behavior of `>` and `<` operations. The `ClearSelection()` call keeps cursor in place.

### Undo Behavior

The `>` and `<` operations don't record undo history explicitly (they modify lines directly). For consistency, `=` will follow the same pattern. If undo recording is desired later, it can be added uniformly to all three operations.
