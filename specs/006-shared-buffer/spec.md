# Feature Specification: Shared Buffer Synchronization

**Feature Branch**: `006-shared-buffer`
**Created**: 2026-02-06
**Status**: Draft
**Input**: User description: "multiple edits of the same file. When user opens the same file for example twice `ef file.txt file.txt` I want to see changes introduced from one tab in another, highly likely immediately, but if it is performance consuming, once file saved."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Real-time Cross-Tab Editing (Priority: P1)

As a user editing the same file in multiple tabs, I want changes made in one tab to appear immediately in the other tab(s), so I can see different parts of the same file while keeping all views synchronized.

**Why this priority**: This is the core feature - without synchronized views, opening the same file twice would cause confusion and potential data loss from conflicting edits.

**Independent Test**: Can be fully tested by opening `ef file.txt file.txt`, typing in one tab, and verifying the text appears in the other tab in real-time.

**Acceptance Scenarios**:

1. **Given** user opens `ef file.txt file.txt` with two panes showing the same file, **When** user types "hello" in pane 1, **Then** pane 2 displays "hello" immediately (within 100ms).

2. **Given** user has same file open in two panes, **When** user deletes a line in pane 2, **Then** pane 1 reflects the deletion immediately.

3. **Given** user has same file open in two panes with different scroll positions, **When** user edits in pane 1, **Then** pane 2 updates content while preserving its own scroll position.

---

### User Story 2 - Cursor Independence (Priority: P2)

As a user with the same file open in multiple tabs, I want each tab to maintain its own cursor position, so I can navigate and edit different sections of the file independently.

**Why this priority**: Essential for the use case of viewing/editing different parts of the same file simultaneously - without independent cursors, the feature loses most of its utility.

**Independent Test**: Can be tested by opening the same file twice, positioning cursors at different lines, and verifying each cursor remains at its position after edits.

**Acceptance Scenarios**:

1. **Given** same file open in two panes, **When** user moves cursor to line 10 in pane 1 and line 50 in pane 2, **Then** each pane maintains its respective cursor position.

2. **Given** pane 1 cursor at line 5 and pane 2 cursor at line 20, **When** user inserts 3 new lines at line 10 in pane 1, **Then** pane 1 cursor stays at line 5, and pane 2 cursor adjusts to line 23.

3. **Given** pane 1 cursor at line 15 and pane 2 cursor at line 10, **When** user deletes lines 5-8 in pane 2, **Then** pane 2 cursor adjusts appropriately, and pane 1 cursor adjusts to line 11.

---

### User Story 3 - Unified Undo History (Priority: P3)

As a user editing the same file in multiple tabs, I want undo/redo to work consistently across all views, so I don't lose changes or get confused about what can be undone.

**Why this priority**: Important for maintaining data integrity and user expectations, but secondary to the basic synchronization functionality.

**Independent Test**: Can be tested by making edits in both panes and verifying that undo in either pane correctly reverses the most recent change regardless of which pane made it.

**Acceptance Scenarios**:

1. **Given** same file open in two panes, **When** user types "A" in pane 1, then types "B" in pane 2, then presses undo in either pane, **Then** "B" is removed (most recent change).

2. **Given** same file with shared edit history, **When** user presses undo multiple times, **Then** changes are reversed in reverse chronological order regardless of source pane.

---

### Edge Cases

- What happens when user tries to open the same file more than twice (e.g., `ef file.txt file.txt file.txt`)?
  - System should handle any number of panes showing the same file, all synchronized.

- How does the system handle rapid concurrent edits in multiple panes?
  - Changes should be applied in order received; no edit should be lost.

- What happens to marks set in one pane - are they visible/usable from other panes?
  - Marks should be shared across all panes showing the same file.

- What happens when the file is modified externally while open in multiple panes?
  - External changes should be detected and reflected in all panes (existing behavior extended).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST share the same underlying buffer when the same file is opened multiple times.
- **FR-002**: System MUST propagate all content changes (insertions, deletions, replacements) to all panes viewing the same buffer immediately.
- **FR-003**: System MUST maintain independent cursor positions for each pane viewing the same buffer.
- **FR-004**: System MUST maintain independent scroll positions for each pane viewing the same buffer.
- **FR-005**: System MUST adjust cursor positions in all panes appropriately when lines are added or removed.
- **FR-006**: System MUST share undo/redo history across all panes viewing the same buffer.
- **FR-007**: System MUST share local marks across all panes viewing the same buffer.
- **FR-008**: System MUST maintain separate visual selection state per pane.
- **FR-009**: System MUST correctly handle the "Modified" indicator - showing modified state in all panes when any pane makes changes.
- **FR-010**: System MUST handle external file changes and update all panes viewing the affected buffer.

### Key Entities

- **Buffer**: The in-memory representation of a file's contents. When the same file is opened multiple times, all instances should reference the same Buffer.
- **Pane/View**: A visual representation of a buffer with its own cursor position, scroll offset, and selection state. Multiple panes can reference the same buffer.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Changes made in one pane appear in all other panes showing the same file within 100ms (perceived as instant).
- **SC-002**: User can successfully edit different sections of the same file simultaneously using multiple panes without data loss.
- **SC-003**: Cursor positions in inactive panes adjust correctly 100% of the time when lines are added/removed in the active pane.
- **SC-004**: Undo/redo operations work correctly across panes with no orphaned or inconsistent history entries.
- **SC-005**: System supports at least 5 panes viewing the same file without performance degradation.

## Assumptions

- The existing split-pane infrastructure will be extended rather than replaced.
- Performance impact of immediate synchronization is negligible for typical file sizes and edit operations.
- Buffer identification for "same file" is based on absolute file path comparison.
- The existing file watcher and external change detection will continue to work with shared buffers.
