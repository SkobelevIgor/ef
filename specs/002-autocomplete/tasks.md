# Tasks: Autocomplete

**Input**: Design documents from `/specs/002-autocomplete/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Manual terminal testing per constitution (no automated tests)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

## Path Conventions

- **Project structure**: `editor/` at repository root (existing Go project)
- New file: `editor/autocomplete.go`
- Modified files: `editor/input.go`, `editor/editor.go`, `editor/screen.go`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create new file and data structures

- [x] T001 Create editor/autocomplete.go with package declaration and imports
- [x] T002 [P] Add Autocomplete field to InputState struct in editor/input.go

---

## Phase 2: Foundational (Core Data Structures)

**Purpose**: Implement data structures and core algorithms that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T003 Implement AutocompleteState struct in editor/autocomplete.go
- [x] T004 Implement Suggestion struct in editor/autocomplete.go
- [x] T005 Implement isDelimiter() helper function in editor/autocomplete.go
- [x] T006 Implement ExtractWords() function in editor/autocomplete.go
- [x] T007 Implement MatchSubsequence() function in editor/autocomplete.go (case-insensitive)
- [x] T008 Implement ScoreSuggestion() function in editor/autocomplete.go (prefix bonus, length penalty)
- [x] T009 Implement FindSuggestions() function in editor/autocomplete.go (filter, score, sort, limit to 10)
- [x] T010 Implement NewAutocompleteState() constructor in editor/autocomplete.go
- [x] T011 Implement Selected() method on AutocompleteState in editor/autocomplete.go

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Basic Word Completion (Priority: P1) 🎯 MVP

**Goal**: User types 2+ chars, dropdown appears with suggestions, Tab accepts top suggestion

**Independent Test**: Open file with existing words, type 2+ chars, verify dropdown appears, press Tab to accept

### Implementation for User Story 1

- [x] T012 [US1] Implement getCurrentWordPrefix() helper method in editor/editor.go
- [x] T013 [US1] Implement triggerAutocomplete() method in editor/editor.go
- [x] T014 [US1] Implement acceptAutocomplete() method in editor/editor.go
- [x] T015 [US1] Add autocomplete trigger call after character insert in handleInsertMode() in editor/editor.go
- [x] T016 [US1] Add Tab key handling when autocomplete active in handleInsertMode() in editor/editor.go
- [x] T017 [US1] Implement basic renderAutocomplete() method in editor/screen.go (below cursor only)
- [x] T018 [US1] Call renderAutocomplete() from Screen.Render() in editor/screen.go
- [x] T019 [US1] Dismiss autocomplete on backspace when prefix < 2 chars in editor/editor.go
- [x] T020 [US1] Update suggestions on continued typing in editor/editor.go
- [x] T021 [US1] Hide dropdown when no matches in editor/editor.go
- [x] T022 [US1] Rebuild binary and manual test: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 1 complete - basic autocomplete functional

---

## Phase 4: User Story 2 - Keyboard Navigation (Priority: P1)

**Goal**: User navigates suggestions with Ctrl+n/Ctrl+p, Tab accepts highlighted item

**Independent Test**: Trigger dropdown with multiple suggestions, use Ctrl+n/Ctrl+p to navigate, Tab to accept

### Implementation for User Story 2

- [x] T023 [US2] Implement Next() method on AutocompleteState in editor/autocomplete.go (with wrap)
- [x] T024 [US2] Implement Prev() method on AutocompleteState in editor/autocomplete.go (with wrap)
- [x] T025 [US2] Add Ctrl+n key handling when autocomplete active in handleInsertMode() in editor/editor.go
- [x] T026 [US2] Add Ctrl+p key handling when autocomplete active in handleInsertMode() in editor/editor.go
- [x] T027 [US2] Update renderAutocomplete() to highlight selected item in editor/screen.go
- [x] T028 [US2] Rebuild binary and manual test navigation: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 2 complete - navigation functional

---

## Phase 5: User Story 3 - Shallow Matching (Priority: P2)

**Goal**: Typing non-consecutive chars (e.g., "fn") matches words containing those chars in order (e.g., "function")

**Independent Test**: Type "fn" in file containing "function", verify it appears in suggestions

### Implementation for User Story 3

- [x] T029 [US3] Verify MatchSubsequence() handles shallow matching correctly in editor/autocomplete.go
- [x] T030 [US3] Ensure scoring prioritizes prefix matches over shallow matches in editor/autocomplete.go
- [x] T031 [US3] Rebuild binary and manual test shallow matching: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 3 complete - shallow matching functional

---

## Phase 6: User Story 4 - Dropdown Positioning (Priority: P2)

**Goal**: Dropdown appears above cursor when near bottom of screen

**Independent Test**: Position cursor at bottom of terminal, trigger autocomplete, verify dropdown appears above

### Implementation for User Story 4

- [x] T032 [US4] Add ShowAbove field to AutocompleteState if not present in editor/autocomplete.go
- [x] T033 [US4] Calculate available space below cursor in renderAutocomplete() in editor/screen.go
- [x] T034 [US4] Position dropdown above cursor when insufficient space below in editor/screen.go
- [x] T035 [US4] Handle edge case: truncate dropdown if neither above nor below has enough space in editor/screen.go
- [x] T036 [US4] Rebuild binary and manual test positioning: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 4 complete - smart positioning functional

---

## Phase 7: User Story 5 - Dismiss Autocomplete (Priority: P2)

**Goal**: User can dismiss dropdown with Escape or by moving cursor

**Independent Test**: Trigger dropdown, press Escape to dismiss, verify text unchanged

### Implementation for User Story 5

- [x] T037 [US5] Add Escape key handling to dismiss autocomplete in handleInsertMode() in editor/editor.go
- [x] T038 [US5] Dismiss autocomplete on cursor movement (arrow keys) in editor/editor.go
- [x] T039 [US5] Dismiss autocomplete on mode change in editor/editor.go
- [x] T040 [US5] Rebuild binary and manual test dismiss: `rm -f ef && go build -o ef .`

**Checkpoint**: User Story 5 complete - dismiss functionality working

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup and edge case handling

- [x] T041 [P] Ensure word-in-progress excluded from suggestions in editor/autocomplete.go
- [x] T042 [P] Ensure duplicate words appear only once in suggestions in editor/autocomplete.go
- [x] T043 [P] Add distinct background/foreground colors for dropdown styling in editor/screen.go
- [x] T044 [P] Add border or visual separator for dropdown in editor/screen.go
- [x] T045 Final rebuild and comprehensive manual testing: `rm -f ef && go build -o ef .`
- [x] T046 Run quickstart.md testing checklist validation

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phases 3-7)**: All depend on Foundational phase completion
- **Polish (Phase 8)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational - No dependencies on other stories
- **User Story 2 (P1)**: Can start after Foundational - Builds on US1 rendering but independently testable
- **User Story 3 (P2)**: Can start after Foundational - Independent of US1/US2
- **User Story 4 (P2)**: Can start after US1 - Enhances US1 rendering
- **User Story 5 (P2)**: Can start after Foundational - Independent of other stories

### Within Each User Story

- Models/structs before functions
- Core logic before integration
- Integration before rendering
- Binary rebuild after each story for validation

### Parallel Opportunities

- T001 and T002 can run in parallel (different files)
- T023 and T024 can run in parallel (same file but independent methods)
- T041, T042, T043, T044 can run in parallel (independent enhancements)
- User Stories 3 and 5 can run in parallel after Foundational phase

---

## Parallel Example: Foundational Phase

```bash
# After T003-T004 (structs), these can run in parallel:
Task: "T005 Implement isDelimiter() helper"
Task: "T007 Implement MatchSubsequence()"
Task: "T008 Implement ScoreSuggestion()"
```

## Parallel Example: Polish Phase

```bash
# These can all run in parallel:
Task: "T041 Ensure word-in-progress excluded"
Task: "T042 Ensure duplicate words appear only once"
Task: "T043 Add distinct background/foreground colors"
Task: "T044 Add border or visual separator"
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL)
3. Complete Phase 3: User Story 1 (basic completion)
4. Complete Phase 4: User Story 2 (navigation)
5. **STOP and VALIDATE**: Test basic autocomplete independently
6. This delivers a fully functional autocomplete feature

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test → Basic completion works (MVP!)
3. Add User Story 2 → Test → Navigation works
4. Add User Story 3 → Test → Shallow matching works
5. Add User Story 4 → Test → Smart positioning works
6. Add User Story 5 → Test → Dismiss works
7. Polish → Final testing → Feature complete

---

## Notes

- [P] tasks = different files or independent functions, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Binary rebuild required after changes per constitution
