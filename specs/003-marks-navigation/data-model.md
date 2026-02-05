# Data Model: Marks Navigation

**Feature**: 003-marks-navigation
**Date**: 2026-02-05

## Entities

### Mark

Represents a saved cursor position in a buffer.

| Field | Type | Description | Constraints |
|-------|------|-------------|-------------|
| Row | int | Line number (0-indexed) | >= 0 |
| Col | int | Column position (0-indexed) | >= 0 |

**Notes**:
- Identifier is not stored in Mark struct - it's the map key in MarkRegistry
- Position may become invalid if file is edited (handled at jump time)

### MarkRegistry (conceptual)

Collection of marks for a buffer, stored as `map[rune]Mark` in Buffer struct.

| Operation | Description |
|-----------|-------------|
| Set(id, row, col) | Create or overwrite mark with given identifier |
| Get(id) | Return mark if exists, nil otherwise |
| Delete(id) | Remove mark (optional, not required by spec) |
| Clear() | Remove all marks (optional, for buffer close) |

## State Extensions

### InputState (existing struct in input.go)

Add fields for pending mark operations:

| Field | Type | Description |
|-------|------|-------------|
| PendingMark | bool | True when waiting for mark identifier after 'm' |
| PendingJumpToMark | bool | True when waiting for mark identifier after '`' |

**Integration with existing methods**:
- `HasPending()` → include `PendingMark || PendingJumpToMark`
- `PendingString()` → return `"m_"` or `` "`_" `` as appropriate
- `Reset()` → clear both pending states

### Buffer (existing struct in buffer.go)

Add field for marks storage:

| Field | Type | Description |
|-------|------|-------------|
| Marks | map[rune]Mark | Per-buffer mark registry, keyed by identifier |

**Initialization**: Create empty map in `NewBuffer()` constructor.

## State Transitions

### Setting a Mark

```
Normal Mode → 'm' pressed → PendingMark=true → identifier pressed → Mark stored, PendingMark=false
                                             → Escape pressed → PendingMark=false (cancelled)
                                             → Invalid char → PendingMark=false (ignored)
```

### Jumping to a Mark

```
Normal Mode → '`' pressed → PendingJumpToMark=true → identifier pressed → Jump if exists, PendingJumpToMark=false
                                                   → Escape pressed → PendingJumpToMark=false (cancelled)
                                                   → Invalid char → PendingJumpToMark=false (ignored)
```

## Validation Rules

### Identifier Validation

```go
func isValidMarkIdentifier(r rune) bool {
    return unicode.IsLetter(r) || unicode.IsDigit(r)
}
```

### Position Clamping (at jump time)

```go
func clampPosition(row, col int, lines [][]rune) (int, int) {
    // Clamp row to valid range
    if row >= len(lines) {
        row = len(lines) - 1
    }
    if row < 0 {
        row = 0
    }

    // Clamp column to line length
    if col > len(lines[row]) {
        col = len(lines[row])
    }
    if col < 0 {
        col = 0
    }

    return row, col
}
```

## File Locations

| Entity/Change | File |
|---------------|------|
| Mark struct | `editor/marks.go` (new) |
| isValidMarkIdentifier | `editor/marks.go` (new) |
| Buffer.Marks field | `editor/buffer.go` |
| InputState pending fields | `editor/input.go` |
| Key handling | `editor/editor.go` |
