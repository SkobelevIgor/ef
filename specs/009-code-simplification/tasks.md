# Tasks: Code Simplification

**Input**: Design documents from `/specs/009-code-simplification/`
**Prerequisites**: plan.md (required), spec.md (required), research.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Create new files that will hold shared utilities. No existing code modified yet.

- [x] T001 Create `editor/utils.go` with `VisualColumn`, `VisualLineWidth`, `NormalizeRange`, `ClampPosition`, and `isWhitespace` functions per plan D1
- [x] T002 Verify build succeeds: `go build -o ef . && mv ef ignore/`

**Checkpoint**: New utility file exists and compiles. No callers updated yet — zero behavioral risk.

---

## Phase 2: Foundational (Dead Code Removal)

**Purpose**: Remove dead code before refactoring begins

- [x] T003 Remove unused `firstNonWhitespace()` function from `editor/buffer.go:725-733` (zero callers confirmed in research R4)
- [x] T004 Verify build succeeds: `go build -o ef . && mv ef ignore/`

**Checkpoint**: Dead code removed. Build passes.

---

## Phase 3: User Story 1 - Consolidate Duplicate Utilities (Priority: P1)

**Goal**: Consolidate visual column calculations, range normalization, cursor clamping, and whitespace helpers into single implementations.

**Independent Test**: Run e2e test suite — all cursor movement, selection, rendering, and tab handling must behave identically.

### Implementation for User Story 1

- [x] T005 [US1] Update `Buffer.GetVisualColumn` and `Buffer.GetVisualLineWidth` in `editor/buffer.go:325-345` to be thin wrappers calling `VisualColumn`/`VisualLineWidth` from `editor/utils.go`, passing `b.Config.TabStop`
- [x] T006 [US1] Replace standalone `getVisualColumn` and `getVisualLineWidth` in `editor/screen.go:520-540` with direct calls to `VisualColumn`/`VisualLineWidth` from `editor/utils.go`. Update callers at screen.go:539, 562, 572
- [x] T007 [US1] Update `GetSelection()` in `editor/pane.go:220-222` to call `NormalizeRange` from `editor/utils.go`
- [x] T008 [P] [US1] Update `DeleteRange()` in `editor/buffer.go:423-425` to call `NormalizeRange` from `editor/utils.go`
- [x] T009 [P] [US1] Update `GetRange()` in `editor/buffer.go:488-490` to call `NormalizeRange` from `editor/utils.go`
- [x] T010 [US1] Refactor whitespace helpers in `editor/buffer.go` (`getLeadingWhitespace:304-315`, `stripLeadingWhitespace:676-684`, `hasContent:666-674`, `lastNonWhitespace:704-712`) to use `isWhitespace` predicate from `editor/utils.go`
- [x] T011 [US1] Update `jumpToMark()` in `editor/editor_marks.go:81-97` to use `ClampPosition` from `editor/utils.go` instead of inline boundary checks
- [x] T012 [US1] Update undo handler cursor clamping in `editor/editor_undo.go:72-75` to use `ClampPosition` from `editor/utils.go`
- [x] T013 [US1] Verify build and run e2e tests: `go build -o ef . && mv ef ignore/ && cd tests/e2e && python3 run_tests.py`

**Checkpoint**: All duplicate utility logic consolidated. E2e tests pass. Visual columns, range normalization, cursor clamping, and whitespace checks each have a single source of truth.

---

## Phase 4: User Story 2 - Extract Indentation Module (Priority: P2)

**Goal**: Move all indentation logic into a dedicated `editor/indent.go` file for cohesion.

**Independent Test**: Run e2e tests — auto-indent on Enter, reindent on paste, block reindentation with `=` must produce identical output.

### Implementation for User Story 2

- [x] T014 [US2] Create `editor/indent.go` and move from `editor/buffer.go`: `ReindentLines` (lines 735-837), `smartIndentForNewLine` (lines 233-268), `GetPrevNonEmptyLineIndent` (lines 686-702), `getIndentLevel`, `makeIndent`, `countRune` helper functions
- [x] T015 [US2] Move from `editor/editor_insert.go` to `editor/indent.go`: `reindentPastedRange` (lines 9-18), `endInsertSession` (lines 48-73), `reindentInsertSession` (lines 85-133)
- [x] T016 [US2] Verify all callers compile correctly — `editor/editor_normal.go:234,252` (paste reindent), `editor/editor_visual.go:162` (visual paste), `editor/editor.go:219,392` (ReindentEvent, F10), `editor/editor_insert.go:169` (Escape)
- [x] T017 [US2] Verify build and run e2e tests: `go build -o ef . && mv ef ignore/ && cd tests/e2e && python3 run_tests.py`

**Checkpoint**: All indentation logic lives in `editor/indent.go`. No logic changes — pure file reorganization. E2e tests pass.

---

## Phase 5: User Story 3 - Reduce Repetitive Patterns (Priority: P3)

**Goal**: Simplify inputState.Reset repetition, split performPaste, and extract search navigation helper.

**Independent Test**: Run e2e tests — all normal mode commands, paste operations, and search navigation must behave identically.

### Implementation for User Story 3

- [x] T018 [US3] Split `performPaste` in `editor/editor_paste.go:3-108` into three sub-functions: `pasteWholeLines(before bool) (int, int)` (lines 15-38), `pasteSingleLine(insertPos int)` (lines 51-66), `pasteMultiLine(insertPos int) (int, int)` (lines 68-107). Keep `performPaste` as thin dispatcher.
- [x] T019 [US3] Extract `navigateToMatch(cursorRow, cursorCol int)` helper in `editor/editor_search.go` that combines `FindFirstMatchAfterCursor` + cursor positioning. Replace 3 call sites at lines 30-35, 110-116, 275-278.
- [x] T020 [US3] Implement inputState auto-reset wrapper: change `handleNormalModeRune` return semantics in `editor/editor_normal.go` so `true` = skip reset (opt-out for digits 0-9, f/F, :, m, `, d/y) and `false` = auto-reset. Add `if !e.handleNormalModeRune(ch) { e.inputState.Reset() }` at the call site. Remove ~30 explicit `e.inputState.Reset()` calls from individual command handlers in `editor/editor_normal.go`.
- [x] T021 [US3] Remove explicit `e.inputState.Reset()` calls from `editor/editor_visual.go` handlers where the auto-reset wrapper in handleVisualMode can handle them (apply same pattern as editor_normal.go if applicable, or leave if visual mode has different reset semantics)
- [x] T022 [US3] Verify build and run e2e tests: `go build -o ef . && mv ef ignore/ && cd tests/e2e && python3 run_tests.py`

**Checkpoint**: Paste logic is split into focused sub-functions. Search navigation consolidated. inputState.Reset() calls reduced from ~45 to ~7 opt-outs. E2e tests pass.

---

## Phase 6: User Story 4 - Clean Up Incomplete Features (Priority: P4)

**Goal**: Complete the key mapping timeout implementation and add undo/redo documentation.

**Independent Test**: Configure key mappings, verify mapped key sequences execute correctly with proper timeout behavior.

### Implementation for User Story 4

- [x] T023 [US4] Define `EventMappingTimeout` custom event type implementing `tcell.Event` interface in `editor/editor.go` with `when time.Time` and `snapshot time.Time` fields per plan D6
- [x] T024 [US4] Replace empty goroutine in `tryKeyMapping()` at `editor/editor.go:477-480` with snapshot-based event posting: capture `e.mapKeyTime`, sleep `MappingTimeout`, then post `EventMappingTimeout` via `e.screen.PostEvent()`
- [x] T025 [US4] Add `EventMappingTimeout` handler in main event loop (`Run()` in `editor/editor.go`): on receipt, compare `ev.snapshot == e.mapKeyTime`, if match and `pendingMapKeys != ""` then call `flushPendingMapKeys()` and clear state
- [x] T026 [US4] Add documentation comment block above `applyRemoveText` and `applyInsertText` in `editor/editor_undo.go` explaining the mirror relationship between the two functions per plan D7
- [x] T027 [US4] Verify build and run e2e tests: `go build -o ef . && mv ef ignore/ && cd tests/e2e && python3 run_tests.py`

**Checkpoint**: Key mapping timeout fully functional with race-safe snapshot comparison. Undo/redo mirror relationship documented. E2e tests pass.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and cleanup

- [x] T028 Final build and full e2e test run: `go build -o ef . && mv ef ignore/ && cd tests/e2e && python3 run_tests.py`
- [ ] T029 Manual terminal testing: verify cursor movement with tabs, visual mode selection, mark jumps, paste operations, search navigation, indent/reindent, key mappings with timeout
- [x] T030 Verify duplication reduction: count duplicate implementations before vs after to confirm SC-001 (≥40% reduction)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (T001 must exist before T003)
- **US1 (Phase 3)**: Depends on Phase 2 — uses utilities created in T001
- **US2 (Phase 4)**: Depends on Phase 3 — whitespace helpers (used by indentation) must be refactored first
- **US3 (Phase 5)**: Independent of US2 — can start after Phase 3 if desired
- **US4 (Phase 6)**: Independent of US2 and US3 — can start after Phase 3
- **Polish (Phase 7)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (P1)**: Foundational — must complete first (creates shared utilities used by everything)
- **US2 (P2)**: Depends on US1 (uses `isWhitespace` from utils.go in moved indentation helpers)
- **US3 (P3)**: Depends on US1 only (does not depend on US2)
- **US4 (P4)**: Depends on US1 only (does not depend on US2 or US3)

### Within Each User Story

- Tasks within a story are sequential unless marked [P]
- Each story ends with build + e2e verification
- Stop at any checkpoint to validate independently

### Parallel Opportunities

- T008 and T009 can run in parallel (different functions in same file, no overlap)
- US3 and US4 can run in parallel after US1 completes (independent changes to different files)
- Within US4, T023-T025 (key mapping) and T026 (undo docs) touch different files

---

## Parallel Example: User Story 1

```bash
# After T007 completes, these can run in parallel:
Task: "T008 [P] Update DeleteRange() in editor/buffer.go"
Task: "T009 [P] Update GetRange() in editor/buffer.go"
```

## Parallel Example: After US1

```bash
# US3 and US4 can start simultaneously:
Task: "T018 [US3] Split performPaste in editor/editor_paste.go"
Task: "T023 [US4] Define EventMappingTimeout in editor/editor.go"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T002)
2. Complete Phase 2: Dead code removal (T003-T004)
3. Complete Phase 3: US1 - Consolidate duplicates (T005-T013)
4. **STOP and VALIDATE**: E2e tests pass, all utilities consolidated
5. This alone delivers the highest-impact simplification

### Incremental Delivery

1. Setup + Foundational → Utilities ready
2. US1 → Duplicate logic eliminated → Verify (MVP!)
3. US2 → Indentation module extracted → Verify
4. US3 + US4 (parallel) → Patterns simplified + timeout completed → Verify
5. Polish → Final validation

---

## Notes

- All tasks are pure refactoring — zero user-facing behavior changes
- Each phase builds and passes e2e tests before moving to next
- [P] tasks = different files/functions, no dependencies
- Commit after each task or logical group
- Stop at any checkpoint to validate independently
