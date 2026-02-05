# Feature Specification: Marks Navigation

**Feature Branch**: `003-marks-navigation`
**Created**: 2026-02-05
**Status**: Draft
**Input**: User description: "new feature marks breadcrumbs. In normal mode user can set mark anywhere in file by using `m`<identifier> command. For example m1. Then he always can return cursor to this place using ` command, for example `1 - will return cursor to the place, where m1 was set."

## Glossary

- **Mark**: A saved cursor position in the file, identified by a single character (the identifier)
- **Identifier**: A single alphanumeric character (0-9, a-z, A-Z) used to name a mark
- **Jump to Mark**: The action of moving the cursor to a previously saved mark position

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Set and Jump to Mark (Priority: P1)

A user is editing a file and wants to quickly navigate between different sections. They set a mark at their current position using `m` followed by an identifier (e.g., `m1`), then navigate elsewhere in the file. When they want to return, they press backtick followed by the identifier (e.g., `` `1 ``) to jump back to the marked position.

**Why this priority**: This is the core functionality of the feature - without setting and jumping to marks, the entire feature has no value.

**Independent Test**: Can be tested by opening any file, positioning cursor, pressing `m1` to set mark, moving cursor elsewhere, pressing `` `1 `` to verify cursor returns to marked position.

**Acceptance Scenarios**:

1. **Given** user is in Normal mode with cursor at line 50 column 10, **When** user presses `m` then `1`, **Then** a mark named "1" is stored with position (row 50, col 10) and no visible change occurs
2. **Given** user has set mark "1" at line 50, **When** user moves to line 100 and presses `` ` `` then `1`, **Then** cursor moves to line 50 column 10 (the marked position)
3. **Given** user has set mark "a" at a position, **When** user sets mark "a" again at a different position, **Then** the old mark is overwritten with the new position

---

### User Story 2 - Use Multiple Marks (Priority: P1)

A user is working on a complex file and needs to jump between multiple locations frequently (e.g., a function definition, its usage, and a related test). They set multiple marks with different identifiers and can jump to any of them independently.

**Why this priority**: Multiple marks are essential for practical use - a single mark provides limited value compared to just using undo/cursor history.

**Independent Test**: Can be tested by setting marks `m1`, `m2`, `m3` at different positions, then jumping to each with `` `1 ``, `` `2 ``, `` `3 `` to verify each jumps to the correct position.

**Acceptance Scenarios**:

1. **Given** user sets mark "1" at line 10 and mark "2" at line 50, **When** user is at line 100 and presses `` `1 ``, **Then** cursor moves to line 10
2. **Given** user has marks "1" and "2" set, **When** user presses `` `2 ``, **Then** cursor moves to mark "2" position (not affected by mark "1")
3. **Given** user has marks "a", "b", "c" set, **When** user overwrites mark "b", **Then** marks "a" and "c" remain unchanged

---

### User Story 3 - Invalid Mark Handling (Priority: P2)

A user attempts to jump to a mark that doesn't exist. The editor should handle this gracefully without disrupting the user's workflow.

**Why this priority**: Error handling is important for user experience but not core functionality.

**Independent Test**: Can be tested by attempting to jump to an unset mark identifier and verifying cursor doesn't move and no crash occurs.

**Acceptance Scenarios**:

1. **Given** no marks have been set, **When** user presses `` ` `` then `1`, **Then** cursor position remains unchanged
2. **Given** mark "1" is set but mark "2" is not, **When** user presses `` `2 ``, **Then** cursor position remains unchanged
3. **Given** user presses `` ` ``, **When** user presses an invalid identifier (non-alphanumeric), **Then** the command is cancelled and cursor remains unchanged

---

### Edge Cases

- What happens when mark position is beyond current file length? (Line was deleted) → Cursor moves to end of file or closest valid position
- What happens when mark column is beyond line length? (Line was shortened) → Cursor moves to end of that line
- How are marks handled across buffer/file switches? → Marks are per-buffer (each open file has its own marks)
- What identifiers are valid? → Single alphanumeric characters: 0-9, a-z, A-Z (62 possible marks)
- What happens if user presses `m` in Insert mode? → The 'm' character is inserted normally (marks only work in Normal mode)
- What happens if user presses `` ` `` and then Escape? → The pending backtick command is cancelled

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow setting marks only in Normal mode
- **FR-002**: System MUST accept `m` followed by a single alphanumeric character (0-9, a-z, A-Z) to set a mark
- **FR-003**: System MUST store the current cursor row and column when a mark is set
- **FR-004**: System MUST accept `` ` `` (backtick) followed by a single alphanumeric character to jump to a mark
- **FR-005**: System MUST move cursor to the stored row and column when jumping to a valid mark
- **FR-006**: System MUST overwrite existing mark if user sets a mark with an already-used identifier
- **FR-007**: System MUST leave cursor unchanged when jumping to a non-existent mark
- **FR-008**: System MUST maintain separate marks for each open buffer (per-file marks)
- **FR-009**: System MUST handle invalid mark positions gracefully (deleted lines, shortened lines)
- **FR-010**: System MUST show visual feedback in status bar when waiting for mark identifier (e.g., "m_" or "`_")
- **FR-011**: System MUST cancel pending mark command if user presses Escape

### Key Entities

- **Mark**: Represents a saved position containing: identifier (single char), row (line number), column (position in line)
- **MarkRegistry**: Collection of marks for a buffer, keyed by identifier character

## Assumptions

- Marks do not persist between editor sessions (cleared when file is closed)
- Marks use the same identifier namespace as vim (a-z for file-local, A-Z could be global in future)
- Maximum of 62 marks per buffer (10 digits + 26 lowercase + 26 uppercase)
- Marks are not visible in the editor (no gutter indicators) - this could be added as enhancement

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can set a mark and return to it within 3 keystrokes (e.g., `m1` to set, `` `1 `` to return)
- **SC-002**: Mark operations complete instantly with no perceptible delay
- **SC-003**: Users can maintain up to 62 independent marks per file
- **SC-004**: Jump to mark correctly positions cursor at saved row and column 100% of the time when position is still valid
- **SC-005**: Invalid mark jumps leave cursor position unchanged (no unexpected movement)
