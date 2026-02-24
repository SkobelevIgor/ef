# Data Model: Fix Paste Indentation

No new persistent entities. All changes are in-memory state within the Editor and Buffer structs.

## Modified Entities

### Editor (editor.go)

**Removed fields**:
- `pasteSkipWhitespace bool` — no longer needed (batch replaces incremental)
- `pasteBaseIndent int` — no longer needed
- `pasteAutoIndent int` — no longer needed
- `pasteCollectedWS []rune` — no longer needed

**Added fields**:
- `pasteStartRow int` — row where the current paste operation began; used to determine the range for batch reindentation

**Retained fields**:
- `pasting bool` — still needed to detect paste mode
- `lastKeyTime time.Time` — still needed for timing-based paste detection

### Buffer (buffer.go)

**No structural changes**. Two new methods added:

- `ReindentLines(startRow, endRow, contextIndent int)` — batch reindent a range of lines
- `GetPrevNonEmptyLineIndent(row int) int` — find context indent from previous non-empty line

### FileTypeConfig (config.go)

**No changes**. Existing fields used as gates:
- `AutoIndentation bool` — gates whether reindentation occurs
- `ExpandTab bool` — determines tab vs space output
- `ShiftWidth int` — determines indent unit size

## State Transitions

### Paste Lifecycle (Insert Mode)

```
Idle → Pasting (EventPaste start OR timing < 5ms)
  Record pasteStartRow = buf.CursorRow
  Characters inserted normally (no whitespace interception)

Pasting → Idle (EventPaste end OR timing > 5ms)
  If AutoIndentation enabled:
    contextIndent = GetPrevNonEmptyLineIndent(pasteStartRow)
    ReindentLines(pasteStartRow, buf.CursorRow, contextIndent)
  Reset pasting state
```

### Put Lifecycle (Normal/Visual Mode p/P)

```
p/P triggered → snapshot oldLines
  Execute pasteAfter/pasteBefore → returns (firstRow, lastRow)
  If AutoIndentation enabled:
    contextIndent = GetPrevNonEmptyLineIndent(firstRow)
    ReindentLines(firstRow, lastRow, contextIndent)
  Push ChangeReplace to history
```
