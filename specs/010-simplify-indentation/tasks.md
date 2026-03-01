# Tasks: Simplify Indentation Logic

**Input**: Design documents from `/specs/010-simplify-indentation/`
**Prerequisites**: plan.md (required), spec.md (required), research.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: User Story 1 - Remove Post-Paste Reindentation (Priority: P1)

**Goal**: Paste operations produce verbatim clipboard content with no indentation correction.

**Independent Test**: Yank multiple lines with mixed indentation, paste with `p` — pasted text must be character-for-character identical to clipboard.

### Implementation for User Story 1

- [x] T001 [P] [US1] Remove `e.reindentPastedRange(buf, firstRow, lastRow)` call from `p` handler in `editor/editor_normal.go` (~line 217)
- [x] T002 [P] [US1] Remove `e.reindentPastedRange(buf, firstRow, lastRow)` call from `P` handler in `editor/editor_normal.go` (~line 234)
- [x] T003 [P] [US1] Remove `e.reindentPastedRange(buf, fr, lr)` call from visual paste handler in `editor/editor_visual.go` (~line 162)
- [x] T004 [US1] Verify build succeeds: `go build -o ef . && go test ./editor/`

**Checkpoint**: All paste operations are now verbatim. No indentation correction on paste.

---

## Phase 2: User Story 2 - Remove Insert Session Reindentation (Priority: P2)

**Goal**: Exiting insert mode and auto-save events no longer trigger retroactive reindentation.

**Independent Test**: Enter insert mode, type 3 lines with custom indentation, press Escape — all lines retain exact indentation as typed.

### Implementation for User Story 2

- [x] T005 [US2] Remove `e.endInsertSession()` call from Escape handler in `editor/editor_insert.go` (~line 94)
- [x] T006 [US2] Remove `e.endInsertSession()` call from F10 quit handler in `editor/editor.go` (~line 396)
- [x] T007 [US2] Remove `ReindentEvent` posting from `scheduleAutoSave()` in `editor/editor.go` (~line 236) — keep only `e.saveAllModified()` in the timer callback
- [x] T008 [US2] Remove `case *ReindentEvent` handler from `Run()` event loop in `editor/editor.go` (~lines 217-220)
- [x] T009 [US2] Remove `ReindentEvent` struct and `When()` method from `editor/editor_insert.go` (~lines 37-45)
- [x] T010 [US2] Remove `startInsertSession()` function from `editor/editor_insert.go` (~lines 9-14) and its call from `enterInsertMode()` (~line 23)
- [x] T011 [US2] Remove `insertStartRow` and `insertStartCol` fields from Editor struct in `editor/editor.go` (~lines 62-63)
- [x] T012 [US2] Verify build succeeds: `go build -o ef . && go test ./editor/`

**Checkpoint**: No retroactive reindentation on insert-exit or auto-save. Insert session tracking removed.

---

## Phase 3: Dead Code Removal (Priority: P3)

**Goal**: Remove functions that have no remaining callers after US1 and US2.

**Independent Test**: Build succeeds with no dead code warnings. `indent.go` reduced to ~110 lines.

### Implementation for Phase 3

- [x] T013 Remove `reindentPastedRange` function from `editor/indent.go` (~lines 216-226)
- [x] T014 Remove `endInsertSession` function from `editor/indent.go` (~lines 228-253)
- [x] T015 Remove `reindentInsertSession` function from `editor/indent.go` (~lines 255-302)
- [x] T016 Remove `ReindentLines` function from `editor/indent.go` (~lines 112-214)
- [x] T017 Remove `GetPrevNonEmptyLineIndent` function from `editor/indent.go` (~lines 83-99)
- [x] T018 Verify build succeeds and auto-indent on new lines still works: `go build -o ef . && mv ef ignore/ && go test ./editor/`

**Checkpoint**: `indent.go` contains only `smartIndentForNewLine`, `getIndentLevel`, `makeIndent`, `countRune` (93 lines). Build passes.

---

## Phase 4: Polish & Verification

**Purpose**: Final validation

- [x] T019 Final build and test: `go build -o ef . && mv ef ignore/ && go test ./editor/`
- [ ] T020 Manual terminal testing: verify paste is verbatim, Enter/o/O auto-indent works, Escape doesn't reindent, auto-save doesn't reindent

---

## Dependencies & Execution Order

### Phase Dependencies

- **US1 (Phase 1)**: No dependencies — can start immediately
- **US2 (Phase 2)**: Independent of US1 — can run in parallel
- **Dead Code (Phase 3)**: Depends on US1 and US2 completion (callers must be removed before functions)
- **Polish (Phase 4)**: Depends on all phases complete

### Parallel Opportunities

- T001, T002, T003 can all run in parallel (different files/lines, no overlap)
- US1 and US2 can run in parallel (different files)

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Remove reindent from paste (T001-T004)
2. **STOP and VALIDATE**: Paste is now verbatim
3. This alone delivers the most impactful change

### Full Delivery

1. US1 → Paste is verbatim (T001-T004)
2. US2 → Insert-exit is clean (T005-T012)
3. Dead code removal (T013-T018)
4. Polish (T019-T020)

---

## Notes

- All tasks are pure code removal — zero new functionality
- Each phase builds and passes tests before moving to next
- [P] tasks = different files, no dependencies
- Stop at any checkpoint to validate independently
