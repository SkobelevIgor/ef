# Tasks: Search Mode

**Input**: Design documents from `/specs/001-search-mode/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Manual terminal testing only (per constitution - no automated tests)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

**Updated**: 2026-02-04 (Added Phase 7 for query preservation change request)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: `editor/` package at repository root
- Go files follow existing patterns in the codebase

---

## Phase 1: Setup (Core Data Structures)

**Purpose**: Create the foundational search data structures

- [x] T001 [P] Create SearchMatch struct and SearchState struct in editor/search.go
- [x] T002 [P] Add NewSearchState constructor and ParseQuery function in editor/search.go
- [x] T003 Extend InputState struct with Search field (*SearchState) in editor/input.go

---

## Phase 2: Foundational (Search Infrastructure)

**Purpose**: Core search infrastructure that MUST be complete before user story features

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T004 Add FindAllMatches method for case-insensitive search in editor/buffer.go
- [x] T005 Add F4 key handler to enter search mode in editor/editor.go handleKey()
- [x] T006 Add enterSearchMode() method to initialize search state in editor/editor.go
- [x] T007 Add search mode dispatch in handleKey() to route to handleSearchMode() in editor/editor.go
- [x] T008 Add handleSearchMode() skeleton method in editor/editor.go

**Checkpoint**: Search mode can be entered with F4 - foundational infrastructure ready

---

## Phase 3: User Story 1 - Basic Text Search (Priority: P1) 🎯 MVP

**Goal**: Enable users to search for text, navigate matches with n/N, and exit with Escape

**Independent Test**: Open file, press F4, type query, press Enter, navigate with n/N, verify cursor moves to matches

### Implementation for User Story 1

- [x] T009 [US1] Add renderSearchBar() method to render search input row at top of screen in editor/screen.go
- [x] T010 [US1] Modify Render() to conditionally show search bar and adjust content area in editor/screen.go
- [x] T011 [US1] Implement query input handling (KeyRune, Backspace) in handleSearchMode() in editor/editor.go
- [x] T012 [US1] Add updateSearchMatches() method to recalculate matches on query change in editor/editor.go
- [x] T013 [US1] Implement incremental search highlighting (first match after CursorZero) in editor/editor.go
- [x] T014 [US1] Implement confirmSearch() method to enable n/N navigation after Enter in editor/editor.go
- [x] T015 [US1] Add findFirstMatchAfterCursor() helper to find match after CursorZero in editor/search.go
- [x] T016 [US1] Implement nextMatch() for forward navigation with wrapping in editor/editor.go
- [x] T017 [US1] Implement prevMatch() for backward navigation with wrapping in editor/editor.go
- [x] T018 [US1] Add navigateToCurrentMatch() to move cursor and highlight match in editor/editor.go
- [x] T019 [US1] Implement exitSearchMode() with cursor positioning logic in editor/editor.go
- [x] T020 [US1] Handle Escape key: return to CursorZero if not confirmed, end of match if confirmed in editor/editor.go
- [x] T021 [US1] Add "No matches" feedback display in renderSearchBar() in editor/screen.go
- [x] T022 [US1] Handle empty query (prevent search, stay in search row) in handleSearchMode() in editor/editor.go
- [x] T023 [US1] Add match highlighting in renderPane() for current search match in editor/screen.go

**Checkpoint**: Basic search fully functional - can find text, navigate matches, exit properly

---

## Phase 4: User Story 2 - Case-Insensitive Search (Priority: P1)

**Goal**: All searches match regardless of case differences

**Independent Test**: Search "hello" in file containing "Hello", "HELLO", "hElLo" - all should match

### Implementation for User Story 2

- [x] T024 [US2] Verify FindAllMatches uses strings.ToLower for case-insensitive matching in editor/buffer.go
- [x] T025 [US2] Add test case handling for Unicode case folding edge cases in editor/buffer.go

**Checkpoint**: Case-insensitive search works correctly

---

## Phase 5: User Story 3 - Find and Replace (Priority: P2)

**Goal**: Enable users to replace text using `replace::<search>::<replacement>` syntax

**Independent Test**: Enter `replace::foo::bar`, press Enter to replace or n to skip, verify text changes correctly

### Implementation for User Story 3

- [x] T026 [US3] Integrate ParseQuery() call in confirmSearch() to detect replace syntax in editor/editor.go
- [x] T027 [US3] Set IsReplaceMode and ReplaceText fields when replace syntax detected in editor/editor.go
- [x] T028 [US3] Implement replaceCurrentMatch() method in editor/editor.go
- [x] T029 [US3] Add undo history recording for each replacement using history.Push() in editor/editor.go
- [x] T030 [US3] Handle Enter key in replace mode: perform replacement and move to next in editor/editor.go
- [x] T031 [US3] Handle n key in replace mode: skip current match without replacing in editor/editor.go
- [x] T032 [US3] Handle N key in replace mode: skip to previous match without replacing in editor/editor.go
- [x] T033 [US3] Recalculate match positions after each replacement in editor/editor.go
- [x] T034 [US3] Handle empty replacement text (delete matched occurrence) in replaceCurrentMatch() in editor/editor.go
- [x] T035 [US3] Update renderSearchBar() to show "Replace:" prompt in replace mode in editor/screen.go

**Checkpoint**: Find and replace fully functional - can replace, skip, undo individual replacements

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Edge cases, cleanup, and final validation

- [x] T036 [P] Handle malformed replace syntax (e.g., `replace::foo`) as literal search in editor/editor.go
- [x] T037 [P] Handle single match wrapping (n/N stays on same match) in editor/editor.go
- [x] T038 [P] Ensure search works from all modes (Normal, Insert, Visual) in editor/editor.go
- [x] T039 Remove all search highlights on exitSearchMode() in editor/editor.go
- [x] T040 Add scheduleAutoSave() call after replacements in editor/editor.go

**Checkpoint**: All edge cases handled

---

## Phase 7: Change Request - Preserve Query on F4 Re-entry (Priority: P1)

**Goal**: When user presses F4 after a confirmed search, preserve the previous query instead of starting empty

**Independent Test**: Open file, press F4, type query, press Enter, press Escape, press F4 again - query should be preserved and n/N navigation should work immediately

**Requirements**: FR-023, FR-024, FR-025

### Implementation for Query Preservation

- [x] T043 [US1] Add lastSearchQuery string field to Editor struct in editor/editor.go
- [x] T044 [US1] Modify exitSearchMode() to save query to e.lastSearchQuery when Confirmed=true in editor/editor.go
- [x] T045 [US1] Modify enterSearchMode() to restore lastSearchQuery if available in editor/editor.go
- [x] T046 [US1] Set Confirmed=true when restoring query to enable immediate n/N navigation in editor/editor.go
- [x] T047 [US1] Calculate matches for restored query in enterSearchMode() in editor/editor.go
- [x] T048 [US1] Find first match after current cursor when restoring query in editor/editor.go

**Checkpoint**: Query preservation fully functional - F4 reopens with previous query ready for navigation

---

## Phase 8: Final Validation

**Purpose**: End-to-end testing and rebuild

- [ ] T049 Run manual testing per quickstart.md testing checklist
- [x] T050 Delete old binary and rebuild with `go build -o ef .`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately ✅ COMPLETE
- **Foundational (Phase 2)**: Depends on Setup completion ✅ COMPLETE
- **User Story 1 (Phase 3)**: Depends on Foundational phase completion ✅ COMPLETE
- **User Story 2 (Phase 4)**: Depends on Foundational ✅ COMPLETE
- **User Story 3 (Phase 5)**: Depends on User Story 1 completion ✅ COMPLETE
- **Polish (Phase 6)**: Depends on all user stories being complete ✅ COMPLETE
- **Query Preservation (Phase 7)**: Depends on User Story 1 - builds on search foundation 🔄 NEW
- **Final Validation (Phase 8)**: Depends on Phase 7 completion

### User Story Dependencies

- **User Story 1 (P1)**: Core search - REQUIRED for US3 ✅
- **User Story 2 (P1)**: Case-insensitive matching - independent ✅
- **User Story 3 (P2)**: Find and replace - DEPENDS on US1 ✅
- **Query Preservation**: Enhancement to US1 - requires existing search infrastructure 🔄 NEW

### Within Each Phase

- Tasks without [P] marker must be completed in order
- Tasks with [P] marker can run in parallel (different files)

### Parallel Opportunities

**Phase 7 (query preservation):**
```
T043 Add lastSearchQuery field - editor.go (Editor struct)
T044-T048 Sequential - all in editor.go, modify related methods
```

---

## Implementation Strategy

### Current State

Phases 1-6 are COMPLETE. Search mode is fully functional with:
- Basic text search ✅
- Case-insensitive matching ✅
- Find and replace ✅
- All edge cases handled ✅

### Change Request Implementation

1. Complete Phase 7: Query Preservation (T043-T048)
2. Run manual testing (T049)
3. Rebuild binary (T050)
4. **VALIDATE**: Test F4 re-entry preserves query

### Critical Path

```
T043 → T044 → T045-T048 → T049 → T050
(Field) (Save) (Restore)  (Test) (Build)
```

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Manual testing required per constitution (no automated tests)
- Commit after each logical group of tasks
- Rebuild binary after all changes: `rm -f ef && go build -o ef .`
- Phase 7 is a change request adding query persistence feature to US1
- Tasks T043-T048 all modify editor/editor.go but different methods
