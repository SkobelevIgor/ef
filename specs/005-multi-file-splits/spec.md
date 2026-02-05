# Feature Specification: Multi-File Splits

**Feature Branch**: `005-multi-file-splits`
**Created**: 2026-02-05
**Updated**: 2026-02-05 (Change request: swap default split mode and flag)
**Status**: Draft
**Input**: User description: "allow user to open more then 2 files at the same time. Also if user provides a key -v, files should be splitted vertically."
**Change Request**: "By default it should open files with vertical separation, change -v key with -h. If -h key provided, editor open files with horizontal split."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Open Multiple Files with Vertical Splits (Default) (Priority: P1)

As a user, I want to open more than 2 files simultaneously in vertical splits (side-by-side) by default so that I can view and edit multiple related files at once without switching between editor instances, taking advantage of modern wide screens.

**Why this priority**: This is the core functionality request - removing the current 2-file limitation is the primary user need. Vertical splits are now the default as they better utilize modern widescreen displays.

**Independent Test**: Can be fully tested by launching the editor with 3+ file arguments and verifying all files are visible in vertical split panes (side-by-side). Delivers immediate value by expanding editing capability.

**Acceptance Scenarios**:

1. **Given** the user has 3 files (a.txt, b.txt, c.txt), **When** the user runs `ef a.txt b.txt c.txt`, **Then** all 3 files are displayed in vertical splits (side-by-side left-to-right) with each file visible in its own pane.
2. **Given** the user has 4 files to open, **When** the user runs `ef file1 file2 file3 file4`, **Then** all 4 files are displayed in vertical splits with approximately equal width allocation.
3. **Given** 5 files are open in vertical splits, **When** the user navigates between panes, **Then** the cursor moves to the next/previous pane and the active pane is visually indicated.

---

### User Story 2 - Horizontal Split Mode with -h Flag (Priority: P2)

As a user, I want to use a `-h` flag to display files in horizontal splits (stacked top-to-bottom) so that I can choose the layout that best suits my workflow and screen aspect ratio.

**Why this priority**: This provides layout flexibility which enhances the core multi-file feature but is not essential for basic functionality.

**Independent Test**: Can be fully tested by launching the editor with `-h` flag and multiple files, verifying all files appear stacked vertically. Delivers value by offering layout choice.

**Acceptance Scenarios**:

1. **Given** the user has 3 files, **When** the user runs `ef -h a.txt b.txt c.txt`, **Then** all 3 files are displayed in horizontal splits (stacked top-to-bottom).
2. **Given** the user has 4 files, **When** the user runs `ef -h file1 file2 file3 file4`, **Then** all 4 files are displayed in horizontal splits with approximately equal height allocation.
3. **Given** the user provides `-h` flag with 2 files, **When** the editor opens, **Then** the files are displayed stacked top-to-bottom.

---

### User Story 3 - Pane Navigation with Multiple Files (Priority: P3)

As a user, I want to navigate between multiple panes efficiently so that I can work across all open files seamlessly.

**Why this priority**: Navigation is essential for usability but builds upon the core multi-file display feature.

**Independent Test**: Can be fully tested by opening 3+ files and using navigation shortcuts to move between all panes. Delivers value by enabling practical multi-file workflows.

**Acceptance Scenarios**:

1. **Given** 4 files are open in splits, **When** the user presses Shift+Tab repeatedly, **Then** the active pane cycles through all open panes in order.
2. **Given** multiple panes are open, **When** the user switches panes, **Then** the previously active pane's content and cursor position are preserved.
3. **Given** 3 files are open, **When** the user is on the last pane and presses Shift+Tab, **Then** focus wraps to the first pane.

---

### Edge Cases

- What happens when the user opens more files than can fit on screen with minimum readable size?
  - Each pane should have a minimum usable height (3 lines for horizontal) or width (20 characters for vertical). If minimum cannot be met, display a warning and open only as many files as can fit.
- What happens when the user provides `-h` flag without any files?
  - Display usage error message: "Usage: ef [-h] <filename[:line]> [filename2[:line]] ..."
- What happens when the terminal is resized while multiple panes are open?
  - All panes should resize proportionally while respecting minimum dimensions.
- What happens when a file argument includes `:line` syntax with the `-h` flag?
  - Line positioning should work exactly as before: `ef -h file1:50 file2:100` opens file1 at line 50 and file2 at line 100.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST accept more than 2 file arguments on the command line (minimum 1, no hard maximum enforced in parsing).
- **FR-002**: System MUST support the `-h` flag to enable horizontal split layout.
- **FR-003**: System MUST display files in vertical splits (side-by-side left-to-right) by default when no `-h` flag is provided.
- **FR-004**: System MUST display files in horizontal splits (stacked top-to-bottom) when `-h` flag is provided.
- **FR-005**: System MUST allocate approximately equal space to each pane based on available terminal dimensions.
- **FR-006**: System MUST enforce minimum pane dimensions (3 lines height for horizontal, 20 characters width for vertical) to ensure readability.
- **FR-007**: System MUST limit the number of displayed panes based on terminal size and minimum dimension requirements.
- **FR-008**: System MUST display a visual separator between panes (horizontal line for horizontal splits, vertical line for vertical splits).
- **FR-009**: System MUST allow users to navigate between all open panes using existing pane-switching mechanism (Shift+Tab).
- **FR-010**: System MUST preserve existing functionality: file watching, cursor persistence, syntax highlighting, and global marks across all open panes.
- **FR-011**: System MUST continue to support the `:line` syntax for each file argument regardless of split mode.
- **FR-012**: System MUST display an error and exit if no files are provided.

### Key Entities

- **Pane**: A rectangular viewport displaying a single buffer. Has position (x, y), dimensions (width, height), and a reference to its associated buffer.
- **Split Layout**: The arrangement of panes, either horizontal (stacked) or vertical (side-by-side). Determined by the `-h` flag at startup (default: vertical).
- **Buffer**: Existing entity representing an open file's content, cursor position, and scroll state. One buffer per open file.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can open and view at least 4 files simultaneously on a standard terminal (80x24).
- **SC-002**: Users can open and view at least 8 files simultaneously on a large terminal (200x50).
- **SC-003**: Pane switching cycles through all open panes in under 100ms response time.
- **SC-004**: Terminal resize events result in proportional pane resizing without visual glitches or data loss.
- **SC-005**: All existing features (search, marks, syntax highlighting, file watching) continue to function correctly with 3+ files open.

## Assumptions

- The `-h` flag will be the first argument when provided; file arguments follow after the flag.
- The default behavior (no flag) uses vertical splits because this better utilizes modern widescreen displays for code editing.
- Minimum pane dimensions (3 lines / 20 characters) are reasonable defaults that ensure basic usability.
- Users typically open fewer than 10 files simultaneously; extreme cases will be handled gracefully with warnings.
- The existing pane switching mechanism (Shift+Tab cycling) scales naturally to more than 2 panes without requiring new keybindings.
