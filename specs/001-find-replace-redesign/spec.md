# Feature Specification: Find and Replace Redesign

**Feature Branch**: `001-find-replace-redesign`
**Created**: 2026-03-02
**Status**: Draft
**Input**: User description: "Change requests for Find and Find-and-Replace features: new keybindings (F3/F4), two-row replace widget, and multiline bar support"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Search with F4 Key (Priority: P1)

A user working in the editor wants to search for text. They press F4 to open the Search bar at the top of the screen. They type their search query and see matches highlighted in real time. Pressing F4 again toggles focus between the Search bar and the code editor. Pressing Escape closes the Search bar entirely.

**Why this priority**: This is the most fundamental search interaction and the entry point for all search functionality. Without this, no other search features work.

**Independent Test**: Can be fully tested by opening a file, pressing F4, typing a query, verifying matches highlight, pressing F4 to return to editor, and pressing F4 again to return to the Search bar.

**Acceptance Scenarios**:

1. **Given** the editor is in Normal mode with no search active, **When** the user presses F4, **Then** a Search bar appears at the top of the screen with cursor in it, ready for input.
2. **Given** the Search bar is active and focused, **When** the user presses F4, **Then** focus returns to the code editor with the search bar still visible and matches highlighted.
3. **Given** the Search bar is visible but focus is on the editor, **When** the user presses F4, **Then** focus returns to the Search bar for editing the query.
4. **Given** the Search bar is active (focused or unfocused), **When** the user presses Escape, **Then** the Search bar closes and the editor returns to its previous state.
5. **Given** no search is active, **When** the user presses F4, and a previous search query exists, **Then** the Search bar opens with the previous query pre-filled and confirmed for n/N navigation.

---

### User Story 2 - Find and Replace with F3 Key (Priority: P1)

A user wants to find and replace text in their file. They press F3 to open the Find-and-Replace widget, which shows two rows: a Find row and a Replace row, each with a distinct color. The user types the search term in the Find row, then presses F3 to move focus to the Replace row and types the replacement text. Pressing F3 again returns focus to the code editor. The cycle repeats: F3 toggles through Find bar, Replace bar, and editor.

**Why this priority**: This is the core replace interaction and the second most critical search feature. The two-row widget and F3 cycling are the primary UX changes requested.

**Independent Test**: Can be fully tested by opening a file, pressing F3, typing in Find row, pressing F3 to move to Replace row, typing replacement, pressing F3 to return to editor, and triggering a replace action.

**Acceptance Scenarios**:

1. **Given** the editor is in Normal mode with no search active, **When** the user presses F3, **Then** a two-row Find-and-Replace widget appears with the Find row focused.
2. **Given** the Find-and-Replace widget is open with Find row focused, **When** the user presses F3, **Then** focus moves to the Replace row.
3. **Given** the Find-and-Replace widget is open with Replace row focused, **When** the user presses F3, **Then** focus moves to the code editor, keeping the widget visible.
4. **Given** the Find-and-Replace widget is open with focus on the editor, **When** the user presses F3, **Then** focus returns to the Find row.
5. **Given** the Find-and-Replace widget is active, **When** the user types in the Find row, **Then** matches are highlighted in real time (same as Search mode).
6. **Given** the Find-and-Replace widget is active, **When** the user presses Escape from any focus position, **Then** the entire widget closes and the editor returns to its previous state.
7. **Given** the Find row and Replace row both have text, **When** the user presses Enter (from either the Find row or the Replace row), **Then** the current match is replaced with the Replace row text, and the cursor advances to the next match.

---

### User Story 3 - Multiline Bar Support (Priority: P2)

A user types a long search query that exceeds the width of the Search bar. Instead of the text disappearing off the right edge, the bar automatically expands to a second row to accommodate the overflow. This works for both the Find row and the Replace row in Find-and-Replace mode.

**Why this priority**: This improves usability for long queries but is not essential for basic search/replace functionality.

**Independent Test**: Can be tested by opening a file, pressing F4, typing a very long query that exceeds the terminal width, and verifying the bar expands to show the full text.

**Acceptance Scenarios**:

1. **Given** the Search bar is active, **When** the query text length exceeds the available row width, **Then** the bar expands to a second row and the content wraps.
2. **Given** the Find-and-Replace widget is active, **When** the Find row text exceeds the row width, **Then** only the Find row expands to a second row; the Replace row stays single-row.
3. **Given** the Find-and-Replace widget is active, **When** the Replace row text exceeds the row width, **Then** only the Replace row expands; the Find row stays single-row.
4. **Given** a bar has expanded to two rows, **When** the user deletes text to fit within one row, **Then** the bar contracts back to a single row.
5. **Given** multiline expansion is active, **When** the bar expands, **Then** the editor content area shifts down accordingly so no content is hidden behind the bar.

---

### User Story 4 - Mode Switching Between Search and Find-and-Replace (Priority: P2)

A user has the Search bar open (via F4) and realizes they need to replace text. They press F3 to switch from Search mode to Find-and-Replace mode, and the widget expands to show a second Replace row, preserving the existing search query. Conversely, pressing F4 from Find-and-Replace mode switches back to Search-only mode. F3 and F4 maintain dual independent sessions — each session preserves its own query, replace text, and match state. Switching between them restores the previously active session.

**Why this priority**: This enhances workflow fluidity but depends on both Search and Find-and-Replace modes being implemented first.

**Independent Test**: Can be tested by pressing F4 to open Search, typing a query, pressing F3 to switch to Find-and-Replace, verifying the query is preserved and a Replace row appears.

**Acceptance Scenarios**:

1. **Given** the Search bar is open via F4 with a query typed, **When** the user presses F3, **Then** the widget switches to Find-and-Replace mode, showing the Replace row, with the existing search query preserved in the Find row.
2. **Given** the Find-and-Replace widget is open via F3, **When** the user presses F4, **Then** the widget switches to Search-only mode, hiding the Replace row.

---

### Edge Cases

- What happens when the user presses F3 in Visual mode or Insert mode? (Assumed: F3 and F4 are global keys that work from any mode, same as current F4 behavior)
- What happens when a query wraps to exactly the width boundary — does it stay on one row or expand? (Assumed: expands only when text exceeds the row width, not at exactly the boundary)
- What happens if the terminal is resized while a multiline bar is visible? (Assumed: the bar re-wraps to fit the new width)
- What happens when both Find and Replace rows are multiline? (Assumed: both expand independently, editor content shifts down by total bar height)
- What happens when the user presses Enter in the Find row during Find-and-Replace mode? (Answer: Enter replaces the current match with the Replace row text and advances to the next match)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST open a Search bar when the user presses F4, with the cursor in the bar ready for text input.
- **FR-002**: System MUST toggle focus between the Search bar and the code editor when F4 is pressed repeatedly (Search bar → Editor → Search bar → ...).
- **FR-003**: System MUST open a two-row Find-and-Replace widget when the user presses F3, with the Find row focused.
- **FR-004**: System MUST cycle focus through Find row → Replace row → Editor → Find row when F3 is pressed repeatedly in Find-and-Replace mode.
- **FR-005**: System MUST render the Find row with a blue background and the Replace row with a green background, each with white foreground text.
- **FR-006**: System MUST close the search/replace widget and return to the previous editor state when the user presses Escape from any focus position.
- **FR-007**: System MUST remove the `replace::search::replacement` text-based syntax for entering replace mode.
- **FR-008**: System MUST expand a bar row to a second (or more) row when the text length exceeds the available width, wrapping the text character-by-character.
- **FR-009**: System MUST contract a bar row back when the text fits within fewer rows after deletion.
- **FR-010**: System MUST shift the editor content area down by the total height of the search/replace bars so content is never hidden behind the bars.
- **FR-011**: System MUST allow switching from Search mode (F4) to Find-and-Replace mode (F3) and vice versa, preserving the current search query in the Find row.
- **FR-012**: System MUST highlight search matches in real time as the user types in the Find row, regardless of whether the mode is Search or Find-and-Replace.
- **FR-013**: System MUST support n/N navigation for moving between matches after search is confirmed, in both Search and Find-and-Replace modes.
- **FR-014**: System MUST preserve the last search query across mode switches and re-entries (pressing F4 or F3 after Escape should restore the previous query).

### Key Entities

- **SearchState**: Represents the current search/replace session, including the search query, replace text, match list, current match index, active mode (search-only vs find-and-replace), and focus position (find bar, replace bar, or editor).
- **Bar Row**: A single visual row within the Search or Replace bar, which can dynamically expand to multiple terminal rows when text overflows.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can initiate a text search with a single keypress (F4) and see matches highlighted within the normal editor response time.
- **SC-002**: Users can initiate a find-and-replace operation with a single keypress (F3) and see both Find and Replace input rows displayed clearly with distinct colors.
- **SC-003**: Users can toggle between input rows and the editor using the same key (F3) without needing to learn additional keybindings — the full cycle (Find → Replace → Editor) completes in exactly 3 presses of F3.
- **SC-004**: Users can type search queries of any length without text disappearing — long queries wrap to additional rows automatically.
- **SC-005**: Editor test coverage for the editor package remains at or above 80% after all changes.
- **SC-006**: The old `replace::` text-based syntax is fully removed and no longer recognized.

## Assumptions

- F3 and F4 are global keys that work from Normal, Insert, and Visual modes (consistent with current F4 behavior).
- Character-by-character wrapping is used for multiline expansion (not word wrapping), since search queries are often arbitrary strings.
- The Find row in Find-and-Replace mode behaves identically to the Search bar in Search-only mode (real-time highlighting, n/N navigation after confirm).
- When switching from Find-and-Replace to Search-only mode, the Replace row text is preserved internally but hidden from view.
- The existing match highlighting, n/N navigation, and undo/redo for replacements continue to work as before.
- Backspace in a bar row works the same as currently — deletes the last character and updates matches.
