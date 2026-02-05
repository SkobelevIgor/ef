# Feature Specification: Search Mode

**Feature Branch**: `001-search-mode`
**Created**: 2026-02-04
**Status**: Draft
**Input**: User description: "Search Mode for fe editor - allowing users to search for text occurrences and perform find-and-replace operations"

## Glossary

- **CursorZero**: The cursor position immediately before the editor entered Search Mode. Used as the reference point for finding the first occurrence.

## Clarifications

### Session 2026-02-04

- Q: How should "no matches found" feedback be communicated to the user? → A: Display "No matches" text inline in the search row itself
- Q: What happens to search highlights when exiting search mode? → A: Remove all highlights on exit; position cursor at end of the matched occurrence
- Q: Should matches be highlighted in real-time as user types? → A: Highlight first match in real-time as user types; full navigation enabled after pressing Enter
- Q: Can replace operations be undone? → A: Each replacement is added to editor's undo history individually
- Q: Where does cursor go if Escape is pressed before Enter (during incremental search)? → A: Return to CursorZero (cancelled search)
- Q: What happens if user presses F4 while search is already active and confirmed? → A: Return focus to the search bar with previous query preserved; user can now edit the query (typing modifies query instead of triggering n/N navigation)

### Session 2026-02-05

- Q: Where should cursor go after accepting a replacement in find-and-replace sub-mode? → A: Cursor moves to the next occurrence after the current cursor position, NOT back to CursorZero. This enables efficient sequential replacement workflow.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Basic Text Search (Priority: P1)

A user is editing a file and wants to quickly find all occurrences of a specific word or phrase. They press F4 to open the search bar, type their query, and navigate through matches using keyboard shortcuts.

**Why this priority**: This is the core functionality that enables users to locate text within their files. Without basic search, all other search features have no foundation.

**Independent Test**: Can be fully tested by opening a file, pressing F4, entering a search term, and verifying cursor navigation to matches. Delivers immediate value by letting users find text without manually scrolling.

**Acceptance Scenarios**:

1. **Given** user is in any editor mode with a file open, **When** user presses F4, **Then** a search input row appears at the very top of the editor
2. **Given** search row is open with a query entered, **When** user presses Enter, **Then** cursor moves to the first occurrence of the query after CursorZero and highlights search occurance.
3. **Given** search found a match and cursor is on it, **When** user presses `n`, **Then** cursor moves to the next occurrence and highlights found occurance (wrapping to start of file if at last match)
4. **Given** search found a match and cursor is on it, **When** user presses `N`, **Then** cursor moves to the previous occurrence and highlights found occurance (wrapping to end of file if at first match)
5. **Given** search row is open and matches exist, **When** user presses Escape, **Then** search row closes, highlights are removed, and cursor is positioned at the end of the current match
6. **Given** search row is open and no matches exist, **When** user presses Escape, **Then** search row closes and cursor returns to CursorZero
7. **Given** user is typing query with incremental match highlighted but has not pressed Enter, **When** user presses Escape, **Then** search row closes, highlights removed, and cursor returns to CursorZero (search cancelled)
8. **Given** user has confirmed a search (pressed Enter) and exited with Escape, **When** user presses F4 again, **Then** search bar reopens with the previous query preserved and ready for navigation
9. **Given** search bar is reopened with previous query, **When** user presses `n` or `N`, **Then** navigation continues from current cursor position using the preserved query
10. **Given** search is active and confirmed (user pressed Enter), **When** user presses F4 again, **Then** focus returns to search bar and user can modify the existing query

---

### User Story 2 - Case-Insensitive Search (Priority: P1)

A user wants to find text regardless of capitalization. When searching for "hello", the search should match "Hello", "HELLO", "hElLo", and any other case variation.

**Why this priority**: Case-insensitive search is essential for practical text searching. Most users expect search to be case-insensitive by default. This is bundled with P1 as it's a core search behavior.

**Independent Test**: Can be tested by searching for a lowercase term in a file containing mixed-case variations and verifying all matches are found.

**Acceptance Scenarios**:

1. **Given** file contains "Hello", "hello", and "HELLO", **When** user searches for "hello", **Then** all three occurrences are found as matches
2. **Given** file contains "CamelCase", **When** user searches for "camelcase", **Then** the "CamelCase" text is found as a match

---

### User Story 3 - Find and Replace (Priority: P2)

A user wants to replace specific text occurrences with new text. They use the special replace syntax to enter find-and-replace sub-mode, review each match, and decide whether to replace it or skip to the next occurrence.

**Why this priority**: Find-and-replace builds on basic search and provides significant productivity value for refactoring and bulk text changes. It depends on P1 being complete.

**Independent Test**: Can be tested by entering a replace pattern, confirming replacements, and verifying the text changes correctly while preserving the ability to skip unwanted matches.

**Acceptance Scenarios**:

1. **Given** search row is open, **When** user enters `replace::foo::bar` and presses Enter, **Then** editor enters find-and-replace sub-mode and highlights first occurrence of "foo" after CursorZero
2. **Given** find-and-replace is active with "foo" highlighted, **When** user presses Enter, **Then** "foo" is replaced with "bar" and cursor moves to next occurrence of "foo"
3. **Given** find-and-replace is active with "foo" highlighted, **When** user presses `n`, **Then** current occurrence is skipped (not replaced) and cursor moves to next occurrence of "foo"
4. **Given** find-and-replace is active with "foo" highlighted, **When** user presses `N`, **Then** current occurrence is skipped and cursor moves to previous occurrence of "foo"
5. **Given** find-and-replace is active and user is on the last occurrence, **When** user presses Enter to replace, **Then** text is replaced and editor indicates no more occurrences (or wraps to first occurrence if wrapping is desired)

---

### Edge Cases

- What happens when the search query is empty? (Search should not proceed; user stays in search row)
- What happens when no matches are found? (Search row displays "No matches" inline; cursor returns to CursorZero on Escape)
- What happens when the file is empty? (No matches found; cursor stays at position 0,0)
- How does search handle special characters in the query? (Characters should be treated literally, not as regex)
- What happens in find-and-replace when `<replace-with-row>` is empty? (Replace effectively deletes the matched text)
- What happens when find-and-replace syntax is malformed (e.g., `replace::foo`)? (Treat as regular search for the literal text "replace::foo")
- How does wrapping work when there's only one match? (Pressing `n` or `N` should stay on the same match)
- What happens if user presses F4 when no previous search exists? (Opens empty search bar for new query)
- What happens if user starts typing after reopening with preserved query? (User can edit the preserved query or clear and type new query)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST open a search input row at the top of the editor when user presses F4 from any mode
- **FR-002**: System MUST close the search input row and restore focus to the editor when user presses Escape
- **FR-003**: System MUST record the cursor position (CursorZero) when entering Search Mode
- **FR-004**: System MUST perform case-insensitive matching for all search operations
- **FR-005**: System MUST move cursor to the first matching occurrence after CursorZero when user presses Enter with a non-empty query
- **FR-006**: System MUST move cursor to the next occurrence when user presses `n` (lowercase), wrapping to start of file after last match
- **FR-007**: System MUST move cursor to the previous occurrence when user presses `N` (uppercase), wrapping to end of file before first match
- **FR-008**: System MUST position cursor at the end of the matched occurrence when exiting Search Mode with a match found
- **FR-009**: System MUST return cursor to CursorZero if no matches were found when exiting Search Mode
- **FR-010**: System MUST recognize the pattern `replace::<search>::<replacement>` as find-and-replace sub-mode
- **FR-011**: System MUST replace the current match with the replacement text when user presses Enter in find-and-replace sub-mode, then navigate to the next occurrence after the current cursor position (not CursorZero)
- **FR-012**: System MUST skip the current match without replacing when user presses `n` in find-and-replace sub-mode
- **FR-013**: System MUST navigate to the previous match without replacing when user presses `N` in find-and-replace sub-mode
- **FR-014**: System MUST treat search queries as literal text (not regular expressions)
- **FR-015**: System MUST handle empty replacement text by deleting the matched occurrence
- **FR-016**: System MUST treat malformed replace syntax (missing delimiters) as a literal search query
- **FR-017**: System MUST display "No matches" text inline in the search row when no occurrences are found
- **FR-018**: System MUST remove all search highlights when exiting Search Mode
- **FR-019**: System MUST highlight the first match after CursorZero in real-time as the user types the search query (incremental search)
- **FR-020**: System MUST enable `n`/`N` navigation only after user presses Enter to confirm the search query
- **FR-021**: System MUST add each replacement operation to the editor's undo history as an individual entry
- **FR-022**: System MUST return cursor to CursorZero when Escape is pressed before Enter (cancelled incremental search)
- **FR-023**: System MUST preserve the last confirmed search query when exiting Search Mode
- **FR-024**: System MUST restore the preserved search query when re-entering Search Mode via F4 (if a previous query exists)
- **FR-025**: System MUST treat a reopened search session as confirmed, enabling immediate `n`/`N` navigation without requiring Enter again
- **FR-026**: System MUST return focus to the search bar (allowing query editing) when user presses F4 while already in a confirmed search session
- **FR-027**: System MUST position the cursor in the search bar at the end of the query text when the search bar is active and accepting input (not confirmed)

### Key Entities

- **SearchState**: Represents the active search session including CursorZero, current query, current match index, list of match positions, whether in replace sub-mode, and preserved query from previous session
- **SearchMatch**: Represents a single occurrence of the search query in the file, including its position (line and column) and length
- **ReplaceOperation**: Represents a pending replacement including the search term, replacement text, and target match

## Assumptions

- The F4 key is available and not conflicting with other editor functions in the current mode configuration
- Search operates on the currently active/focused file only (multi-file search is out of scope)
- Incremental search shows first match in real-time; `n`/`N` navigation requires Enter to confirm query first
- Visual highlighting of matches is expected but specific styling is an implementation detail
- The search row input supports standard text editing (backspace, arrow keys, etc.)
- When reopening search with preserved query, the user can modify the query (typing clears preserved query and starts fresh input)

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can find any text occurrence in a file within 3 keystrokes after pressing F4 (F4, type query, Enter)
- **SC-002**: Users can navigate between all occurrences using only `n` and `N` keys without re-entering the search query
- **SC-003**: Users can exit search mode and return to editing with a single Escape keypress
- **SC-004**: 100% of search results match the query regardless of case differences
- **SC-005**: Users can complete a single find-and-replace operation within 5 keystrokes (F4, enter pattern, Enter, Enter)
- **SC-006**: Users can selectively replace some occurrences while skipping others in a single search session
