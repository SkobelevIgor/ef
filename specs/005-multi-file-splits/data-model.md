# Data Model: Multi-File Splits

**Feature**: 005-multi-file-splits
**Date**: 2026-02-05

## New Types

### SplitMode

```go
// SplitMode determines how multiple panes are arranged on screen
type SplitMode int

const (
    // SplitHorizontal arranges panes top-to-bottom (stacked)
    // This is the default when no -v flag is provided
    SplitHorizontal SplitMode = iota

    // SplitVertical arranges panes left-to-right (side-by-side)
    // Activated with the -v command line flag
    SplitVertical
)
```

### Pane (computed at render time, not stored)

```go
// Pane represents the computed layout for a single buffer viewport
// This is calculated during rendering based on screen size and split mode
type Pane struct {
    BufferIndex int    // Index into Editor.buffers slice
    StartX      int    // Screen X coordinate of pane's top-left corner
    StartY      int    // Screen Y coordinate of pane's top-left corner
    Width       int    // Pane width in characters
    Height      int    // Pane height in lines
}
```

## Modified Types

### Editor (editor/editor.go)

**Before**:
```go
type Editor struct {
    buffers       []*Buffer
    activePane    int
    screen        *Screen
    // ... other fields
}
```

**After**:
```go
type Editor struct {
    buffers       []*Buffer
    activePane    int
    screen        *Screen
    splitMode     SplitMode  // NEW: horizontal or vertical layout
    // ... other fields
}
```

### FileInfo (unchanged)

The existing `FileInfo` struct remains unchanged:
```go
type FileInfo struct {
    Filename string
    Line     int // 1-based line number, 0 means not specified
}
```

## Function Signature Changes

### editor.New()

**Before**:
```go
func New(fileInfos []FileInfo) (*Editor, error)
```

**After**:
```go
func New(fileInfos []FileInfo, splitMode SplitMode) (*Editor, error)
```

### screen.Render()

**Before**:
```go
func (s *Screen) Render(buffers []*Buffer, activePane int, mode Mode, inputState *InputState)
```

**After**:
```go
func (s *Screen) Render(buffers []*Buffer, activePane int, mode Mode, inputState *InputState, splitMode SplitMode)
```

## Constants

```go
const (
    // MinPaneHeightHorizontal is the minimum height for horizontal splits
    MinPaneHeightHorizontal = 3

    // MinPaneWidthVertical is the minimum width for vertical splits
    MinPaneWidthVertical = 20
)
```

## Entity Relationships

```
┌─────────────────────────────────────────────────────────────┐
│                         Editor                               │
│  - buffers: []*Buffer (1..N files)                          │
│  - activePane: int (index into buffers)                     │
│  - splitMode: SplitMode (horizontal/vertical)               │
│  - screen: *Screen                                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ renders to
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                         Screen                               │
│  - screen: tcell.Screen                                     │
│  - Render(): computes Pane layouts, draws all panes         │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ computes at render time
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    []Pane (computed)                         │
│  For each buffer:                                           │
│  - BufferIndex, StartX, StartY, Width, Height               │
└─────────────────────────────────────────────────────────────┘
```

## State Transitions

### SplitMode
- Set once at startup based on `-v` flag
- Immutable during editor session
- No runtime switching between modes

### Pane Layout
- Recalculated on every `Render()` call
- Responds to terminal resize events
- Adjusts visible panes based on minimum dimension constraints

## Invariants

1. `len(buffers) >= 1` - At least one file must be open
2. `0 <= activePane < len(buffers)` - Active pane index always valid
3. `splitMode ∈ {SplitHorizontal, SplitVertical}` - Only two modes
4. All buffers are independent (no shared state between panes)
