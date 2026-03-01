# Feature Specification: Reindent Selection

**Feature Branch**: `011-reindent-selection`
**Created**: 2026-03-01
**Status**: Draft
**Input**: User description: "In selection mode, select lines and press = to re-apply indentation. Ltrim the line and apply previous line's indentation. No language-specific smart formatting."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reindent Single Selected Line (Priority: P1)

A user selects one line in visual/selection mode and presses `=`. The editor left-trims the selected line and applies the indentation of the line immediately above it.

**Why this priority**: This is the simplest and most fundamental case. If reindenting a single line works correctly, the feature delivers immediate value.

**Independent Test**: Select one incorrectly indented line, press `=`, verify the line's leading whitespace matches the previous line's indentation.

**Acceptance Scenarios**:

1. **Given** a line with incorrect indentation is selected in visual mode, **When** the user presses `=`, **Then** the line is left-trimmed and receives the same indentation as the line above it.
2. **Given** a line with no indentation is selected and the previous line has 4 spaces of indentation, **When** the user presses `=`, **Then** the selected line receives 4 spaces of leading indentation.
3. **Given** a line with excessive indentation is selected and the previous line has 2 spaces, **When** the user presses `=`, **Then** the selected line is trimmed to 2 spaces of leading indentation.

---

### User Story 2 - Reindent Multiple Selected Lines (Priority: P1)

A user selects multiple lines (2 or more) in visual mode and presses `=`. Each selected line is processed sequentially from top to bottom. Each line is left-trimmed and receives the indentation of its respective previous line (which may be another already-reindented selected line).

**Why this priority**: Multi-line reindentation is the primary use case. Users typically need to fix indentation for blocks of code, not single lines.

**Independent Test**: Select 3 lines with mixed/broken indentation, press `=`, verify each line matches the indentation of the line above it.

**Acceptance Scenarios**:

1. **Given** 3 lines are selected where line 1 (the line above selection) has 4 spaces, **When** the user presses `=`, **Then** all 3 selected lines receive 4 spaces of indentation.
2. **Given** 5 lines are selected with varying incorrect indentation, **When** the user presses `=`, **Then** each line is reindented to match the indentation of the line immediately preceding it (processed top to bottom).
3. **Given** lines are selected across different indentation levels where the line above the first selected line has 0 indentation, **When** the user presses `=`, **Then** all selected lines are left-trimmed to 0 indentation.

---

### User Story 3 - Reindent First Line of Buffer (Priority: P2)

A user selects a line that is the very first line of the buffer (no previous line exists) and presses `=`. Since there is no previous line to reference, the line is simply left-trimmed (indentation set to 0).

**Why this priority**: This is an edge case but important for correctness. The first line has no predecessor, so the behavior must be defined.

**Independent Test**: Select the first line of a buffer, press `=`, verify the line is left-trimmed to column 0.

**Acceptance Scenarios**:

1. **Given** the first line of the buffer is selected and has leading whitespace, **When** the user presses `=`, **Then** the line is left-trimmed to 0 indentation.
2. **Given** multiple lines starting from line 1 are selected, **When** the user presses `=`, **Then** line 1 is left-trimmed to 0 indentation and subsequent lines each receive the indentation of the line above them.

---

### Edge Cases

- What happens when a selected line is empty (contains only whitespace or nothing)? The empty line is left-trimmed (resulting in an empty line) and receives the previous line's indentation as leading whitespace.
- What happens when the previous line (reference line) is empty? The indentation is considered to be 0 (no indentation).
- What happens when all selected lines already have correct indentation matching their previous lines? No visible change occurs — the operation is idempotent.
- What happens after pressing `=`? The editor exits visual/selection mode and returns to normal mode.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The editor MUST support the `=` key binding in visual/selection mode to trigger reindentation of all selected lines.
- **FR-002**: For each selected line, the editor MUST left-trim the line (remove all leading whitespace) before applying new indentation.
- **FR-003**: The new indentation for each selected line MUST be the leading whitespace of the line immediately above it (the previous line).
- **FR-004**: Lines MUST be processed sequentially from top to bottom, so that a reindented line serves as the reference for the next selected line.
- **FR-005**: If the first selected line is the first line of the buffer (no previous line exists), it MUST be left-trimmed to 0 indentation.
- **FR-006**: If the previous (reference) line is empty, the indentation applied MUST be 0.
- **FR-007**: After the reindentation operation completes, the editor MUST exit visual/selection mode and return to normal mode.
- **FR-008**: The reindentation MUST NOT apply any language-specific or smart formatting rules — it is purely mechanical indentation matching.
- **FR-009**: The `=` operation MUST work with both tab and space indentation, preserving whatever whitespace character the reference line uses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can reindent any number of selected lines (1 or more) with a single `=` keypress.
- **SC-002**: Reindented lines match the indentation of their respective previous line in 100% of cases.
- **SC-003**: The operation completes instantly (no perceptible delay) for selections of up to 1000 lines.
- **SC-004**: The feature works correctly regardless of whether the file uses tabs or spaces for indentation.

## Assumptions

- The editor already has a visual/selection mode where users can select one or more lines.
- The `=` key is not currently bound to another action in visual/selection mode (or if it is, the user intends to replace that binding).
- "Previous line" means the line at index N-1 relative to the current line at index N in the buffer, regardless of whether that line is part of the selection.
- The indentation character (tabs vs spaces) is determined by what the reference line actually contains, not by any editor setting.
