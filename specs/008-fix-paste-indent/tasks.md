# Tasks: Fix Paste Indentation

**Input**: Design documents from `/specs/007-fix-paste-indent/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Foundational (Core Reindentation Engine)

**Purpose**: Build the shared `ReindentLines` and `GetPrevNonEmptyLineIndent` functions that ALL user stories depend on. Also clean up obsolete incremental paste state.

- [x] T001 [P] Add `GetPrevNonEmptyLineIndent(row int) int` method to `Buffer` in `editor/buffer.go` — walks backward from `row-1` to find first non-empty line, returns `getIndentLevel()` of that line, returns 0 if none found
- [x] T002 [P] Add `ReindentLines(startRow, endRow, contextIndent int)` method to `Buffer` in `editor/buffer.go` — scans `[startRow, endRow]` for minimum indent level across non-empty lines, then for each non-empty line: strips leading whitespace, computes `newLevel = contextIndent + (originalLevel - baseIndent)`, prepends `makeIndent(newLevel)`. Skips empty lines (FR-006). Handles mixed tabs/spaces via existing `getIndentLevel` + `makeIndent` (FR-007)
- [x] T003 Remove obsolete paste state fields from `Editor` struct in `editor/editor.go` — remove `pasteSkipWhitespace`, `pasteBaseIndent`, `pasteAutoIndent`, `pasteCollectedWS` fields. Add `pasteStartRow int` field. Remove the `applyPastedIndent()` function entirely

**Checkpoint**: Core reindentation engine ready. No user-facing behavior yet — integration follows in user story phases.

---

## Phase 2: User Story 1+2 - Multi-Line Paste with Indentation Fixing (Priority: P1) 🎯 MVP

**Goal**: Terminal paste (bracketed + timing-detected) of multi-line content gets batch-reindented to match insertion context, preserving relative structure.

**Independent Test**: Open a `.go` file with a function body at indent level 1. Paste a 5-line snippet with zero indentation. Verify all lines get tab-based indent level 1+ with relative depth preserved.

### Implementation for User Stories 1+2

- [x] T004 [US1] Modify `EventPaste` start handler in `editor/editor.go` (Run loop, ~line 227) — when `ev.Start()` is true, record `e.pasteStartRow = buf.CursorRow` (after `pane.SyncToBuffer()`)
- [x] T005 [US1] Modify `EventPaste` end handler in `editor/editor.go` (Run loop, ~line 229-234) — when paste ends, before resetting state: if `buf.FileType != "" && buf.Config.AutoIndentation`, compute `contextIndent = buf.GetPrevNonEmptyLineIndent(e.pasteStartRow)`, determine `startRow` (if cursor was mid-line at paste start, use `pasteStartRow + 1`, else `pasteStartRow`), call `buf.ReindentLines(startRow, buf.CursorRow, contextIndent)`. Then push a `ChangeReplace` covering `[pasteStartRow, CursorRow]` for undo (FR-008)
- [x] T006 [US1] Modify timing-based paste detection in `handleKey` in `editor/editor.go` (~line 399-415) — on paste start (gap < 5ms, first detection): record `e.pasteStartRow = buf.CursorRow`. On paste end (gap > 5ms after pasting): apply same reindentation logic as T005 before resetting state
- [x] T007 [US1] Remove incremental whitespace interception from `handleInsertMode` in `editor/editor.go` (~lines 745-798) — remove all `pasteSkipWhitespace` checks, `pasteCollectedWS` accumulation, and `applyPastedIndent()` calls from the Enter, KeyRune, and KeyTab handlers. Pasted characters should now be inserted normally without any interception
- [x] T008 [US1] Handle mid-line paste detection in `editor/editor.go` — when paste starts, check if `buf.CursorCol > 0` and current line is non-empty. If so, store a flag (`pasteStartedMidLine`) so that on paste end, `ReindentLines` starts from `pasteStartRow + 1` instead of `pasteStartRow` (FR-010)

**Checkpoint**: Terminal paste produces correctly indented output. Test with bracketed paste and rapid-type simulation. Verify undo restores pre-paste state in single action.

---

## Phase 3: User Story 3 - Paste As-Is for Unconfigured File Types (Priority: P2)

**Goal**: Files without efconfig entry or with `AutoIndentation: false` get raw paste — no indentation modification.

**Independent Test**: Open a `.txt` file (no efconfig entry), paste indented content, verify byte-identical output.

### Implementation for User Story 3

- [x] T009 [US3] Verify guard conditions in paste-end handlers in `editor/editor.go` — confirm that the `buf.FileType != "" && buf.Config.AutoIndentation` gate in T005 and T006 correctly skips reindentation for unconfigured file types and files with `AutoIndentation: false`. This should be inherent from the T005/T006 implementation but verify with manual testing

**Checkpoint**: Unconfigured files paste verbatim. Configured files with `AutoIndentation: false` also paste verbatim.

---

## Phase 4: User Story 4 - Single-Line Paste (Priority: P2)

**Goal**: Single-line paste adjusts indentation to match insertion point.

**Independent Test**: Paste a single line with 0 indent into a function body at indent level 2. Verify the line receives level 2 indentation.

### Implementation for User Story 4

- [x] T010 [US4] Verify `ReindentLines` handles single-line range in `editor/buffer.go` — confirm that when `startRow == endRow`, the function correctly reindents the single line (min indent = that line's indent, delta applied). This should work automatically from T002 but verify edge case where single line has no leading whitespace

**Checkpoint**: Single-line paste gets correct indentation. Mid-line single-line paste joins inline without modification.

---

## Phase 5: User Story 5 - Vim p/P Put Commands (Priority: P1)

**Goal**: Vim `p`/`P` commands in normal and visual mode apply the same indentation fixing as terminal paste.

**Independent Test**: Yank a block from indent level 0, move cursor to indent level 2, press `p`. Verify pasted block shifts to level 2.

### Implementation for User Story 5

- [x] T011 [US5] Modify `pasteAfter()` in `editor/editor.go` (~line 1141) to return `(firstRow, lastRow int)` — the range of inserted lines. For line-mode: `firstRow = CursorRow + 1`, `lastRow = firstRow + len(clipboard) - 1`. For char-mode multi-line: compute from splice logic
- [x] T012 [US5] Modify `pasteBefore()` in `editor/editor.go` (~line 1494) to return `(firstRow, lastRow int)` — same logic as T011 but for before-cursor insertion
- [x] T013 [US5] Integrate reindentation into normal-mode `p` handler in `editor/editor.go` (~line 1083) — after `pasteAfter()` returns range, if `buf.FileType != "" && buf.Config.AutoIndentation`: compute `contextIndent = buf.GetPrevNonEmptyLineIndent(firstRow)`, call `buf.ReindentLines(firstRow, lastRow, contextIndent)`. For char-mode paste where first line joins inline, use `firstRow + 1` as start. History push (ChangeReplace) already captures the final state since reindent happens before the push
- [x] T014 [US5] Integrate reindentation into normal-mode `P` handler in `editor/editor.go` (~line 1103) — same pattern as T013 but using `pasteBefore()` return values
- [x] T015 [US5] Integrate reindentation into visual-mode `p`/`P` handler in `editor/editor.go` (~line 1880) — after `DeleteRange` + `pasteAfter()`/`pasteBefore()`, apply same reindentation logic. Ensure the combined delete+paste+reindent is a single `ChangeReplace` for undo

**Checkpoint**: Vim put commands produce correctly indented output. Undo restores pre-put state. Works in both normal and visual mode.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup and build

- [x] T016 Rebuild binary: `go build -o ef .` and move to `ignore/` directory
- [x] T017 Run quickstart.md validation — test all 4 scenarios from `specs/007-fix-paste-indent/quickstart.md` manually in terminal

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Foundational)**: No dependencies — start immediately
- **Phase 2 (US1+2 Terminal Paste)**: Depends on Phase 1 completion
- **Phase 3 (US3 As-Is)**: Depends on Phase 2 (verifies guard conditions from Phase 2 implementation)
- **Phase 4 (US4 Single-Line)**: Depends on Phase 1 (verifies ReindentLines edge case)
- **Phase 5 (US5 Vim p/P)**: Depends on Phase 1 completion only
- **Phase 6 (Polish)**: Depends on all previous phases

### User Story Dependencies

- **US1+2 (Terminal Paste)**: Depends on foundational T001-T003
- **US3 (As-Is)**: Verification only — inherent from US1+2 guard conditions
- **US4 (Single-Line)**: Verification only — inherent from foundational T002
- **US5 (Vim p/P)**: Depends on foundational T001-T002 only (independent from US1+2)

### Parallel Opportunities

- **Phase 1**: T001 and T002 can run in parallel (different functions in same file)
- **Phase 2 + Phase 5**: US1+2 (terminal paste) and US5 (vim p/P) can run in parallel after Phase 1 since they modify different code sections in `editor.go`
- **Phase 3 + Phase 4**: Both are verification tasks, can run in parallel

---

## Implementation Strategy

### MVP First (User Stories 1+2)

1. Complete Phase 1: Foundational (T001-T003)
2. Complete Phase 2: Terminal paste reindentation (T004-T008)
3. **STOP and VALIDATE**: Test multi-line paste with broken indentation in a `.go` file
4. This covers the core value — broken paste gets fixed

### Incremental Delivery

1. Phase 1 → Core engine ready
2. Phase 2 → Terminal paste works → Manual test (MVP!)
3. Phase 3 → Verify unconfigured files paste verbatim
4. Phase 4 → Verify single-line paste edge case
5. Phase 5 → Vim p/P commands get same indentation fixing
6. Phase 6 → Build and full validation

---

## Notes

- T001 and T002 are the core building blocks — all other tasks depend on them
- T003 (cleanup) removes ~30 lines of obsolete code and simplifies `handleInsertMode`
- T009 and T010 are verification tasks — the behavior should be inherent from the foundational implementation, but explicit testing is needed
- All modifications are in 2 files: `editor/buffer.go` and `editor/editor.go`
