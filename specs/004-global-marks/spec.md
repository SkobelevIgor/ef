# Feature Specification: Global Marks Navigation

**Feature Branch**: `004-global-marks`
**Created**: 2026-02-05
**Status**: Draft
**Input**: User description: "addition for 003-marks-navigation: If user opens two files at the same time, marks should work globally. If user set mark in file 1 and then jump to file 2 and then tries to move to mark 1 - cursor should return to file 1 to mark 1 place."

## Glossary

- **Global Mark**: A mark that stores both file identity and cursor position, allowing cross-file navigation
- **Active Pane**: The currently focused editor pane in split view
- **Cross-file Jump**: Navigation from one file to another via a global mark

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cross-File Mark Jump (Priority: P1)

A user has two files open in split view. They set a mark in file 1, then switch to file 2 to work on something else. When they want to return to their marked position in file 1, they press the backtick command with the mark identifier, and the editor automatically switches focus to file 1 and positions the cursor at the marked location.

**Why this priority**: This is the core functionality of the feature - enabling marks to work across files in split view is the entire purpose.

**Independent Test**: Can be tested by opening two files in split view, setting mark `m1` in file 1, switching to file 2, pressing `` `1 ``, and verifying that focus returns to file 1 with cursor at the marked position.

**Acceptance Scenarios**:

1. **Given** user has file1.go and file2.go open in split view, cursor is in file1 at line 25, **When** user presses `m1` then switches to file2, then presses `` `1 ``, **Then** active pane switches to file1 and cursor moves to line 25
2. **Given** user sets mark "a" in file1 at position (10, 5), **When** user is in file2 and presses `` `a ``, **Then** file1 pane becomes active and cursor is at row 10, column 5
3. **Given** user sets mark in file1, then closes file1, **When** user tries to jump to that mark, **Then** cursor position remains unchanged (mark is invalid)

---

### User Story 2 - Overwrite Mark from Different File (Priority: P1)

A user sets a mark in file 1, then later decides to reassign that mark to a position in file 2. The mark should update to reference the new file and position, replacing the old reference entirely.

**Why this priority**: Users need predictable behavior when reusing mark identifiers - the most recent assignment should always win.

**Independent Test**: Can be tested by setting `m1` in file1, then setting `m1` in file2, then attempting `` `1 `` from file1 to verify it jumps to file2.

**Acceptance Scenarios**:

1. **Given** user set mark "1" in file1 at line 10, **When** user switches to file2 and presses `m1` at line 50, **Then** mark "1" now references file2 line 50
2. **Given** mark "1" was reassigned from file1 to file2, **When** user is in file1 and presses `` `1 ``, **Then** focus switches to file2 and cursor moves to the marked position

---

### User Story 3 - Same-File Mark Jump in Split View (Priority: P2)

When user sets a mark and jumps to it within the same file (even in split view context), the behavior should remain consistent with the original marks feature - no pane switching occurs.

**Why this priority**: Ensuring backwards compatibility with existing single-file mark behavior is important for user expectations.

**Independent Test**: Can be tested by setting mark in current pane and jumping to it while still in same file.

**Acceptance Scenarios**:

1. **Given** user has two files open, sets mark "1" in file1, remains in file1, **When** user presses `` `1 ``, **Then** cursor moves to mark position without any pane switching
2. **Given** user is in file1 pane, **When** user sets and jumps to mark within file1, **Then** behavior is identical to single-file mode

---

### User Story 4 - Single File Mode Compatibility (Priority: P2)

When only one file is open (no split view), marks should work exactly as they did before - this feature should not change single-file behavior.

**Why this priority**: Maintaining backwards compatibility ensures existing users are not disrupted.

**Independent Test**: Can be tested by opening single file, setting marks, and verifying identical behavior to 003-marks-navigation.

**Acceptance Scenarios**:

1. **Given** user has only one file open (no split view), **When** user sets and uses marks, **Then** behavior is identical to original marks feature
2. **Given** user is in single-file mode, **When** user jumps to a mark, **Then** no pane-related operations occur

---

### Edge Cases

- What happens when the target file's pane was closed but file is still in memory? → Mark is invalid, cursor unchanged
- What happens when mark references a file that was modified and line no longer exists? → Jump to closest valid position (end of file if line deleted)
- What happens when user has same file open in both panes? → Mark stores specific pane reference, jumps to that pane
- What happens when user switches from split view to single file view? → Marks for closed file become invalid
- How are marks handled when opening a third file (replacing one in split view)? → Marks for replaced file become invalid

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST store file/buffer reference along with row and column when setting a mark
- **FR-002**: System MUST switch active pane to the target file when jumping to a mark in a different file
- **FR-003**: System MUST position cursor at the marked row and column after switching panes
- **FR-004**: System MUST leave mark unchanged if set and jumped to within the same file
- **FR-005**: System MUST allow overwriting a mark with a new position in a different file
- **FR-006**: System MUST invalidate marks when their target file is closed
- **FR-007**: System MUST maintain backwards compatibility with single-file mode
- **FR-008**: System MUST handle invalid cross-file marks gracefully (cursor unchanged)
- **FR-009**: System MUST use a single global mark registry shared across all open buffers
- **FR-010**: All 62 mark identifiers (0-9, a-z, A-Z) MUST support cross-file navigation

### Key Entities

- **GlobalMark**: Extends Mark to include: identifier, buffer/file reference, row, column
- **GlobalMarkRegistry**: Single registry replacing per-buffer registries, storing all marks globally

## Assumptions

- Marks remain non-persistent (cleared when editor closes)
- Split view supports exactly two files (current editor limitation)
- The same file cannot be open in both panes simultaneously (if it can, marks store pane reference)
- All mark identifiers become global (no separate local vs global namespace)
- This feature replaces the per-buffer mark storage from 003-marks-navigation

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can set a mark in one file and return to it from another file within 3 keystrokes
- **SC-002**: Cross-file mark jump (including pane switch) completes instantly with no perceptible delay
- **SC-003**: 100% of valid cross-file mark jumps correctly switch pane and position cursor
- **SC-004**: Single-file mark behavior remains identical to pre-feature behavior
- **SC-005**: Invalid mark jumps (closed file, non-existent mark) leave editor state unchanged
