# Tasks: Multi-File Splits

**Input**: Design documents from `/specs/005-multi-file-splits/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md

**Tests**: No automated tests requested. Manual terminal testing per constitution requirements.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

---

## Change Request (2026-02-05): Swap Default Split Mode and Flag

**Request**: By default open files with vertical separation, change -v key with -h. If -h key provided, editor opens files with horizontal split.

- [x] CR001 Change default split mode from SplitHorizontal to SplitVertical in main.go
- [x] CR002 Change flag from `-v` to `-h` for horizontal splits in main.go
- [x] CR003 Update usage message to show `[-h]` instead of `[-v]` in main.go
- [x] CR004 Update quickstart.md to reflect new default (vertical) and new flag (-h)
- [x] CR005 Rebuild binary and verify usage message shows correct flag

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: Repository root with `main.go` and `editor/` package
- No new directories needed - changes to existing files only

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add SplitMode type and constants that all stories will use

- [x] T001 Add SplitMode type and constants (SplitHorizontal, SplitVertical, MinPaneHeightHorizontal, MinPaneWidthVertical) to editor/editor.go
- [x] T002 Add splitMode field to Editor struct in editor/editor.go
- [x] T003 Modify editor.New() signature to accept SplitMode parameter and store it in editor/editor.go

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Modify CLI argument parsing to support `-h` flag and unlimited files

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T004 Remove the 2-file limit check in main.go (currently: `len(os.Args) > 3`)
- [x] T005 Add `-h` flag detection at start of argument parsing in main.go (updated from -v per change request)
- [x] T006 Update usage message to show `[-h]` option: "Usage: ef [-h] <filename[:line]> [filename2[:line]] ..." in main.go
- [x] T007 Pass detected SplitMode to editor.New() call in main.go

**Checkpoint**: Foundation ready - CLI accepts unlimited files and `-h` flag

---

## Phase 3: User Story 1 - Open Multiple Files with Vertical Splits (Default) (Priority: P1) 🎯 MVP

**Goal**: Enable opening 3+ files with vertical split layout (side-by-side) as default

**Independent Test**: Launch `./ignore/ef /tmp/a.txt /tmp/b.txt /tmp/c.txt` and verify all 3 files visible side-by-side (vertical splits are now default)

### Implementation for User Story 1

- [x] T008 [US1] Add Pane struct for computed layout (BufferIndex, StartX, StartY, Width, Height) in editor/screen.go
- [x] T009 [US1] Create calculatePaneLayoutHorizontal() function that divides screen height by number of buffers, accounting for search bar and separators, in editor/screen.go
- [x] T010 [US1] Add renderHorizontalSeparator() function to draw `─` character across full width between panes in editor/screen.go
- [x] T011 [US1] Modify screen.Render() signature to accept splitMode parameter in editor/screen.go
- [x] T012 [US1] Update editor.Run() to pass e.splitMode to screen.Render() in editor/editor.go
- [x] T013 [US1] Implement horizontal split rendering loop in screen.Render() - iterate panes, call renderPaneWithSearch() with computed startX, startY, width, height for each pane in editor/screen.go
- [x] T014 [US1] Update cursor positioning logic in screen.Render() to add pane's startY offset based on activePane index in editor/screen.go
- [x] T015 [US1] Add minimum height enforcement - skip rendering panes that don't meet MinPaneHeightHorizontal (3 lines) in editor/screen.go
- [x] T016 [US1] Update autocomplete dropdown positioning to stay within active pane boundaries in editor/screen.go

**Checkpoint**: User Story 1 complete - 3+ files can be opened with horizontal splits, pane navigation works

---

## Phase 4: User Story 2 - Horizontal Split Mode with -h Flag (Priority: P2)

**Goal**: Enable `-h` flag to display files stacked top-to-bottom (horizontal splits)

**Independent Test**: Launch `./ignore/ef -h /tmp/a.txt /tmp/b.txt /tmp/c.txt` and verify all 3 files visible stacked vertically

### Implementation for User Story 2

- [x] T017 [US2] Create calculatePaneLayoutVertical() function that divides screen width by number of buffers, accounting for separators, in editor/screen.go
- [x] T018 [US2] Add renderVerticalSeparator() function to draw `│` character from top to bottom between panes in editor/screen.go
- [x] T019 [US2] Add conditional in screen.Render() to select horizontal vs vertical layout based on splitMode in editor/screen.go
- [x] T020 [US2] Implement vertical split rendering loop - iterate panes, call renderPaneWithSearch() with computed startX, startY, width, height for each pane in editor/screen.go
- [x] T021 [US2] Update cursor positioning logic for vertical splits to add pane's startX offset based on activePane index in editor/screen.go
- [x] T022 [US2] Add minimum width enforcement - skip rendering panes that don't meet MinPaneWidthVertical (20 chars) in editor/screen.go

**Checkpoint**: User Story 2 complete - `-h` flag enables horizontal splits, all files visible stacked

---

## Phase 5: User Story 3 - Pane Navigation with Multiple Files (Priority: P3)

**Goal**: Ensure Shift+Tab cycles through all panes correctly and active pane is visually indicated

**Independent Test**: Open 4 files, press Shift+Tab 4 times, verify each pane becomes active in order and wraps back to first

### Implementation for User Story 3

- [x] T023 [US3] Verify existing checkDoubleShift() logic in editor/editor.go already uses `% len(buffers)` for cycling (no change expected, but verify)
- [x] T024 [US3] Ensure mode indicator (line number color) renders correctly for active pane only - verify in horizontal splits in editor/screen.go
- [x] T025 [US3] Ensure mode indicator (line number color) renders correctly for active pane only - verify in vertical splits in editor/screen.go
- [x] T026 [US3] Verify cursor position is preserved when switching panes (no implementation change expected, confirm behavior)

**Checkpoint**: User Story 3 complete - pane navigation works seamlessly across all open panes

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Edge cases, cleanup, and final validation

- [x] T027 [P] Handle edge case: no files provided with `-h` flag - display usage error in main.go
- [x] T028 [P] Handle edge case: terminal resize - verify panes recalculate proportionally on EventResize in editor/screen.go
- [x] T029 [P] Verify global marks (m/`) work correctly with 3+ panes in editor/editor.go
- [x] T030 [P] Verify search (F4) works correctly with 3+ panes in editor/editor.go
- [x] T031 [P] Verify file watching works for all open files with 3+ panes in editor/editor.go
- [x] T032 Delete old binary and rebuild with `go build -o ef .` then move to ignore directory
- [x] T033 Run full quickstart.md validation checklist manually

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-5)**: All depend on Foundational phase completion
  - US1 (horizontal splits) should complete before US2 (vertical splits) since US2 builds on rendering patterns from US1
  - US3 (navigation) can run after US1 completes
- **Polish (Phase 6)**: Depends on US1-US3 completion

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Can start after US1 (Phase 3) - Reuses rendering patterns from US1
- **User Story 3 (P3)**: Can start after US1 (Phase 3) - Navigation verification

### Within Each User Story

- Layout calculation functions before rendering loop
- Rendering loop before cursor positioning
- Core implementation before edge case handling

### Parallel Opportunities

- T001, T002, T003 are sequential (same file, dependent changes)
- T004, T005, T006 can run in parallel (different parts of main.go, but T007 depends on T005)
- T008, T009, T010 can run in parallel (different functions in screen.go)
- T017, T018 can run in parallel (different functions in screen.go)
- T027, T028, T029, T030, T031 can all run in parallel (different edge case verifications)

---

## Parallel Example: User Story 1 Foundation

```bash
# Launch these screen.go additions together:
Task: "Add Pane struct for computed layout in editor/screen.go"
Task: "Create calculatePaneLayoutHorizontal() function in editor/screen.go"
Task: "Add renderHorizontalSeparator() function in editor/screen.go"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T003)
2. Complete Phase 2: Foundational (T004-T007)
3. Complete Phase 3: User Story 1 (T008-T016)
4. **STOP and VALIDATE**: Test `ef file1 file2 file3` with horizontal splits
5. Rebuild binary (T032) and manual test

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test horizontal splits → MVP complete!
3. Add User Story 2 → Test `-v` flag → Vertical splits working
4. Add User Story 3 → Test navigation → Full feature complete
5. Polish phase → Edge cases handled
6. Final rebuild and quickstart validation

---

## Notes

- [P] tasks = different files or independent functions, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Manual terminal testing required per constitution (no automated tests)
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Existing pane switching logic (`% len(buffers)`) already scales to N panes
