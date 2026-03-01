# Research: Code Simplification

## R1: Visual Column Calculation Consolidation

**Decision**: Unify into a single standalone function `VisualColumn(line []rune, charCol int, tabStop int) int` with `tabStop` as explicit parameter.

**Rationale**: The screen.go version already takes `tabStop` as a parameter, which is more flexible and testable. Buffer methods become thin wrappers passing `b.Config.TabStop`. Both implementations have identical core logic.

**Current State**:
- `Buffer.GetVisualColumn(line, charCol)` at buffer.go:325-340 — reads `b.Config.TabStop` internally
- `Buffer.GetVisualLineWidth(line)` at buffer.go:342-345 — delegates to above
- `getVisualColumn(line, charCol, tabStop)` at screen.go:520-535 — takes tabStop param
- `getVisualLineWidth(line, tabStop)` at screen.go:537-540 — delegates to above

**Callers**: 7 total — buffer.go:293 (InsertTab), pane.go:140,147,160,176 (AdjustScroll), screen.go:539,562,572 (calcCursorScreenPos)

**Alternatives considered**: Keep Buffer methods as primary → rejected because screen.go callers don't have a Buffer reference in the calling context.

## R2: Range Normalization

**Decision**: Extract `normalizeRange(startRow, startCol, endRow, endCol int) (int, int, int, int)` utility function.

**Rationale**: Identical 3-line swap pattern appears in 3 places. Additional row-only swaps in IndentRange, UnindentRange, ReindentLines.

**Current State**: Identical pattern at pane.go:220-222 (GetSelection), buffer.go:423-425 (DeleteRange), buffer.go:488-490 (GetRange).

## R3: Cursor Clamping

**Decision**: Create `ClampPosition(buf *Buffer, row, col int) (int, int)` utility used by jumpToMark and other ad-hoc clamping sites. Keep `Pane.clampCursor()` as it directly mutates pane fields.

**Rationale**: The primary `clampCursor()/clampCursorCol()` methods on Pane are well-structured. The ad-hoc clamping in jumpToMark (editor_marks.go:81-97) and undo handler (editor_undo.go:72-75) duplicates the same logic. A pure function taking row/col and returning clamped values lets these callers avoid reimplementing bounds checks.

## R4: Whitespace Helpers

**Decision**: Add `isWhitespace(r rune) bool` predicate; refactor existing helpers to use it. Remove unused `firstNonWhitespace()`.

**Rationale**: Five functions independently check `ch == ' ' || ch == '\t'`. Sharing a predicate improves consistency. `firstNonWhitespace` has zero callers — dead code.

## R5: Indentation Module Extraction

**Decision**: Move all indentation functions to new file `editor/indent.go`. No new types — keep as Buffer methods and standalone functions.

**Rationale**: Indentation logic totals ~300 lines across buffer.go and editor_insert.go. A dedicated file groups related logic without introducing new abstractions. Functions to move:
- From buffer.go: `ReindentLines`, `smartIndentForNewLine`, `GetPrevNonEmptyLineIndent`, `getIndentLevel`, `makeIndent`, `countRune`
- From editor_insert.go: `reindentPastedRange`, `endInsertSession`, `reindentInsertSession`

**Dependencies**: All stay within `editor` package. No import cycle risk.

## R6: inputState.Reset() Auto-Reset Wrapper

**Decision**: Add auto-reset wrapper in `handleNormalModeRune`. Commands that should NOT reset return early before the wrapper calls Reset().

**Rationale**: 45 Reset() calls across 5 files. In editor_normal.go alone, ~30 calls follow the pattern `doThing(); e.inputState.Reset()`. 7 command types must opt out: numeric input (0-9), `f`/`F` (pending find), `:` (goto line), `m` (mark set), `` ` `` (mark jump), `d`/`y` (pending operator).

**Pattern**: The `handleNormalModeRune` already returns a bool. Change semantics: return `true` to skip auto-reset (state-accumulating commands), `false` for normal commands where auto-reset applies. Add `defer` at caller site.

## R7: performPaste Split

**Decision**: Split into 3 sub-functions: `pasteWholeLines(before bool) (int, int)`, `pasteSingleLine(insertPos int)`, `pasteMultiLine(insertPos int) (int, int)`.

**Rationale**: 101-line function with 3 clear branches (whole-line, single-line inline, multi-line). Each branch is independent with no shared mutable state beyond the clipboard.

## R8: Search Navigation Helper

**Decision**: Extract `navigateToMatch(search *SearchState, pane *Pane, cursorRow, cursorCol int)` that combines `FindFirstMatchAfterCursor` + cursor positioning.

**Rationale**: Pattern repeated identically 3 times at editor_search.go:30-35, 110-116, 275-278.

## R9: Undo/Redo Simplification

**Decision**: Keep `applyRemoveText` and `applyInsertText` separate. Add comments documenting mirror relationship.

**Rationale**: While structurally mirrored, unifying them into a single function with a direction parameter would make the code harder to follow. The operations are conceptually different (insert vs remove) and each is only ~30 lines. The risk/benefit ratio doesn't favor merging. Instead, add a brief comment block documenting the mirror relationship.

**Alternatives considered**: Unified `applyChange(buf, change, isInsert bool)` → rejected because conditionals inside would be as complex as having two functions.

## R10: Key Mapping Timeout Completion

**Decision**: Complete the timeout by posting a custom tcell event when timeout expires, and handling it in the event loop.

**Current State** (editor.go:473-483):
- Timeout constant exists: `MappingTimeout = 500ms` (constants.go:13-14)
- State exists: `pendingMapKeys string`, `mapKeyTime time.Time` (editor.go:51-53)
- Prefix matching works (lines 449-462)
- Exact match works (lines 464-471)
- **Missing**: goroutine at line 477 sleeps but does nothing — no event posted
- **Missing**: no timeout check in main event loop
- **Missing**: no race condition protection

**Implementation Plan**:
1. Define custom event type implementing `tcell.Event` interface
2. In timeout goroutine: post custom event via `e.screen.PostEvent()`
3. In main loop `handleKey()`: handle custom event → check if `mapKeyTime` still matches → flush pending keys
4. Use snapshot of `mapKeyTime` in goroutine to avoid race (compare on receipt)
