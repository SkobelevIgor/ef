# Feature Specification: Autocomplete

**Feature Branch**: `002-autocomplete`
**Created**: 2026-02-05
**Status**: Draft
**Input**: User description: "Autocomplete based on current file content with dropdown suggestions, keyboard navigation, and shallow matching"

## Glossary

- **Shallow Matching**: A matching strategy where typed characters can appear anywhere within candidate words, not necessarily consecutively or at the beginning (e.g., typing "fn" matches "function", "final", "define")
- **Trigger Threshold**: The minimum number of characters (2) required before autocomplete suggestions appear
- **Suggestion Dropdown**: A floating UI element displaying candidate completions near the cursor

## Clarifications

### Session 2026-02-05

- Q: How should suggestions be ordered when multiple matches exist? → A: Relevance-based ordering: prefix matches first, then shorter words, with alphabetical as tiebreaker
- Q: Which keys should accept the selected suggestion? → A: Both Tab and Enter accept the highlighted suggestion
- Q: Which keys should navigate the dropdown? → A: Ctrl+n/Ctrl+p AND Up/Down arrow keys for navigation
- Q: What colors should the dropdown use? → A: White text on black background for selected item; black text on purple background for unselected items
- Q: How wide should the dropdown be? → A: Dropdown should have extra padding (spaces) on left and right sides for better readability

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Basic Word Completion (Priority: P1)

A user is typing code or text and wants to quickly complete a word they've already used elsewhere in the file. After typing at least two characters, a dropdown appears with matching suggestions. The user presses Tab to accept the top suggestion and continue working.

**Why this priority**: This is the core autocomplete workflow that provides immediate productivity value. Without basic completion, other features have no foundation.

**Independent Test**: Can be tested by opening a file with existing words, typing 2+ characters that match existing words, and verifying dropdown appears with Tab acceptance working.

**Acceptance Scenarios**:

1. **Given** user is typing in Insert mode with a file containing the word "function", **When** user types "fu", **Then** a dropdown appears showing "function" as a suggestion
2. **Given** autocomplete dropdown is visible with suggestions, **When** user presses Tab or Enter, **Then** the currently highlighted suggestion replaces the typed prefix and dropdown closes
3. **Given** autocomplete dropdown is visible, **When** user continues typing more characters, **Then** suggestions update to reflect the new, longer prefix
4. **Given** autocomplete dropdown is visible, **When** user types a character that results in no matches, **Then** dropdown disappears
5. **Given** user has typed only 1 character, **When** that character exists in words in the file, **Then** no dropdown appears (minimum 2 characters required)

---

### User Story 2 - Keyboard Navigation (Priority: P1)

A user sees multiple suggestions in the dropdown and wants to select one that isn't at the top. They use Ctrl+n or Down arrow to move down through suggestions and Ctrl+p or Up arrow to move up, then Tab or Enter to accept their selection.

**Why this priority**: Navigation is essential for practical use when multiple matches exist. Bundled with P1 as it's required for the feature to be useful.

**Independent Test**: Can be tested by triggering dropdown with multiple suggestions and verifying Ctrl+n/Ctrl+p and Up/Down arrows cycle through them correctly.

**Acceptance Scenarios**:

1. **Given** dropdown is visible with multiple suggestions and first item is highlighted, **When** user presses Ctrl+n or Down arrow, **Then** highlight moves to the next suggestion
2. **Given** dropdown is visible with last suggestion highlighted, **When** user presses Ctrl+n or Down arrow, **Then** highlight wraps to the first suggestion
3. **Given** dropdown is visible with first suggestion highlighted, **When** user presses Ctrl+p or Up arrow, **Then** highlight wraps to the last suggestion
4. **Given** dropdown is visible with any suggestion highlighted, **When** user presses Tab or Enter, **Then** the highlighted suggestion is inserted

---

### User Story 3 - Shallow Matching (Priority: P2)

A user wants to find words even when the typed characters don't appear consecutively or at the start of the word. For example, typing "fn" should match "function" (f...n), "final" (f...n...al), and "define" (d...f...n...e).

**Why this priority**: Shallow matching significantly improves the usefulness of autocomplete by finding relevant matches that prefix-only matching would miss. Depends on P1 being complete.

**Independent Test**: Can be tested by typing non-consecutive characters and verifying that words containing those characters in order are suggested.

**Acceptance Scenarios**:

1. **Given** file contains the word "function", **When** user types "fn", **Then** "function" appears in suggestions
2. **Given** file contains "define", "final", and "function", **When** user types "fn", **Then** all three words appear in suggestions
3. **Given** file contains "hello", **When** user types "fn", **Then** "hello" does NOT appear in suggestions (no 'f' or 'n' in order)

---

### User Story 4 - Dropdown Positioning (Priority: P2)

A user is typing near the bottom of the visible screen area. The dropdown should appear above the cursor instead of below to remain visible. Similarly, if typing at the very end of the file with no room below, the dropdown adjusts its position.

**Why this priority**: Proper positioning ensures the dropdown is always usable regardless of cursor location. Enhances UX but not blocking for core functionality.

**Independent Test**: Can be tested by positioning cursor at various screen locations and verifying dropdown remains fully visible.

**Acceptance Scenarios**:

1. **Given** cursor is in the upper portion of the screen with space below, **When** dropdown appears, **Then** it displays below the cursor
2. **Given** cursor is near the bottom of the screen with insufficient space below, **When** dropdown appears, **Then** it displays above the cursor
3. **Given** cursor is at the end of the file with no content below, **When** dropdown appears, **Then** it displays in a visible position (above cursor or at top of screen)

---

### User Story 5 - Dismiss Autocomplete (Priority: P2)

A user sees suggestions but decides they don't want any of them. They press Escape to dismiss the dropdown and continue typing normally.

**Why this priority**: Users need a way to dismiss unwanted suggestions without disrupting their workflow.

**Independent Test**: Can be tested by triggering dropdown and pressing Escape, verifying it closes without affecting the typed text.

**Acceptance Scenarios**:

1. **Given** autocomplete dropdown is visible, **When** user presses Escape, **Then** dropdown closes and typed text remains unchanged
2. **Given** autocomplete dropdown is visible, **When** user moves cursor with Left or Right arrow keys, **Then** dropdown closes (Up/Down navigate within dropdown)
3. **Given** autocomplete dropdown is visible, **When** user presses any key that isn't navigation or acceptance, **Then** that key is processed normally and suggestions update accordingly

---

### Edge Cases

- What happens when the file is empty? (No suggestions possible; dropdown never appears)
- What happens when the typed prefix matches no words? (Dropdown disappears or doesn't appear)
- What happens when there's only one matching word? (Show dropdown with single item; Tab accepts it)
- How are duplicate words handled? (Each unique word appears only once in suggestions)
- What happens if the word being typed is itself a match? (Exclude the current word-in-progress from suggestions)
- How does autocomplete handle words with special characters? (Words are tokenized by whitespace and common delimiters; special characters within words are included in the match)
- What is the maximum number of suggestions shown? (Reasonable limit to avoid overwhelming the user; assume 10 items max)
- What happens when user is in modes other than Insert mode? (Autocomplete only activates in Insert mode)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST display autocomplete suggestions only when user is in Insert mode
- **FR-002**: System MUST trigger autocomplete after user types at least 2 characters
- **FR-003**: System MUST perform case-insensitive matching when finding suggestions
- **FR-004**: System MUST use shallow matching where typed characters can match non-consecutively within words (e.g., "fn" matches "function")
- **FR-005**: System MUST display suggestions in a dropdown list near the current cursor position
- **FR-006**: System MUST highlight the first suggestion by default when dropdown appears
- **FR-007**: System MUST allow navigation through suggestions using Ctrl+n or Down arrow (next) and Ctrl+p or Up arrow (previous)
- **FR-008**: System MUST wrap navigation when reaching the end or beginning of the suggestion list
- **FR-009**: System MUST insert the highlighted suggestion when user presses Tab or Enter
- **FR-010**: System MUST replace the typed prefix with the complete word when accepting a suggestion
- **FR-011**: System MUST close the dropdown after a suggestion is accepted
- **FR-012**: System MUST close the dropdown when user presses Escape
- **FR-013**: System MUST update suggestions in real-time as user types additional characters
- **FR-014**: System MUST hide dropdown when no suggestions match the current prefix
- **FR-015**: System MUST position dropdown above the cursor when insufficient space exists below
- **FR-016**: System MUST extract candidate words from the current file's content
- **FR-017**: System MUST display each unique word only once in suggestions (no duplicates)
- **FR-018**: System MUST exclude the current word-in-progress from suggestions
- **FR-019**: System MUST limit the number of displayed suggestions to a maximum of 10 items
- **FR-020**: System MUST close dropdown when cursor moves horizontally (Left/Right arrows) away from the insertion point
- **FR-021**: System MUST order suggestions by relevance: prefix matches first, then by word length (shorter first), with alphabetical sorting as tiebreaker
- **FR-022**: System MUST display selected suggestion with white text on black background
- **FR-023**: System MUST display unselected suggestions with black text on purple background
- **FR-024**: System MUST add horizontal padding (extra spaces) on left and right sides of dropdown for better readability

### Key Entities

- **SuggestionState**: Represents the active autocomplete session including the typed prefix, list of matching suggestions, currently highlighted index, and dropdown position
- **Suggestion**: Represents a single candidate word including the word text and its match score/relevance
- **WordIndex**: Represents the collection of unique words extracted from the current file, used for efficient matching

## Assumptions

- Autocomplete operates on the currently active/focused file only
- Words are tokenized by whitespace and common programming delimiters (parentheses, brackets, operators, etc.)
- The dropdown styling and visual appearance are implementation details
- Suggestions are ordered by relevance: prefix matches rank highest, then shorter words, with alphabetical sorting as tiebreaker
- Performance should remain responsive even for large files (thousands of words)
- Autocomplete does not persist state between sessions; word index is rebuilt when file is opened or modified

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can complete words with 2-3 keystrokes (type 2 chars + Tab) instead of typing full words
- **SC-002**: Suggestions appear within 100ms of typing the trigger threshold (perceived as instant)
- **SC-003**: Users can navigate and select any visible suggestion within 5 keystrokes (2 chars + up to 3 navigations)
- **SC-004**: 90% of commonly repeated words in a file are successfully suggested when user types first 2 characters
- **SC-005**: Dropdown never obscures the text being edited (always positions to remain visible)
- **SC-006**: Users can dismiss unwanted suggestions with a single Escape keypress
