# Data Model: Search Mode

**Feature**: 001-search-mode
**Date**: 2026-02-04

## Entities

### SearchMatch

Represents a single occurrence of the search query in the buffer.

| Field | Type | Description |
|-------|------|-------------|
| Row | int | Line number (0-indexed) where match starts |
| Col | int | Column position (0-indexed) where match starts |
| Length | int | Number of characters in the match |

**Validation Rules**:
- Row must be >= 0 and < len(buffer.Lines)
- Col must be >= 0 and <= len(buffer.Lines[Row])
- Length must be > 0

### SearchState

Represents the active search session state.

| Field | Type | Description |
|-------|------|-------------|
| Active | bool | Whether search mode is currently active |
| Query | string | Current search query text |
| Confirmed | bool | Whether Enter has been pressed to confirm search |
| CursorZeroRow | int | Row position before entering search mode |
| CursorZeroCol | int | Column position before entering search mode |
| Matches | []SearchMatch | All matches found for current query |
| CurrentIndex | int | Index of currently highlighted match (-1 if none) |
| IsReplaceMode | bool | Whether in find-and-replace sub-mode |
| ReplaceText | string | Replacement text (when IsReplaceMode is true) |
| NoMatches | bool | Flag indicating query returned no results |

**State Transitions**:

```
                    ┌──────────────────────────────────────┐
                    │                                      │
    F4              ▼                                      │
    (no prev) ►  [Active=true, Confirmed=false]            │
                (Incremental search, typing query)         │
                    │                                      │
                    │ Enter                                │ Escape (before Enter)
                    ▼                                      │
            [Active=true, Confirmed=true]                  │
            (Navigation enabled: n/N)                      │
                    │                                      │
                    │ Escape (saves query)                 │
                    ▼                                      ▼
            [Active=false] ◄───────────────────────────────┘
            (Cursor at match end, or CursorZero)
                    │
                    │ F4 (with prev query)
                    ▼
            [Active=true, Confirmed=true, Query=prev]
            (Immediate n/N navigation available)
```

**Replace Mode Sub-States**:

```
            Query: "replace::foo::bar"
                    │
                    │ Enter
                    ▼
            [IsReplaceMode=true]
                    │
        ┌───────────┼───────────┐
        │           │           │
        │ n         │ Enter     │ N
        ▼           ▼           ▼
    [Skip to    [Replace,    [Skip to
     next]       move next]   previous]
```

### InputState Extensions

Fields to add to existing `InputState` struct:

| Field | Type | Description |
|-------|------|-------------|
| Search | *SearchState | Pointer to active search state (nil when not searching) |

### Editor Extensions (for query persistence)

Fields to add to existing `Editor` struct:

| Field | Type | Description |
|-------|------|-------------|
| lastSearchQuery | string | Last confirmed search query for restoration on F4 re-entry |

## Relationships

```
Editor
  ├── lastSearchQuery (persists across search sessions)
  └── InputState
        └── *SearchState (when searching)
              └── []SearchMatch (computed from query)

Buffer
  └── Lines [][]rune (searched content)
        └── Matches map to positions in Lines
```

## Key Operations

### FindAllMatches(query string) []SearchMatch

Scans entire buffer for case-insensitive occurrences of query.

**Algorithm**:
1. Convert query to lowercase
2. For each line in buffer.Lines:
   - Convert line to lowercase string
   - Find all occurrences using strings.Index in a loop
   - For each occurrence, create SearchMatch with original row/col
3. Return sorted list (by row, then col)

### NavigateToMatch(index int)

Moves cursor to the match at given index.

**Algorithm**:
1. Validate index within Matches bounds
2. Set buffer.CursorRow = Matches[index].Row
3. Set buffer.CursorCol = Matches[index].Col
4. Update SearchState.CurrentIndex = index

### ReplaceCurrentMatch()

Replaces the currently highlighted match with ReplaceText.

**Algorithm**:
1. Get current match from Matches[CurrentIndex]
2. Snapshot old text for undo
3. Delete match.Length characters starting at (match.Row, match.Col)
4. Insert ReplaceText at same position
5. Push Change to history
6. Recalculate matches (positions shifted by replacement)
7. Navigate to next match (or indicate completion)

### ExitSearchMode(foundMatch bool)

Cleans up search state and positions cursor.

**Algorithm**:
1. If Confirmed:
   - Save query to Editor.lastSearchQuery (for restoration on F4 re-entry)
2. If foundMatch && Confirmed:
   - Position cursor at end of current match
3. Else:
   - Restore cursor to CursorZero
4. Clear all highlights
5. Set SearchState.Active = false
6. Reset InputState.Search = nil

### EnterSearchMode()

Initializes search mode, restoring previous query if available.

**Algorithm**:
1. Create new SearchState with current cursor as CursorZero
2. If Editor.lastSearchQuery != "":
   - Set SearchState.Query = Editor.lastSearchQuery
   - Set SearchState.Confirmed = true (enable immediate n/N navigation)
   - Calculate matches for restored query
   - Position to first match after current cursor
3. Else:
   - Start with empty query, Confirmed = false
