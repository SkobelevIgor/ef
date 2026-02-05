# Data Model: Global Marks Navigation

**Feature**: 004-global-marks
**Date**: 2026-02-05

## Entities

### GlobalMark

Represents a saved cursor position with buffer reference for cross-file navigation.

| Field | Type | Description |
|-------|------|-------------|
| Buffer | *Buffer | Pointer to the buffer containing this mark |
| Row | int | Line number (0-indexed) |
| Col | int | Column position (0-indexed) |

**Validation Rules**:
- Buffer must be non-nil when mark is set
- Row and Col are clamped to valid range when jumping (line may have been deleted)

**Lifecycle**:
- Created: When user presses `m<id>` in Normal mode
- Updated: When user sets same mark identifier again (overwrites)
- Invalid: When target buffer is closed (mark remains in registry but jump is no-op)
- Destroyed: When editor session ends (non-persistent)

### GlobalMarkRegistry (conceptual)

The global mark registry is implemented as a simple map in the Editor struct.

```go
type Editor struct {
    // ... existing fields ...
    globalMarks map[rune]GlobalMark
}
```

| Property | Value |
|----------|-------|
| Max entries | 62 (0-9, a-z, A-Z) |
| Key type | rune (mark identifier) |
| Persistence | None (in-memory only) |
| Scope | Editor session |

## Relationships

```
Editor (1) ----owns----> (*) GlobalMark
Editor (1) ----owns----> (1..2) Buffer
GlobalMark (*) ----references----> (1) Buffer
```

## State Transitions

### Mark Lifecycle

```
[Not Set] ---(m<id>)---> [Active]
[Active] ---(m<id> same id)---> [Active] (updated position/buffer)
[Active] ---(buffer closed)---> [Stale]
[Stale] ---(`<id>)---> [No-op] (cursor unchanged)
```

### Jump Behavior

```
User presses `<id>
    |
    v
Mark exists in registry?
    |-- No --> No-op (cursor unchanged)
    |-- Yes
        |
        v
    Buffer still in e.buffers?
        |-- No --> No-op (cursor unchanged)
        |-- Yes
            |
            v
        Same buffer as active?
            |-- Yes --> Jump within pane
            |-- No --> Switch activePane, then jump
```

## Removed Entities

### Buffer.Marks (REMOVED)

Previously each Buffer had its own marks map:

```go
// OLD - to be removed
type Buffer struct {
    Marks map[rune]Mark  // REMOVE this field
}
```

This is replaced by the global `Editor.globalMarks` map.

## Code Changes Summary

### marks.go

```go
// Existing - keep unchanged
type Mark struct {
    Row int
    Col int
}

func isValidMarkIdentifier(r rune) bool {
    return unicode.IsLetter(r) || unicode.IsDigit(r)
}

// NEW - add GlobalMark
type GlobalMark struct {
    Buffer *Buffer
    Row    int
    Col    int
}
```

### editor.go

```go
type Editor struct {
    // ... existing fields ...
    globalMarks map[rune]GlobalMark  // NEW
}

func New(fileInfos []FileInfo) (*Editor, error) {
    // ... existing code ...
    return &Editor{
        // ... existing fields ...
        globalMarks: make(map[rune]GlobalMark),  // NEW
    }, nil
}
```

### buffer.go

```go
type Buffer struct {
    // ... existing fields ...
    // Marks map[rune]Mark  // REMOVE this line
}

func NewBuffer(filename string) (*Buffer, error) {
    b := &Buffer{
        // ... existing fields ...
        // Marks: make(map[rune]Mark),  // REMOVE this line
    }
    // ...
}
```
