# Quickstart: Shared Buffer Synchronization

**Feature**: 006-shared-buffer
**Date**: 2026-02-06

## Overview

This feature enables opening the same file in multiple panes with synchronized editing. Changes in one pane immediately appear in all other panes showing the same file.

## Usage

```bash
# Open same file twice (side by side by default)
ef file.txt file.txt

# Open same file twice with horizontal split
ef -h file.txt file.txt

# Mix: different files and same file
ef main.go main.go utils.go
```

## Key Behaviors

### Content Synchronization

When you edit in one pane, all other panes showing the same file update immediately:

```
┌─────────────────┬─────────────────┐
│ file.txt (1)    │ file.txt (2)    │
│                 │                 │
│ Hello█          │ Hello           │  ← Type "Hello" in pane 1
│                 │                 │  ← Pane 2 shows it instantly
└─────────────────┴─────────────────┘
```

### Independent Cursors

Each pane maintains its own cursor position:

```
┌─────────────────┬─────────────────┐
│ file.txt (1)    │ file.txt (2)    │
│                 │                 │
│ Line 1          │ Line 1          │
│ Line 2█         │ Line 2          │  ← Cursor at line 2 in pane 1
│ Line 3          │ Line 3█         │  ← Cursor at line 3 in pane 2
└─────────────────┴─────────────────┘
```

### Cursor Adjustment

When lines are added/removed, cursors in other panes adjust automatically:

```
Before: Pane 1 cursor at line 2, Pane 2 cursor at line 5
Action: Insert 2 lines at line 3 in Pane 1
After:  Pane 1 cursor at line 2 (unchanged), Pane 2 cursor at line 7 (adjusted +2)
```

### Unified Undo

Undo/redo works across all panes - the most recent change is undone regardless of which pane made it:

```
1. Type "A" in Pane 1
2. Type "B" in Pane 2
3. Press 'u' in either pane → "B" is undone
4. Press 'u' again → "A" is undone
```

## Pane Navigation

- **Shift+Tab**: Switch to next pane
- **Double-Shift**: Switch to next pane (alternative)

## Shared State

These are shared across all panes viewing the same file:
- File content (Lines)
- Modified indicator
- Local marks (a-z)
- Undo/redo history

## Independent State

Each pane maintains its own:
- Cursor position
- Scroll position
- Visual selection
- Search state

## Testing Checklist

1. [ ] Open `ef test.txt test.txt` - verify two panes show same content
2. [ ] Type in pane 1 - verify text appears in pane 2
3. [ ] Move cursor in pane 2 - verify pane 1 cursor unchanged
4. [ ] Insert lines in pane 1 above pane 2's cursor - verify pane 2 cursor adjusts
5. [ ] Make edits in both panes, press undo - verify correct order
6. [ ] Set mark in pane 1 (`ma`), jump to it from pane 2 (`` `a ``) - verify shared
7. [ ] External edit file in vim - verify both panes update
