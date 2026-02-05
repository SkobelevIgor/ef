# Quickstart: Global Marks Navigation

**Feature**: 004-global-marks
**Date**: 2026-02-05

## Prerequisites

- Go 1.21+ installed
- Repository cloned and dependencies fetched

## Build

```bash
rm -f ef && go build -o ef .
```

## Test Scenarios

### Scenario 1: Cross-File Mark Jump (Core Feature)

1. Open two files in split view:
   ```bash
   ./ef file1.go file2.go
   ```

2. In file1 (left pane), navigate to line 25:
   ```
   :25<Enter>
   ```

3. Set mark "1":
   ```
   m1
   ```

4. Switch to file2 (right pane):
   ```
   Shift+Tab
   ```

5. Navigate somewhere else, then jump to mark "1":
   ```
   `1
   ```

6. **Expected**: Pane switches back to file1, cursor at line 25

### Scenario 2: Mark Overwrite from Different File

1. With two files open, set mark "a" in file1
2. Switch to file2
3. Set mark "a" in file2 (overwrites)
4. Switch back to file1
5. Press `` `a ``
6. **Expected**: Pane switches to file2

### Scenario 3: Same-File Mark (No Pane Switch)

1. With two files open, stay in file1
2. Set mark "b" at line 10
3. Navigate to line 50
4. Press `` `b ``
5. **Expected**: Cursor moves to line 10, NO pane switch

### Scenario 4: Single-File Mode Compatibility

1. Open single file:
   ```bash
   ./ef file1.go
   ```

2. Set mark "c" at current position
3. Navigate elsewhere
4. Press `` `c ``
5. **Expected**: Cursor returns to marked position (identical to old behavior)

### Scenario 5: Invalid Mark (Closed Buffer)

1. Open two files
2. Set mark "d" in file2
3. Close editor, reopen with only file1:
   ```bash
   ./ef file1.go
   ```
4. Press `` `d ``
5. **Expected**: Nothing happens (mark references non-existent buffer)

### Scenario 6: Edge Case - Deleted Line

1. Set mark at line 50
2. Delete lines 40-60
3. Jump to mark
4. **Expected**: Cursor moves to closest valid position (end of file if beyond)

## Verification Checklist

- [ ] Cross-file mark jump switches pane correctly
- [ ] Mark overwrite works across files
- [ ] Same-file marks don't cause pane switch
- [ ] Single-file mode works identically to before
- [ ] Invalid mark jumps are no-ops
- [ ] Position clamping works for deleted lines
- [ ] All 62 identifiers work (0-9, a-z, A-Z)
- [ ] Status bar shows pending mark indicator (`m_` or `` `_ ``)
