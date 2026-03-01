# Feature Specification: Code Simplification

**Feature Branch**: `009-code-simplification`
**Created**: 2026-03-01
**Status**: Draft
**Input**: Consolidate duplicated logic, extract shared utilities, and reduce code complexity across the editor codebase without changing any user-facing behavior.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Consolidate Duplicate Utilities (Priority: P1)

As a developer maintaining the editor, I want duplicated utility logic (visual column calculations, range normalization, cursor clamping, whitespace checks) consolidated into single implementations so that bug fixes only need to happen in one place.

**Why this priority**: Duplicated logic is the highest maintenance risk — fixing a bug in one copy but not the other leads to inconsistent behavior. This is the quickest win with highest reliability impact.

**Independent Test**: After consolidation, all existing editor operations (cursor movement, selection, rendering, tab handling) must behave identically to before. Run the e2e test suite and verify no regressions.

**Acceptance Scenarios**:

1. **Given** the editor renders a file with tabs, **When** the user moves the cursor across tab characters, **Then** visual column positions are calculated identically to the current behavior.
2. **Given** a user selects text in visual mode from bottom-to-top, **When** the selection range is computed, **Then** start/end are normalized correctly (same as current behavior).
3. **Given** a user jumps to a mark near the end of a file, **When** the cursor is placed, **Then** the cursor is clamped within valid buffer bounds (same as current behavior).

---

### User Story 2 - Extract Indentation Module (Priority: P2)

As a developer maintaining the editor, I want all indentation logic (reindentation, smart indent for new lines, paste reindentation) extracted into a cohesive module so that indentation behavior is easier to understand, modify, and test.

**Why this priority**: Indentation logic is the most complex scattered code (~200+ lines across 2 files). Centralizing it improves readability and makes future indentation improvements easier.

**Independent Test**: After extraction, all indentation operations (auto-indent on Enter, reindent on paste, block reindentation with `=`) must produce identical output to the current implementation. Run e2e tests covering indentation scenarios.

**Acceptance Scenarios**:

1. **Given** the user presses Enter after a line ending with `{`, **When** a new line is created, **Then** the new line is indented one level deeper (same as current behavior).
2. **Given** the user pastes a multi-line block, **When** the pasted content is reindented, **Then** indentation matches the surrounding context (same as current behavior).
3. **Given** the user selects lines and presses `=`, **When** ReindentLines executes, **Then** all lines are reindented correctly for the detected language (same as current behavior).

---

### User Story 3 - Reduce Repetitive Patterns (Priority: P3)

As a developer maintaining the editor, I want repetitive patterns (inputState.Reset calls, paste sub-operations, search navigation) simplified so that the codebase is smaller and easier to read.

**Why this priority**: These are lower-risk improvements that reduce code volume and improve readability but don't address correctness risks like duplication does.

**Independent Test**: After simplification, all normal mode commands, paste operations, and search navigation must behave identically. Run full e2e test suite.

**Acceptance Scenarios**:

1. **Given** the user executes any normal mode command (h, j, k, l, dd, etc.), **When** the command completes, **Then** the input state is reset (same as current behavior).
2. **Given** the user pastes content with `p` or `P`, **When** the paste executes, **Then** the cursor ends at the correct position with correct content (same as current behavior).
3. **Given** the user searches with `/` and navigates with `n`/`N`, **When** matches are found, **Then** the cursor jumps to the correct match position (same as current behavior).

---

### User Story 4 - Clean Up Incomplete Features (Priority: P4)

As a developer maintaining the editor, I want incomplete or dead code (such as partially implemented key mapping timeout) either completed or removed so the codebase contains only functioning code.

**Why this priority**: Dead/incomplete code creates confusion for maintainers. Lower priority because it doesn't cause bugs, only cognitive overhead.

**Independent Test**: After cleanup, all key mapping functionality that currently works must continue to work. Any removed code must not have been reachable or functional.

**Acceptance Scenarios**:

1. **Given** key mappings are configured, **When** the user triggers a mapped key sequence, **Then** the mapping executes correctly (same as current behavior).
2. **Given** incomplete timeout code existed, **When** it is removed or completed, **Then** no existing functionality is broken.

---

### Edge Cases

- What happens when consolidating visual column calculations if buffer.go and screen.go callers pass different parameter patterns? Callers must be updated to use the unified interface.
- What happens if extracting indentation logic introduces import cycles between packages? All code lives in the `editor` package, so this is not a risk.
- What happens if removing inputState.Reset() from individual commands causes a command to not reset when it should? Use an explicit opt-out list for commands that should NOT reset, rather than opt-in.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: All refactored code MUST produce identical behavior to the current implementation for every user-facing operation.
- **FR-002**: Visual column calculation MUST be consolidated into a single implementation used by both buffer operations and screen rendering.
- **FR-003**: Range normalization (ensuring start <= end) MUST be extracted into a single utility function used by GetSelection, GetRange, and DeleteRange.
- **FR-004**: Cursor position clamping MUST be consolidated into a single utility function used everywhere cursor bounds are checked.
- **FR-005**: Whitespace helper functions MUST share a common `isWhitespace` predicate instead of inline `' ' || '\t'` checks.
- **FR-006**: Indentation logic (ReindentLines, smartIndentForNewLine, reindentPastedRange, reindentInsertSession) MUST be grouped into a cohesive module or file.
- **FR-007**: The performPaste function MUST be broken into smaller, focused sub-functions.
- **FR-008**: The inputState.Reset() pattern MUST be simplified to reduce repetition across normal mode command handlers.
- **FR-009**: Search navigation (FindFirstMatchAfterCursor + cursor positioning) MUST be consolidated into a single helper.
- **FR-010**: Incomplete key mapping timeout code MUST be completed to full functionality.
- **FR-011**: The codebase MUST continue to compile and pass all existing tests after every individual change.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Total lines of duplicated logic reduced by at least 40% (measured by counting duplicate implementations before and after).
- **SC-002**: All existing end-to-end tests pass without modification after refactoring.
- **SC-003**: No new files are created beyond what is necessary for module extraction (maximum 2 new files: one for shared utilities, one for indentation if extracted to its own file).
- **SC-004**: The editor builds successfully and all current functionality works identically in manual terminal testing.

## Assumptions

- All code lives in the `editor` package, so there is no risk of import cycles from reorganization.
- The e2e test suite provides sufficient coverage to detect behavioral regressions.
- "Identical behavior" means the same output for the same input — internal implementation details may change freely.
- Key mapping timeout code is confirmed incomplete (contains TODO comments) and will be completed to full functionality.
- The inputState.Reset() simplification will use an auto-reset wrapper pattern where commands opt out of reset explicitly.
