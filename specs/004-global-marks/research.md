# Research: Global Marks Navigation

**Feature**: 004-global-marks
**Date**: 2026-02-05

## Current Implementation Analysis

### Existing Mark System (003-marks-navigation)

**Location**: `editor/marks.go` + `editor/buffer.go` + `editor/editor.go`

**Current Structure**:
```go
// marks.go
type Mark struct {
    Row int // Line number (0-indexed)
    Col int // Column position (0-indexed)
}

// buffer.go - marks are per-buffer
type Buffer struct {
    // ...
    Marks map[rune]Mark
}

// editor.go - mark operations
func (e *Editor) setMark(id rune) {
    buf := e.activeBuffer()
    buf.Marks[id] = Mark{Row: buf.CursorRow, Col: buf.CursorCol}
}

func (e *Editor) jumpToMark(id rune) {
    buf := e.activeBuffer()
    mark, exists := buf.Marks[id]
    if !exists { return }
    // ... clamp and position cursor
}
```

**Key Observations**:
1. Marks are stored per-buffer in `Buffer.Marks`
2. `setMark()` only writes to current buffer
3. `jumpToMark()` only looks in current buffer
4. No cross-buffer awareness exists

### Split View Mechanism

**Location**: `editor/editor.go`

```go
type Editor struct {
    buffers    []*Buffer
    activePane int
    // ...
}

func (e *Editor) activeBuffer() *Buffer {
    return e.buffers[e.activePane]
}
```

**Pane Switching**: `Shift+Tab` or double-shift switches `activePane`

## Design Decision: Global Registry Location

### Option A: Editor-level global map
**Decision**: ✅ CHOSEN

```go
type Editor struct {
    globalMarks map[rune]GlobalMark
}

type GlobalMark struct {
    Buffer *Buffer  // Reference to target buffer
    Row    int
    Col    int
}
```

**Rationale**:
- Editor owns all buffers, natural place for cross-buffer state
- Simple pointer comparison to determine if pane switch needed
- Clean invalidation when buffer is removed

### Option B: Separate MarkRegistry struct
**Decision**: ❌ REJECTED

**Rationale**:
- Over-engineering for 62 marks max
- Adds unnecessary abstraction layer
- No benefit over simple map

## Design Decision: Buffer Reference Strategy

### Option A: Store *Buffer pointer
**Decision**: ✅ CHOSEN

```go
type GlobalMark struct {
    Buffer *Buffer
    Row    int
    Col    int
}
```

**Rationale**:
- Direct pointer comparison (`mark.Buffer == e.activeBuffer()`)
- Automatic invalidation if buffer is closed (pointer becomes stale, but we track buffers in slice)
- Fast lookup for pane index

### Option B: Store buffer index
**Decision**: ❌ REJECTED

**Rationale**:
- Indices change if buffers reorder
- Requires additional validation logic

### Option C: Store filename
**Decision**: ❌ REJECTED

**Rationale**:
- String comparison overhead
- Same file could theoretically be in both panes (edge case)
- Loses direct buffer reference

## Design Decision: Mark Invalidation

### When to invalidate marks:
1. When target buffer is closed/replaced → Mark becomes invalid
2. When jumping to mark with non-existent buffer → No-op (cursor unchanged)

### Implementation:
```go
func (e *Editor) jumpToMark(id rune) {
    mark, exists := e.globalMarks[id]
    if !exists { return }

    // Find pane containing this buffer
    paneIdx := -1
    for i, buf := range e.buffers {
        if buf == mark.Buffer {
            paneIdx = i
            break
        }
    }

    // Buffer no longer open - mark is invalid
    if paneIdx == -1 { return }

    // Switch pane if needed
    if paneIdx != e.activePane {
        e.activePane = paneIdx
    }

    // Position cursor (with clamping)
    buf := e.buffers[paneIdx]
    // ... clamp row/col and set cursor
}
```

## Migration Strategy

### Backwards Compatibility
- Single-file mode: `e.buffers` has 1 element, pane never switches
- Behavior identical to 003-marks-navigation from user perspective

### Breaking Change
- Remove `Buffer.Marks` field entirely
- All marks now global in `Editor.globalMarks`
- This is internal refactoring; external behavior improves

## Alternatives Considered

### Vim-style local vs global marks (a-z local, A-Z global)
**Decision**: ❌ NOT IMPLEMENTED

**Rationale**:
- Adds complexity for minimal benefit
- EF editor is simpler than vim
- Users expect consistent behavior across all identifiers
- Spec explicitly states: "All mark identifiers become global"

## Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Registry location | Editor-level map | Natural ownership, simple |
| Buffer reference | *Buffer pointer | Direct comparison, fast |
| Invalidation | Check buffer in slice | Clean, automatic |
| Local vs global | All global | Simplicity, per spec |
