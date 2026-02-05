# Tasks: Marks Navigation

**Input**: Design documents from `/specs/003-marks-navigation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Manual terminal testing per constitution (no automated tests)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

## Path Conventions

- **Project structure**: `editor/` at repository root (existing Go project)
- New file: `editor/marks.go`
- Modified files: `editor/input.go`, `editor/editor.go`, `editor/buffer.go`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create new file and add supporting structures

- [x] T001 Create editor/marks.go with package declaration and imports
- [x] T002 [P] Add PendingMark and PendingJumpToMark fields to InputState struct in editor/input.go

---

## Phase 2: Foundational (Core Data Structures)

**Purpose**: Implement data structures and helpers that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T003 Implement Mark struct in editor/marks.go
- [x] T004 Implement isValidMarkIdentifier() helper function in editor/marks.go
- [x] T005 Add Marks field (map[rune]Mark) to Buffer struct in editor/buffer.go
- [x] T006 Initialize Marks map in NewBuffer() constructor in editor/buffer.go
- [x] T007 Update HasPending() to include PendingMark and PendingJumpToMark in editor/input.go
- [x] T008 Update PendingString() to return "m_" or "`_" for pending mark states in editor/input.go
- [x] T009 Update Reset() to clear PendingMark and PendingJumpToMark in editor/input.go

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Set and Jump to Mark (Priority: P1) 🎯 MVP

**Goal**: User can set a mark with `m<id>` and jump to it with `` `<id> ``

**Independent Test**: Open file, set mark with `m1`, move cursor, press `` `1 ``, verify cursor returns to marked position

### Implementation for User Story 1

- [x] T010 [US1] Handle 'm' key press in handleNormalMode() to set PendingMark=true in editor/editor.go
- [x] T011 [US1] Handle pending mark identifier input (after 'm') in handleNormalMode() in editor/editor.go
- [x] T012 [US1] Implement setMark() helper method to store mark in buffer in editor/editor.go
- [x] T013 [US1] Handle '`' (backtick) key press in handleNormalMode() to set PendingJumpToMark=true in editor/editor.go
- [x] T014 [US1] Handle pending jump identifier input (after '`') in handleNormalMode() in editor/editor.go
- [x] T015 [US1] Implement jumpToMark() helper method with position clamping in editor/editor.go
- [x] T016 [US1] Handle Escape key to cancel pending mark operations in editor/editor.go
- [x] T017 [US1] Rebuild binary and manual test: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 1 complete - basic mark set and jump functional

---

## Phase 4: User Story 2 - Use Multiple Marks (Priority: P1)

**Goal**: User can maintain multiple independent marks and jump to any of them

**Independent Test**: Set marks `m1`, `m2`, `m3` at different positions, verify each jump goes to correct position

### Implementation for User Story 2

- [x] T018 [US2] Verify map-based storage supports multiple marks in editor/buffer.go (should work from Phase 2)
- [x] T019 [US2] Test that overwriting a mark does not affect other marks in editor/editor.go
- [x] T020 [US2] Rebuild binary and manual test multiple marks: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 2 complete - multiple independent marks functional

---

## Phase 5: User Story 3 - Invalid Mark Handling (Priority: P2)

**Goal**: Graceful handling when jumping to non-existent marks or invalid positions

**Independent Test**: Jump to unset mark, verify cursor doesn't move; delete lines, jump to mark beyond file, verify cursor moves to valid position

### Implementation for User Story 3

- [x] T021 [US3] Ensure jumpToMark() returns early without moving cursor if mark not found in editor/editor.go
- [x] T022 [US3] Implement position clamping for row beyond file length in jumpToMark() in editor/editor.go
- [x] T023 [US3] Implement position clamping for column beyond line length in jumpToMark() in editor/editor.go
- [x] T024 [US3] Handle invalid identifier (non-alphanumeric) by cancelling pending state in editor/editor.go
- [x] T025 [US3] Rebuild binary and manual test edge cases: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 3 complete - robust error handling in place

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup and validation

- [x] T026 [P] Ensure marks are properly isolated per buffer (test with split view) in editor/editor.go
- [x] T027 [P] Verify visual feedback shows in status bar during pending state
- [x] T028 Final rebuild and comprehensive manual testing: `rm -f ef && go build -o ef .`
- [x] T029 Run quickstart.md testing checklist validation

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phases 3-5)**: All depend on Foundational phase completion
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational - No dependencies on other stories
- **User Story 2 (P1)**: Technically complete once US1 works (verifies map supports multiple marks)
- **User Story 3 (P2)**: Can start after Foundational - Adds robustness to US1 jump logic

### Within Each User Story

- Pending state handling before mark operations
- Set mark before jump to mark
- Binary rebuild after each story for validation

### Parallel Opportunities

- T001 and T002 can run in parallel (different files)
- T007, T008, T009 can run in parallel within same file (independent method updates)
- T026 and T027 can run in parallel (independent verification tasks)

---

## Parallel Example: Setup Phase

```bash
# Launch both setup tasks together:
Task: "Create editor/marks.go with package declaration"
Task: "Add pending fields to InputState in editor/input.go"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL)
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: Test mark set and jump independently
5. This delivers a fully functional marks feature

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test → Basic marks work (MVP!)
3. Add User Story 2 → Test → Multiple marks verified
4. Add User Story 3 → Test → Edge cases handled
5. Polish → Final testing → Feature complete

---

## Notes

- [P] tasks = different files or independent functions, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Binary rebuild required after changes per constitution
