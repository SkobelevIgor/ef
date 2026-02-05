# Quickstart: Marks Navigation

**Feature**: 003-marks-navigation
**Date**: 2026-02-05

## Overview

This feature adds vim-style marks to the FE editor, allowing users to save cursor positions and jump back to them later.

## Usage

### Setting a Mark

1. Position cursor where you want to create a mark
2. Press `m` (you'll see `m_` in the status area)
3. Press an alphanumeric identifier (0-9, a-z, A-Z)
4. Mark is saved silently

**Example**: Press `m1` to set mark "1" at current position

### Jumping to a Mark

1. Press `` ` `` (backtick) - you'll see `` `_ `` in status area
2. Press the identifier of the mark you want to jump to
3. Cursor moves to the marked position

**Example**: Press `` `1 `` to jump to mark "1"

### Cancelling

Press `Escape` while waiting for identifier to cancel the operation.

## Key Bindings

| Keys | Action | Mode |
|------|--------|------|
| `m` + `<id>` | Set mark at current position | Normal |
| `` ` `` + `<id>` | Jump to mark | Normal |

Where `<id>` is any alphanumeric character: 0-9, a-z, A-Z

## Limitations

- Marks only work in Normal mode
- Marks are per-buffer (each file has its own marks)
- Marks are not persisted between sessions
- Maximum 62 marks per file

## Testing Checklist

### Basic Functionality

- [ ] Open a file with multiple lines
- [ ] Go to line 10, press `m1` - verify `m_` appears briefly
- [ ] Go to line 50
- [ ] Press `` `1 `` - verify cursor jumps to line 10
- [ ] Verify cursor column is also restored

### Multiple Marks

- [ ] Set mark `a` at line 5
- [ ] Set mark `b` at line 20
- [ ] Set mark `c` at line 40
- [ ] Jump to each mark and verify correct position

### Overwrite

- [ ] Set mark `1` at line 10
- [ ] Set mark `1` at line 30 (same identifier)
- [ ] Jump to mark `1` - should go to line 30

### Invalid Operations

- [ ] Press `` `9 `` without setting mark 9 - cursor should not move
- [ ] Press `m` then `Escape` - should cancel
- [ ] Press `` ` `` then `Escape` - should cancel

### Edge Cases

- [ ] Set mark at line 100
- [ ] Delete lines so file has only 50 lines
- [ ] Jump to mark - should go to end of file

### Mode Check

- [ ] Enter Insert mode
- [ ] Type `m1` - should insert "m1" as text, not set mark
- [ ] Press Escape to return to Normal mode
- [ ] Now `m1` should set a mark

## Rebuild Command

After implementation, rebuild the binary:

```bash
rm -f ef && go build -o ef .
```
