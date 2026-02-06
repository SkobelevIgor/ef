# Research: Shared Buffer Synchronization

**Feature**: 006-shared-buffer
**Date**: 2026-02-06

## Research Questions

### 1. Buffer State Separation Strategy

**Question**: How to separate shared buffer state from per-pane view state while minimizing code changes?

**Decision**: Extract view state to new Pane struct; Buffer retains data only.

**Rationale**:
- Current Buffer has ~15 fields mixing data (Lines, Modified, Filename) with view state (CursorRow, CursorCol, ScrollOffset, Selection*)
- Clean separation allows multiple Panes to reference single Buffer
- Go embedding not suitable here - composition via pointer reference cleaner
- Pane becomes the "view" abstraction; Buffer becomes the "model"

**Alternatives Considered**:
1. **Keep cursor in Buffer, sync on change**: Would require iterating all panes on every keystroke to sync cursors - rejected due to complexity and potential for drift
2. **Store cursors in Editor by buffer path**: Creates parallel data structure that must stay in sync - rejected due to maintenance burden
3. **Clone Buffer per pane, sync Lines only**: Would need sync mechanism and break undo history - rejected as over-complex

### 2. Cursor Adjustment Algorithm

**Question**: How to adjust cursors in non-active panes when lines are added/removed?

**Decision**: Implement `AdjustCursorsForEdit(editRow, linesDelta)` called after each content-modifying operation.

**Rationale**:
- Cursors only need adjustment when lines change (insertions, deletions)
- Simple algorithm: if cursor row >= edit row, adjust by linesDelta
- For deletions within a range, clamp cursor to valid range
- Column adjustments only needed if editing on same row as cursor

**Algorithm**:
```
For each pane viewing the same buffer (excluding active pane):
  If linesDelta > 0 (insertion):
    If pane.CursorRow >= editRow:
      pane.CursorRow += linesDelta
  If linesDelta < 0 (deletion):
    deletedStart = editRow
    deletedEnd = editRow + abs(linesDelta) - 1
    If pane.CursorRow > deletedEnd:
      pane.CursorRow += linesDelta  # Shift up
    Else if pane.CursorRow >= deletedStart:
      pane.CursorRow = deletedStart  # Clamp to deletion point
    Clamp pane.CursorRow to [0, len(Lines)-1]
    Clamp pane.CursorCol to line length
```

**Alternatives Considered**:
1. **Line markers/anchors**: Track cursors as persistent positions that survive edits - rejected as over-engineered for this use case
2. **Event-based cursor updates**: Emit events for each edit, panes subscribe - rejected as unnecessary complexity in single-threaded model

### 3. Buffer Registry Design

**Question**: How to deduplicate buffers when same file is opened multiple times?

**Decision**: Use `map[string]*Buffer` keyed by absolute path in Editor struct.

**Rationale**:
- Absolute path is canonical identifier for file identity
- Map lookup is O(1) for checking if buffer exists
- `filepath.Abs()` normalizes relative paths and symlinks
- Editor.New() builds registry from file arguments before creating panes

**Implementation**:
```go
type Editor struct {
    bufferRegistry map[string]*Buffer  // key: absolute path
    panes          []*Pane
    activePane     int
    // ...
}

func (e *Editor) getOrCreateBuffer(filename string) (*Buffer, error) {
    absPath, err := filepath.Abs(filename)
    if err != nil {
        return nil, err
    }
    if buf, exists := e.bufferRegistry[absPath]; exists {
        return buf, nil
    }
    buf, err := NewBufferWithRegistry(filename, e.fileTypeRegistry)
    if err != nil {
        return nil, err
    }
    e.bufferRegistry[absPath] = buf
    return buf, nil
}
```

**Alternatives Considered**:
1. **Slice-based linear search**: O(n) lookup, but n is small (typically <10 files) - viable but map is cleaner
2. **inode-based identity**: More robust for hard links but platform-specific - rejected for simplicity

### 4. Mode Per Pane vs Global Mode

**Question**: Should editing mode (Normal/Insert/Visual) be per-pane or global?

**Decision**: Mode remains global (in Editor), but session tracking becomes per-pane.

**Rationale**:
- User interacts with one pane at a time; mode affects active pane only
- Switching panes while in Insert mode is already handled (commits session, starts new)
- Global mode simplifies rendering (status line shows single mode)
- Per-pane mode would add complexity without clear user benefit

**Alternatives Considered**:
1. **Per-pane mode**: Each pane could be in different mode - rejected as confusing UX and no apparent use case
2. **Mode sync across shared-buffer panes**: When one pane enters Insert, all do - rejected as unexpected behavior

### 5. Undo History Scope

**Question**: Should undo history be per-buffer or global across all buffers?

**Decision**: Keep current global history design (already implemented correctly).

**Rationale**:
- History already stores Buffer reference in Change struct
- Undo/redo already switches to correct pane automatically
- Shared buffer means changes from any pane viewing it are recorded once
- No changes needed to history.go beyond ensuring Pane cursor adjustment after undo

**Verification**:
- Current code: `Change.Buffer *Buffer` field exists
- Current code: `undo()` switches `activePane` to match `change.Buffer`
- This naturally works with shared buffers since Buffer pointer is same across panes

## Resolved Clarifications

All technical clarifications resolved. No blockers for Phase 1 design.

## Dependencies

- No external dependencies required
- Relies on existing tcell for rendering
- Uses standard library `path/filepath` for path normalization
