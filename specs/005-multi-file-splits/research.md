# Research: Multi-File Splits

**Feature**: 005-multi-file-splits
**Date**: 2026-02-05

## R1: CLI Flag Parsing Approach

### Question
How should we handle the `-v` flag for vertical split mode while maintaining minimal dependencies?

### Decision
Use manual `os.Args` parsing without external flag libraries.

### Rationale
1. **Minimal dependencies**: Constitution requires minimal dependencies; currently only tcell is used
2. **Simple requirement**: Single optional flag doesn't warrant a flag parsing library
3. **Consistency**: Existing `parseFileArg()` already uses manual parsing
4. **Flexibility**: Easy to extend if more flags needed later

### Alternatives Considered

| Option | Pros | Cons |
|--------|------|------|
| `flag` stdlib | Built-in, standardized | Doesn't mix well with positional args after flag |
| `pflag` | POSIX-style flags | External dependency |
| Manual parsing | No deps, full control | More code, less standardized |

### Implementation Notes
```go
// In main.go
args := os.Args[1:]
splitMode := editor.SplitHorizontal
if len(args) > 0 && args[0] == "-v" {
    splitMode = editor.SplitVertical
    args = args[1:]
}
```

---

## R2: Multi-Pane Layout Algorithm

### Question
How should pane dimensions be calculated for N panes?

### Decision
Equal division with remainder distribution to last pane.

### Rationale
1. **Simplicity**: Integer division is straightforward
2. **Fairness**: Equal space allocation is intuitive
3. **Edge handling**: Extra pixels/lines go to last pane (minimal visual impact)

### Algorithm

**Horizontal splits** (stacked top-to-bottom):
```
totalHeight = screenHeight - searchBarHeight
paneHeight = totalHeight / numPanes
lastPaneHeight = totalHeight - (paneHeight * (numPanes - 1))
```

**Vertical splits** (side-by-side):
```
totalWidth = screenWidth
paneWidth = totalWidth / numPanes
lastPaneWidth = totalWidth - (paneWidth * (numPanes - 1))
separatorWidth = 1 per separator (numPanes - 1 total)
```

### Separator Handling
- Horizontal: `─` character drawn between panes (full width)
- Vertical: `│` character drawn between panes (full height)
- Separators reduce available content space

---

## R3: Minimum Dimension Constraints

### Question
What minimum dimensions ensure usable panes?

### Decision
- Horizontal: minimum 3 lines per pane
- Vertical: minimum 20 characters per pane

### Rationale

**Horizontal (3 lines)**:
- 1 line for content is too small to be useful
- 3 lines allows seeing context around cursor
- Matches typical vim split minimum

**Vertical (20 characters)**:
- Line numbers take ~4-5 characters
- Need ~15+ characters for actual content
- 20 is conservative but usable

### Enforcement
1. At startup: Calculate max panes that fit, warn if requested > max
2. At render: Skip panes that can't fit minimum dimensions
3. On resize: Dynamically adjust visible panes

---

## R4: Pane Switching Behavior

### Question
Does existing pane switching scale to N panes?

### Decision
Existing implementation already works - no changes needed.

### Analysis
Current code in `checkDoubleShift()`:
```go
e.activePane = (e.activePane + 1) % len(e.buffers)
```

This modulo arithmetic naturally cycles through any number of panes:
- 2 panes: 0 → 1 → 0
- 3 panes: 0 → 1 → 2 → 0
- N panes: 0 → 1 → ... → N-1 → 0

No changes required for pane switching logic.

---

## R5: Cursor and Autocomplete Positioning

### Question
How should cursor and autocomplete dropdown positioning work with variable pane layouts?

### Decision
Compute cursor screen position based on pane offset.

### Implementation
Each pane has a computed (startX, startY) offset. Cursor position is:
```go
cursorScreenX = pane.startX + cursorLocalX
cursorScreenY = pane.startY + cursorLocalY
```

Autocomplete dropdown should:
1. Anchor to cursor position
2. Check if there's room below, otherwise show above
3. Constrain to pane boundaries (don't overflow into adjacent panes)
