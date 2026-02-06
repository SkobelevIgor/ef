# Data Model: Shared Buffer Synchronization

**Feature**: 006-shared-buffer
**Date**: 2026-02-06

## Entity Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         Editor                               │
│  ┌─────────────────────┐    ┌───────────────────────────┐   │
│  │   bufferRegistry    │    │         panes             │   │
│  │  map[string]*Buffer │    │       []*Pane             │   │
│  └──────────┬──────────┘    └─────────────┬─────────────┘   │
│             │                             │                  │
│             │         ┌───────────────────┼──────────┐      │
│             │         │                   │          │      │
│             ▼         ▼                   ▼          ▼      │
│       ┌─────────┐  ┌──────┐          ┌──────┐   ┌──────┐   │
│       │ Buffer  │◄─┤ Pane │          │ Pane │   │ Pane │   │
│       │ (file1) │  │  #0  │          │  #1  │   │  #2  │   │
│       └─────────┘  └──────┘          └──────┘   └──────┘   │
│             ▲                             │                  │
│             │         ┌───────────────────┘                 │
│             │         │                                      │
│       ┌─────────┐  ┌──────┐                                 │
│       │ Buffer  │◄─┤ Pane │  (same file as #0)              │
│       │ (file2) │  │  #3  │                                 │
│       └─────────┘  └──────┘                                 │
└─────────────────────────────────────────────────────────────┘
```

## Entities

### Buffer (Shared Data)

Represents the in-memory content of a single file. Multiple panes can reference the same buffer.

| Field | Type | Description |
|-------|------|-------------|
| Lines | `[][]rune` | File content as slice of lines |
| Filename | `string` | File path (used for save/load) |
| Modified | `bool` | True if unsaved changes exist |
| LastModTime | `time.Time` | Last known file modification time |
| FileType | `*FileType` | Detected file type for syntax |
| HighlightCache | `[][]Highlight` | Cached syntax highlighting |
| Config | `FileConfig` | Tab settings, indentation, etc. |
| Marks | `map[rune]Mark` | Local marks (a-z) |

**Identity**: Buffers are identified by absolute file path. Same path = same buffer instance.

**Lifecycle**:
1. Created when file first opened
2. Shared when same file opened again
3. Persisted on save (all panes see saved state)
4. Destroyed when editor closes (no pane references)

### Pane (Per-View State)

Represents one visual view of a buffer. Each pane has independent navigation state.

| Field | Type | Description |
|-------|------|-------------|
| Buffer | `*Buffer` | Reference to shared buffer |
| CursorRow | `int` | Current cursor line (0-indexed) |
| CursorCol | `int` | Current cursor column (0-indexed) |
| ScrollOffset | `int` | First visible line |
| SelectionActive | `bool` | Whether visual selection is active |
| SelectionStartRow | `int` | Selection anchor row |
| SelectionStartCol | `int` | Selection anchor column |

**Identity**: Panes are identified by index in Editor.panes slice.

**Lifecycle**:
1. Created for each file argument on command line
2. Lives for editor session duration
3. Never destroyed during session (no pane close feature)

### Editor (Controller)

Orchestrates buffers and panes.

| Field | Type | Description |
|-------|------|-------------|
| bufferRegistry | `map[string]*Buffer` | Buffers keyed by absolute path |
| panes | `[]*Pane` | All panes in display order |
| activePane | `int` | Index of currently active pane |
| mode | `Mode` | Current editing mode (global) |
| history | `*History` | Undo/redo stack |
| fileWatcher | `*FileWatcher` | External change detection |
| splitMode | `SplitMode` | Horizontal or vertical layout |
| globalMarks | `map[rune]GlobalMark` | Cross-file marks (A-Z) |

### Change (History Entry)

Already exists and correctly references Buffer. No changes needed.

| Field | Type | Description |
|-------|------|-------------|
| Type | `ChangeType` | Insert, Delete, or Replace |
| Buffer | `*Buffer` | Buffer this change applies to |
| Row, Col | `int` | Position of change |
| Text | `[][]rune` | Changed text content |
| OldText | `[][]rune` | Previous content (for Replace) |

## Relationships

| From | To | Cardinality | Description |
|------|-----|-------------|-------------|
| Editor | Buffer | 1:N | Editor owns buffer registry |
| Editor | Pane | 1:N | Editor owns pane list |
| Pane | Buffer | N:1 | Multiple panes can share one buffer |
| Change | Buffer | N:1 | Changes reference their target buffer |
| GlobalMark | Buffer | N:1 | Marks reference their buffer |

## State Transitions

### Buffer.Modified

```
┌───────────┐  Edit operation  ┌──────────┐
│ Modified  │◄─────────────────│Unmodified│
│  = true   │                  │ = false  │
└─────┬─────┘                  └────▲─────┘
      │                             │
      │        Save or Load         │
      └─────────────────────────────┘
```

### Pane Cursor Adjustment

When buffer content changes, cursors in non-active panes must adjust:

```
Edit at row R, delta D lines:
  For each pane P viewing same buffer (P != activePane):
    If D > 0 (lines added):
      If P.CursorRow >= R: P.CursorRow += D
    If D < 0 (lines deleted):
      If P.CursorRow > R + |D| - 1: P.CursorRow += D
      Else if P.CursorRow >= R: P.CursorRow = R
    Clamp P.CursorRow to [0, len(Buffer.Lines)-1]
    Clamp P.CursorCol to [0, len(Buffer.Lines[P.CursorRow])]
```

## Validation Rules

1. **Buffer path uniqueness**: No two buffers may have same absolute path
2. **Pane buffer reference**: Every pane must reference a valid buffer
3. **Cursor bounds**: CursorRow must be in [0, len(Lines)-1], CursorCol in [0, len(Lines[row])]
4. **Active pane bounds**: activePane must be in [0, len(panes)-1]
5. **Non-empty buffer**: Buffer.Lines must always have at least one element (empty line)
