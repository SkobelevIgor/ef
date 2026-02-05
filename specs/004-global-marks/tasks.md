# Tasks: Global Marks Navigation

**Input**: Design documents from `/specs/004-global-marks/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Manual terminal testing per constitution (no automated tests)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

## Path Conventions

- **Project type**: Single CLI application
- **Source code**: `editor/` at repository root
- **Build**: `go build -o ef .`

---

## Phase 1: Setup (Data Model Changes)

**Purpose**: Add GlobalMark struct and global registry to Editor

- [x] T001 [P] Add GlobalMark struct to editor/marks.go with Buffer, Row, Col fields
- [x] T002 [P] Add globalMarks map[rune]GlobalMark field to Editor struct in editor/editor.go
- [x] T003 Initialize globalMarks map in New() function in editor/editor.go

---

## Phase 2: Foundational (Remove Per-Buffer Marks)

**Purpose**: Remove old per-buffer marks system - BLOCKS all user stories

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T004 Remove Marks field from Buffer struct in editor/buffer.go
- [x] T005 Remove Marks initialization from NewBuffer() in editor/buffer.go

**Checkpoint**: Foundation ready - old marks system removed, new global registry in place

---

## Phase 3: User Story 1 - Cross-File Mark Jump (Priority: P1) 🎯 MVP

**Goal**: Enable marks to work across files in split view - user can set mark in file1, switch to file2, and jump back to file1

**Independent Test**: Open two files, set `m1` in file1, switch to file2, press `` `1 ``, verify pane switches to file1 with cursor at marked position

### Implementation for User Story 1

- [x] T006 [US1] Update setMark() to store GlobalMark with buffer reference in editor/editor.go
- [x] T007 [US1] Update jumpToMark() to find pane index for mark's buffer in editor/editor.go
- [x] T008 [US1] Add pane switching logic to jumpToMark() when mark is in different buffer in editor/editor.go
- [x] T009 [US1] Add cursor positioning after pane switch with row/col clamping in editor/editor.go
- [x] T010 [US1] Handle invalid mark case (buffer not in e.buffers) - no-op in editor/editor.go

**Checkpoint**: Cross-file mark jump works - can set mark in file1, switch to file2, jump back to file1

---

## Phase 4: User Story 2 - Overwrite Mark from Different File (Priority: P1)

**Goal**: Mark overwrite from different file updates buffer reference correctly

**Independent Test**: Set `m1` in file1, switch to file2, set `m1` again, switch to file1, press `` `1 ``, verify pane switches to file2

### Implementation for User Story 2

- [x] T011 [US2] Verify setMark() overwrites existing mark with new buffer reference in editor/editor.go (may be already working from T006)

**Checkpoint**: Mark overwrite works across files - most recent assignment wins

---

## Phase 5: User Story 3 - Same-File Mark Jump (Priority: P2)

**Goal**: When mark is in same buffer as active pane, no pane switch occurs

**Independent Test**: With two files open, set mark in file1, stay in file1, navigate elsewhere, jump to mark, verify no pane switch

### Implementation for User Story 3

- [x] T012 [US3] Add same-buffer check in jumpToMark() - skip pane switch when mark.Buffer == activeBuffer() in editor/editor.go

**Checkpoint**: Same-file marks work without unnecessary pane switching

---

## Phase 6: User Story 4 - Single File Mode Compatibility (Priority: P2)

**Goal**: Single-file mode behavior identical to original marks feature

**Independent Test**: Open single file, set mark, navigate, jump to mark, verify cursor returns to marked position

### Implementation for User Story 4

- [x] T013 [US4] Verify jumpToMark() works when e.buffers has single element - no pane operations in editor/editor.go (may be already working from T012)

**Checkpoint**: Single-file mode backwards compatible

---

## Phase 7: Polish & Edge Cases

**Purpose**: Handle edge cases and final validation

- [x] T014 [P] Add row clamping in jumpToMark() for deleted lines (row >= len(buf.Lines)) in editor/editor.go
- [x] T015 [P] Add column clamping in jumpToMark() for shortened lines in editor/editor.go
- [x] T016 Rebuild binary with `rm -f ef && go build -o ef .` and move to ignore/
- [ ] T017 Run quickstart.md validation scenarios manually in terminal

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies - can start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 - BLOCKS all user stories
- **Phase 3 (US1)**: Depends on Phase 2 - core functionality
- **Phase 4 (US2)**: Depends on Phase 3 (uses same setMark/jumpToMark)
- **Phase 5 (US3)**: Depends on Phase 3 (adds condition to jumpToMark)
- **Phase 6 (US4)**: Depends on Phase 5 (verification, may already work)
- **Phase 7 (Polish)**: Depends on all user stories

### User Story Dependencies

- **User Story 1 (P1)**: Core feature - must complete first
- **User Story 2 (P1)**: Follows US1 - tests overwrite behavior
- **User Story 3 (P2)**: Adds optimization to US1 implementation
- **User Story 4 (P2)**: Verification of backwards compatibility

### Parallel Opportunities

**Phase 1**:
```bash
# T001 and T002 can run in parallel (different files)
Task: "Add GlobalMark struct to editor/marks.go"
Task: "Add globalMarks field to Editor struct in editor/editor.go"
```

**Phase 7**:
```bash
# T014 and T015 can run in parallel (same file but independent logic)
Task: "Add row clamping in jumpToMark()"
Task: "Add column clamping in jumpToMark()"
```

---

## Implementation Strategy

### MVP First (User Story 1 + 2 Only)

1. Complete Phase 1: Setup (T001-T003)
2. Complete Phase 2: Foundational (T004-T005)
3. Complete Phase 3: User Story 1 (T006-T010)
4. Complete Phase 4: User Story 2 (T011)
5. **STOP and VALIDATE**: Test cross-file mark jump manually
6. Build and test: `rm -f ef && go build -o ef . && mv ef ignore/`

### Incremental Delivery

1. Setup + Foundational → Global registry in place
2. Add User Story 1 → Cross-file marks work → **MVP Complete**
3. Add User Story 2 → Overwrite works → **Core Complete**
4. Add User Story 3 → Same-file optimization → **Optimized**
5. Add User Story 4 → Backwards compatibility verified → **Production Ready**
6. Polish → Edge cases handled → **Release**

---

## Notes

- All changes are in `editor/` package
- Primary files: `marks.go`, `editor.go`, `buffer.go`
- No automated tests - manual terminal testing per constitution
- Commit after each phase or logical task group
- Run quickstart.md scenarios after each checkpoint
