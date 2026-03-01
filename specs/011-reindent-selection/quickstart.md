# Quickstart: Reindent Selection

## What to Build

Add `=` key in visual mode to reindent selected lines by matching each line's indentation to the line above it.

## Files to Modify

1. **`editor/buffer.go`** — Add `ReindentRange(startRow, endRow int)` method after `UnindentRange`
2. **`editor/editor_visual.go`** — Add `'='` case in `handleVisualMode()` switch after `'<'` case

## Implementation Steps

1. Add `ReindentRange` method to `Buffer`:
   - Loop from `startRow` to `endRow`
   - For each line: get previous line's leading whitespace, strip current line's whitespace, prepend previous line's whitespace
   - Row 0 or empty previous line → 0 indentation
   - Set `b.Modified = true` and increment `b.ModCount`

2. Add `=` handler in visual mode:
   - Get selection: `startRow, _, endRow, _ := pane.GetSelection()`
   - Call `buf.ReindentRange(startRow, endRow)`
   - Exit visual mode, clear selection, reset input, schedule auto-save

## Build & Test

```bash
go build -o ef .
mv ef ignore/
./ignore/ef testfile.go
```

Test: Enter visual mode (`v`), select lines (`j`), press `=`, verify indentation matches previous lines.
