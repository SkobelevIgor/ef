# Research: Marks Navigation

**Feature**: 003-marks-navigation
**Date**: 2026-02-05

## Research Questions

### 1. Existing Pattern for Pending Input State

**Decision**: Follow the `PendingFindForward`/`PendingFindBackward` pattern in `editor/input.go`

**Rationale**: The codebase already has a well-established pattern for multi-key commands:
- `InputState` struct holds pending state (e.g., `PendingFindForward`, `PendingGotoLine`)
- `PendingString()` method returns visual feedback for status display
- `HasPending()` checks if any pending state is active
- `Reset()` clears all pending state

**Alternatives considered**:
- New separate state manager: Rejected - adds unnecessary complexity, breaks consistency
- Inline state in Editor: Rejected - InputState is the established location

### 2. Mark Storage Location

**Decision**: Store marks in `Buffer` struct as `map[rune]Mark`

**Rationale**:
- Marks are per-buffer (FR-008), so Buffer is the natural location
- Map provides O(1) lookup by identifier
- Following pattern of other per-buffer state (CursorRow, CursorCol, etc.)

**Alternatives considered**:
- Store in Editor: Rejected - marks are buffer-specific, not editor-global
- Store in InputState: Rejected - InputState is for transient input state, not persistent marks
- Slice with linear search: Rejected - map is more efficient for lookup

### 3. Valid Identifier Characters

**Decision**: Accept 0-9, a-z, A-Z (62 total identifiers)

**Rationale**:
- Matches vim behavior for local marks
- `unicode.IsLetter()` and `unicode.IsDigit()` provide clean validation
- Covers all practical use cases

**Alternatives considered**:
- Only lowercase letters: Rejected - limits flexibility, doesn't match vim
- Include special characters: Rejected - conflicts with other vim commands

### 4. Invalid Position Handling

**Decision**: Clamp to valid range (closest valid position)

**Rationale**:
- Row beyond file length → move to last line
- Column beyond line length → move to end of line
- This matches vim behavior and provides predictable UX

**Alternatives considered**:
- Delete invalid marks: Rejected - user may want to know where mark was
- Show error message: Rejected - adds noise for a recoverable situation
- Do nothing: Rejected - confusing when mark exists but doesn't work

### 5. Visual Feedback for Pending State

**Decision**: Show `m_` or `` `_ `` in status area using existing `PendingString()` mechanism

**Rationale**:
- Matches existing pattern for `f_`, `F_`, `:` prefix
- User knows system is waiting for identifier input
- No new UI elements needed

**Alternatives considered**:
- Cursor shape change: Rejected - terminal may not support
- Mode line indicator: Rejected - already have pending string mechanism

## Implementation Approach

1. **New file `editor/marks.go`**: Contains `Mark` struct and helper functions
2. **Modify `editor/buffer.go`**: Add `Marks map[rune]Mark` field, initialize in constructor
3. **Modify `editor/input.go`**: Add `PendingMark` and `PendingJumpToMark` bool fields
4. **Modify `editor/editor.go`**: Handle `m` and `` ` `` keys in `handleNormalMode()`
5. **Modify `editor/screen.go`**: Update `PendingString()` to include mark states (if not in input.go)

## Dependencies

None - uses only standard library and existing tcell dependency.
