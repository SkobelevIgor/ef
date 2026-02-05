# Quickstart: Multi-File Splits

**Feature**: 005-multi-file-splits
**Date**: 2026-02-05
**Updated**: 2026-02-05 (Change request: vertical splits now default, -h flag for horizontal)

## Build

```bash
# From repository root
go build -o ef .

# Move to ignore directory (per CLAUDE.md)
mv ef ignore/
```

## Test Scenarios

### Basic: Multiple Files with Vertical Splits (Default)

```bash
# Create test files
echo "File A content" > /tmp/a.txt
echo "File B content" > /tmp/b.txt
echo "File C content" > /tmp/c.txt
echo "File D content" > /tmp/d.txt

# Open 3 files (vertical splits - side-by-side, default)
./ignore/ef /tmp/a.txt /tmp/b.txt /tmp/c.txt

# Open 4 files
./ignore/ef /tmp/a.txt /tmp/b.txt /tmp/c.txt /tmp/d.txt
```

**Expected**: Files displayed side-by-side left-to-right with vertical separators (`│`).

### Horizontal Splits with -h Flag

```bash
# Open 3 files stacked top-to-bottom
./ignore/ef -h /tmp/a.txt /tmp/b.txt /tmp/c.txt

# Open 4 files stacked
./ignore/ef -h /tmp/a.txt /tmp/b.txt /tmp/c.txt /tmp/d.txt
```

**Expected**: Files displayed stacked top-to-bottom with horizontal separators (`─`).

### Line Number Positioning

```bash
# Open files at specific lines (vertical splits - default)
./ignore/ef /tmp/a.txt:10 /tmp/b.txt:20 /tmp/c.txt:5

# With horizontal splits
./ignore/ef -h /tmp/a.txt:10 /tmp/b.txt:20
```

**Expected**: Each file opens at the specified line number.

### Pane Navigation

1. Open multiple files: `./ignore/ef /tmp/a.txt /tmp/b.txt /tmp/c.txt`
2. Press `Shift+Tab` to cycle through panes
3. Verify cursor moves to next pane in order
4. Verify active pane indicator (line number color) changes

**Expected**: Panes cycle 0 → 1 → 2 → 0 with each Shift+Tab.

### Terminal Resize

1. Open multiple files
2. Resize terminal window
3. Verify panes resize proportionally
4. Verify no visual glitches

### Edge Case: Too Many Files

```bash
# Try to open more files than can fit (depends on terminal size)
# On a narrow terminal (80 cols), 4+ files in vertical mode may hit minimum width limits
./ignore/ef /tmp/a.txt /tmp/b.txt /tmp/c.txt /tmp/d.txt /tmp/e.txt /tmp/f.txt /tmp/g.txt /tmp/h.txt
```

**Expected**: Opens as many files as fit with minimum dimensions, warns about skipped files.

### Edge Case: No Files

```bash
./ignore/ef
./ignore/ef -h
```

**Expected**: Usage error message displayed: "Usage: ef [-h] <filename[:line]> [filename2[:line]] ..."

## Feature Verification Checklist

- [ ] 3+ files open in vertical splits (default)
- [ ] 3+ files open in horizontal splits (-h flag)
- [ ] Shift+Tab cycles through all panes
- [ ] Line number colors indicate active pane
- [ ] Terminal resize works without glitches
- [ ] `:line` syntax works with all split modes
- [ ] Search (F4) works across all panes
- [ ] Global marks (m/\`) work across all panes
- [ ] Autocomplete works in all panes
- [ ] Auto-save works for all open files
- [ ] File watching detects changes in all files

## Cleanup

```bash
rm /tmp/a.txt /tmp/b.txt /tmp/c.txt /tmp/d.txt
```
