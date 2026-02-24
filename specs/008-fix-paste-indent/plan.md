# Implementation Plan: Fix Paste Indentation

**Branch**: `007-fix-paste-indent` | **Date**: 2026-02-21 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/007-fix-paste-indent/spec.md`

## Summary

Fix indentation for all paste/put operations in the ef editor. Currently, insert-mode paste (`applyPastedIndent`) partially handles indentation on 2nd+ lines but misses the first line, and vim `p`/`P` commands insert content verbatim with no indentation correction. The plan introduces a unified `reindentPastedBlock()` function that normalizes pasted content to match the insertion context, working for both OS clipboard paste and vim put commands. Gated by `AutoIndentation` config flag — unconfigured file types or files with `AutoIndentation: false` get raw paste.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (terminal rendering)
**Storage**: N/A (in-memory `Buffer.Lines`)
**Testing**: Manual terminal testing + `go test` for unit tests on indentation logic
**Target Platform**: Terminal (macOS, Linux)
**Project Type**: Single Go binary
**Performance Goals**: Instant — paste reindentation must be imperceptible (<1ms for 1000-line paste)
**Constraints**: No external dependencies; no AST parsing
**Scale/Scope**: Affects `editor.go` (paste handling, p/P), `buffer.go` (new reindent function)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | PASS | All changes are in-memory buffer operations; no GUI dependencies |
| II. Vim-Style Modal Editing | PASS | Enhances p/P and insert-mode paste; does not change mode transitions |
| III. Data Integrity | PASS | Paste+reindent is a single undoable operation via existing `ChangeReplace` history |
| Code Quality | PASS | Go idioms, minimal new code, no new dependencies |
| Governance | PASS | Working on feature branch, binary rebuild after changes |

No violations. No complexity tracking needed.

## Project Structure

### Documentation (this feature)

```text
specs/007-fix-paste-indent/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
editor/
├── editor.go            # MODIFY: paste event handling, p/P commands, applyPastedIndent replacement
├── buffer.go            # MODIFY: add reindentLines() helper, update getIndentLevel for mixed input
└── config.go            # READ ONLY: FileTypeConfig (AutoIndentation, ExpandTab, ShiftWidth)
```

**Structure Decision**: Single project, all changes within the existing `editor/` package. Two files modified: `editor.go` and `buffer.go`. No new files needed.

## Design

### Current State Analysis

**Insert-mode paste** (bracketed + timing-detected):
- `applyPastedIndent()` at `editor.go:805-837` handles indentation per-line as characters stream in
- Uses `pasteBaseIndent` (indent of line before Enter) and `pasteAutoIndent` (auto-generated indent on new line) to compute `targetLevel = pasteAutoIndent + (pastedIndent - pasteBaseIndent)`
- **Gap**: First pasted line is never reindented — only lines after an Enter within the paste get processed
- **Gap**: The insertion context uses auto-generated indent rather than previous non-empty line (spec requires previous non-empty line)

**Vim p/P commands** (`pasteAfter`/`pasteBefore` at `editor.go:1141-1220` and `1494-1566`):
- Insert clipboard content verbatim — no indentation adjustment at all
- Line-mode paste (`clipboardLine == true`): inserts whole lines via `InsertLineAfter`
- Char-mode paste: splices into current line at cursor position

### Design: Unified Post-Paste Reindentation

Instead of fixing the incremental approach in `applyPastedIndent`, introduce a **batch reindentation** that runs after any paste/put operation completes:

#### New function: `Buffer.ReindentLines(startRow, endRow, contextIndent int)`

Located in `buffer.go`. Given a range of lines just pasted:

1. **Find base indent**: Scan lines `[startRow, endRow]`, compute `getIndentLevel()` for each non-empty line, find the minimum — this is the block's base indent level.
2. **Reindent each line**: For each non-empty line in range:
   - `originalLevel = getIndentLevel(line)`
   - `newLevel = contextIndent + (originalLevel - baseIndent)`
   - Strip existing leading whitespace
   - Prepend `makeIndent(newLevel)`
3. **Empty lines**: Leave empty (no indent characters added per FR-006).

#### Context indent derivation (FR-001)

New helper: `Buffer.GetPrevNonEmptyLineIndent(row int) int`
- Walk backwards from `row - 1` to find the first non-empty line
- Return `getIndentLevel()` of that line
- If no previous non-empty line found, return 0

#### Integration points

**Insert-mode paste**: After the paste ends (in `EventPaste` end handler at line 229 and timing-based reset at line 406):
1. Track `pasteStartRow` when paste begins (new field on Editor)
2. When paste ends, if `buf.FileType != "" && buf.Config.AutoIndentation`:
   - `contextIndent = buf.GetPrevNonEmptyLineIndent(pasteStartRow)`
   - `buf.ReindentLines(pasteStartRow, buf.CursorRow, contextIndent)`
3. Remove existing incremental `applyPastedIndent()` call path (simplifies the flow)

**Vim p/P commands** (in `handleNormalModeRune` and `handleVisualMode`):
1. After `pasteAfter()`/`pasteBefore()` completes, note the range of inserted lines
2. If `buf.FileType != "" && buf.Config.AutoIndentation`:
   - For line-mode: `contextIndent = buf.GetPrevNonEmptyLineIndent(firstInsertedRow)`
   - `buf.ReindentLines(firstInsertedRow, lastInsertedRow, contextIndent)`
3. For char-mode paste: If multi-line, reindent 2nd+ pasted lines only (first line joins inline per FR-010)

**Mid-line paste handling** (FR-010):
- If cursor is NOT at column 0 (or on an empty line) when paste starts:
  - First pasted line joins inline — no indent change
  - Only 2nd+ pasted lines get reindented
  - `ReindentLines(startRow + 1, endRow, contextIndent)` — skip first row

#### Undo integration (FR-008)

The existing pattern already handles this correctly:
- `p`/`P` commands snapshot `oldLines` before paste and push a single `ChangeReplace` after — reindenting within the same block means the single undo captures everything.
- Insert-mode paste: Need to ensure the reindentation at paste-end is covered by the same undo group. The existing `EventPaste` end handler and timing-based handler should push a `ChangeReplace` covering `[pasteStartRow, CursorRow]` with the pre-paste snapshot.

#### Guard conditions

All reindentation is gated by: `buf.FileType != "" && buf.Config.AutoIndentation`
- If either condition is false, paste/put operates exactly as current behavior (verbatim insertion).

### Mixed indentation handling (FR-007)

The existing `getIndentLevel()` already handles mixed tabs/spaces (counts tabs as 1 level, ShiftWidth spaces as 1 level). Combined with `makeIndent()` which outputs pure tabs or pure spaces based on config, the conversion from mixed source to target style is automatic.

### Changes Summary

| File | Function | Change |
|------|----------|--------|
| `buffer.go` | `ReindentLines()` | NEW: Batch reindent a range of lines |
| `buffer.go` | `GetPrevNonEmptyLineIndent()` | NEW: Walk backward to find context indent |
| `editor.go` | Editor struct | ADD: `pasteStartRow int` field to track paste origin |
| `editor.go` | `EventPaste` handler | MODIFY: Record `pasteStartRow` on start; call `ReindentLines` on end |
| `editor.go` | Timing-based paste handler | MODIFY: Same as EventPaste — track start, reindent on end |
| `editor.go` | `handleInsertMode` | SIMPLIFY: Remove incremental `applyPastedIndent` path; paste characters are collected normally |
| `editor.go` | `applyPastedIndent()` | REMOVE: Replaced by batch `ReindentLines` |
| `editor.go` | `pasteAfter()` | MODIFY: Return inserted line range; caller calls `ReindentLines` |
| `editor.go` | `pasteBefore()` | MODIFY: Return inserted line range; caller calls `ReindentLines` |
| `editor.go` | `handleNormalModeRune` p/P | MODIFY: After paste, call `ReindentLines` on returned range |
| `editor.go` | `handleVisualMode` p/P | MODIFY: After paste, call `ReindentLines` on returned range |
