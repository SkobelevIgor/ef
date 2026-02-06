# Tasks: Shared Buffer Synchronization

**Input**: Design documents from `/specs/006-shared-buffer/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md

**Tests**: Manual terminal testing per constitution - no automated tests.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: `editor/` package at repository root
- Paths shown below follow existing ef project structure

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create Pane struct and prepare for Buffer/Editor refactoring

- [x] T001 [P] Create Pane struct with view state fields in editor/pane.go
- [x] T002 [P] Add helper methods to Pane: clampCursor(), adjustScroll() in editor/pane.go

**Checkpoint**: Pane struct ready for integration

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [~] T003 Remove view state fields from Buffer struct in editor/buffer.go (CursorRow, CursorCol, ScrollOffset, Selection*) - Kept for compatibility, Pane fields are authoritative
- [x] T004 Add bufferRegistry map[string]*Buffer field to Editor struct in editor/editor.go
- [x] T005 Replace buffers []*Buffer with panes []*Pane in Editor struct in editor/editor.go
- [x] T006 Implement getOrCreateBuffer(filename) method using absolute path deduplication in editor/editor.go
- [x] T007 Update Editor.New() to use bufferRegistry and create Panes in editor/editor.go
- [x] T008 Update activeBuffer() to return activePane().Buffer in editor/editor.go
- [x] T009 Add activePane() method returning panes[activePaneIdx] in editor/editor.go
- [x] T010 Update all Buffer field accesses to go through Pane in editor/editor.go

**Checkpoint**: Foundation ready - Buffer and Pane separation complete, user story implementation can begin

---

## Phase 3: User Story 1 - Real-time Cross-Tab Editing (Priority: P1) 🎯 MVP

**Goal**: Changes made in one pane appear immediately in all other panes showing the same file

**Independent Test**: Open `ef file.txt file.txt`, type in pane 1, verify text appears in pane 2 immediately

### Implementation for User Story 1

- [x] T011 [US1] Update screen.Render() to accept panes slice and render each Pane's view in editor/screen.go
- [x] T012 [US1] Update renderPane() to use Pane's cursor/scroll state while reading Buffer's Lines in editor/screen.go
- [x] T013 [US1] Update handleInsertMode() to modify Buffer through active Pane in editor/editor.go
- [x] T014 [US1] Ensure all edit operations (InsertChar, DeleteChar, etc.) modify shared Buffer in editor/buffer.go
- [x] T015 [US1] Update pane switching (checkDoubleShift) to work with panes slice in editor/editor.go
- [x] T016 [US1] Update saveAllModified() to iterate unique buffers from registry in editor/editor.go
- [ ] T017 [US1] Manual test: Open ef test.txt test.txt, type in pane 1, verify pane 2 shows changes

**Checkpoint**: User Story 1 complete - real-time sync working between panes showing same file

---

## Phase 4: User Story 2 - Cursor Independence (Priority: P2)

**Goal**: Each pane maintains its own cursor position, cursors adjust when lines added/removed

**Independent Test**: Open same file twice, position cursors at different lines, insert lines in one pane, verify other pane's cursor adjusts

### Implementation for User Story 2

- [x] T018 [US2] Implement getPanesForBuffer(buf) method returning all panes viewing that buffer in editor/editor.go
- [x] T019 [US2] Implement adjustOtherPaneCursors(editRow, linesDelta) method in editor/editor.go
- [ ] T020 [US2] Call adjustOtherPaneCursors after InsertNewline operations in editor/editor.go
- [ ] T021 [US2] Call adjustOtherPaneCursors after DeleteLine operations in editor/editor.go
- [ ] T022 [US2] Call adjustOtherPaneCursors after line-affecting operations (dd, o, O) in editor/editor.go
- [x] T023 [US2] Update Pane.clampCursor() to handle cursor within deleted range in editor/pane.go (AdjustCursorForEdit already handles this)
- [ ] T024 [US2] Manual test: Open ef test.txt test.txt, cursors at lines 5 and 20, insert 3 lines at line 10, verify cursor at line 20 moves to line 23

**Checkpoint**: User Story 2 complete - independent cursors with automatic adjustment

---

## Phase 5: User Story 3 - Unified Undo History (Priority: P3)

**Goal**: Undo/redo works across all panes viewing the same buffer

**Independent Test**: Make edits in both panes of same file, press undo, verify most recent change is undone regardless of which pane made it

### Implementation for User Story 3

- [ ] T025 [US3] Verify History.Change already stores Buffer reference correctly in editor/history.go
- [ ] T026 [US3] Update undo() to find pane showing change.Buffer and switch if needed in editor/editor.go
- [ ] T027 [US3] Update redo() to find pane showing change.Buffer and switch if needed in editor/editor.go
- [ ] T028 [US3] Call adjustOtherPaneCursors after undo/redo operations in editor/editor.go
- [ ] T029 [US3] Manual test: Open ef test.txt test.txt, type "A" in pane 1, type "B" in pane 2, press undo, verify "B" is removed

**Checkpoint**: User Story 3 complete - unified undo across panes

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Edge cases, marks sharing, external file changes

- [ ] T030 [P] Update handleExternalFileChange() to update all panes showing affected buffer in editor/editor.go
- [ ] T031 [P] Ensure local marks (a-z) stored in Buffer are accessible from all panes in editor/editor.go
- [ ] T032 [P] Update GlobalMark jumping to work with pane-based navigation in editor/editor.go
- [ ] T033 Update file watcher to only watch unique buffers (not duplicate panes) in editor/editor.go
- [ ] T034 Manual test: Open 3+ panes of same file (ef test.txt test.txt test.txt), verify all sync correctly
- [ ] T035 Manual test: External edit file while open in multiple panes, verify all panes update
- [ ] T036 Rebuild binary: go build -o ef . && mv ef ignore/

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-5)**: All depend on Foundational phase completion
  - User stories should proceed sequentially (P1 → P2 → P3) for this feature
  - US2 cursor adjustment depends on US1 edit propagation working
  - US3 undo depends on cursor adjustment from US2
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Can start after US1 - Cursor adjustment requires edit propagation
- **User Story 3 (P3)**: Can start after US2 - Undo needs cursor adjustment logic

### Within Each User Story

- Implementation tasks in order listed
- Story complete before moving to next priority

### Parallel Opportunities

- T001 and T002 can run in parallel (same file but different concerns)
- T030, T031, T032 in Polish phase can run in parallel (different concerns)

---

## Parallel Example: Phase 1

```bash
# Launch both setup tasks together:
Task: "Create Pane struct with view state fields in editor/pane.go"
Task: "Add helper methods to Pane: clampCursor(), adjustScroll() in editor/pane.go"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (create Pane struct)
2. Complete Phase 2: Foundational (refactor Buffer/Editor)
3. Complete Phase 3: User Story 1 (real-time sync)
4. **STOP and VALIDATE**: Test by opening same file twice and editing
5. If working, MVP is complete - deploy/use

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test sync → **MVP Ready**
3. Add User Story 2 → Test cursor independence → Enhanced
4. Add User Story 3 → Test undo → Feature complete
5. Add Polish → Edge cases handled → Production ready

---

## Notes

- [P] tasks = different files or concerns, no dependencies
- [Story] label maps task to specific user story for traceability
- This feature has sequential story dependencies (US1 → US2 → US3)
- Manual testing per constitution requirements
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Rebuild binary after all changes: `go build -o ef . && mv ef ignore/`
