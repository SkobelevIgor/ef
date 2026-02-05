# Research: Search Mode Implementation

**Feature**: 001-search-mode
**Date**: 2026-02-04

## Research Questions

### 1. How to integrate Search Mode with existing modal architecture?

**Decision**: Extend InputState with search-specific fields rather than adding a new Mode constant.

**Rationale**:
- The existing codebase uses `InputState` for pending operations like goto line (`:`) and find character (`f`/`F`)
- Search mode behaves similarly - accumulates input, completes on Enter/Escape
- Adding `PendingSearch` flag follows established pattern
- Keeps the Mode enum clean (Normal, Insert, Visual) while search acts as a "sub-mode" of Normal

**Alternatives considered**:
- Add `ModeSearch` to Mode enum - rejected because search overlays existing modes rather than replacing them
- Create separate SearchManager class - rejected as over-engineering for this scope

### 2. How to handle search bar UI rendering?

**Decision**: Modify `screen.Render()` to conditionally draw search bar at top when `inputState.PendingSearch` is true.

**Rationale**:
- Current rendering flow: `Render()` → `renderPane()` for each buffer
- Search bar should appear at row 0, pushing content down by 1 row
- Follows pattern of modal UIs that overlay the editor
- Search bar needs its own styling (distinct from editor content)

**Implementation**:
- Check `inputState.PendingSearch` at start of `Render()`
- If true, render search row at y=0, then render panes starting at y=1
- Search row shows: query text + cursor + match count/feedback ("No matches")

### 3. How to implement case-insensitive search efficiently?

**Decision**: Use `strings.EqualFold()` for comparison or convert both to lowercase with `strings.ToLower()`.

**Rationale**:
- Go's `strings` package handles Unicode case folding correctly
- Performance is adequate for file sizes typical in a terminal editor
- No need for regex or complex matching - spec requires literal text matching

**Implementation**:
- Convert search query to lowercase once
- For each line, convert to lowercase and use `strings.Index()` to find matches
- Store original positions (character indices) for cursor positioning

### 4. How to track match positions and navigation?

**Decision**: Store matches as `[]SearchMatch` where `SearchMatch` contains `{Row, Col, Length}`.

**Rationale**:
- Need row/col for cursor positioning
- Need length for highlighting (query may be multi-byte)
- Pre-computing all matches enables instant `n`/`N` navigation
- Recalculate on query change (incremental search)

**Implementation**:
- `FindAllMatches(query string) []SearchMatch` scans entire buffer
- `currentMatchIndex` tracks which match cursor is on
- `n` increments index (wrapping at end), `N` decrements (wrapping at start)

### 5. How to integrate replacements with undo history?

**Decision**: Use `history.RecordDelete()` + `history.RecordInsert()` or direct `history.Push()` with `ChangeReplace` for each replacement.

**Rationale**:
- Spec requires: "Each replacement is added to editor's undo history as an individual entry"
- Existing `History.Push()` and `Change` types support this
- Each Enter in replace mode = one undoable operation

**Implementation**:
- On replace: snapshot old text, perform replacement, push Change with ChangeReplace type
- User can undo individual replacements with `u` in normal mode

### 6. How to parse replace syntax `replace::<search>::<replacement>`?

**Decision**: String prefix check and split on `::` delimiter.

**Rationale**:
- Simple, deterministic parsing
- Spec defines: malformed syntax (missing delimiters) treated as literal search
- No complex regex needed

**Implementation**:
```go
func ParseSearchQuery(input string) (query string, replacement string, isReplace bool) {
    if strings.HasPrefix(input, "replace::") {
        parts := strings.SplitN(input[9:], "::", 2)
        if len(parts) == 2 {
            return parts[0], parts[1], true
        }
    }
    return input, "", false
}
```

### 7. How to handle incremental search highlighting?

**Decision**: During query input, highlight first match after CursorZero in real-time.

**Rationale**:
- Spec: "Highlight first match in real-time as user types; full navigation enabled after pressing Enter"
- Provides immediate feedback that query is valid
- Similar to vim's `incsearch` option

**Implementation**:
- On each character typed: recalculate matches, find first after CursorZero, highlight it
- Cursor doesn't move until Enter is pressed
- `n`/`N` navigation disabled until Enter confirms the search

### 8. How to preserve search query across F4 invocations?

**Decision**: Store the last confirmed query in Editor struct and restore it when re-entering search mode.

**Rationale**:
- Change request: "User presses F4 again after confirmed search, don't start new search session, keep previous query active"
- Query should persist at Editor level (not InputState) since InputState.Search is nil when not searching
- Only confirmed queries should be preserved (unconfirmed = cancelled search)
- Restored search should be immediately navigable (Confirmed=true from start)

**Implementation**:
- Add `lastSearchQuery string` field to Editor struct
- In `exitSearchMode()`: if Confirmed, save query to `e.lastSearchQuery`
- In `enterSearchMode()`: if `e.lastSearchQuery != ""`, populate SearchState with it
- Set `Confirmed=true` when restoring to enable immediate n/N navigation
- User can still type to modify/clear the query

**Alternatives considered**:
- Store in InputState - rejected because InputState.Search is nil when not searching
- Store in Buffer - rejected because search is editor-wide, not buffer-specific
- Store in global - rejected to keep state encapsulated in Editor

## Summary

All technical questions resolved. The implementation will:

1. Extend `InputState` with search fields (pattern follows existing goto line)
2. Add `SearchState` struct in new `search.go` file
3. Modify `screen.Render()` to conditionally show search bar
4. Add search logic to find case-insensitive matches
5. Integrate with existing undo history for replacements
6. Use simple string parsing for replace syntax
7. Preserve last confirmed query in Editor for restoration on F4 re-entry
