# Tasks: Reindent Selection

**Input**: Design documents from `/specs/011-reindent-selection/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, quickstart.md

**Tests**: Not requested — no test tasks included.

**Organization**: Tasks are grouped by user story. US1 and US2 share the same implementation (single method handles both single and multi-line), so they are combined. US3 (first-line edge case) is handled within the same method's row-0 branch.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: No setup needed — existing project, no new dependencies.

(No tasks — project structure and dependencies already in place.)

---

## Phase 2: Foundational

**Purpose**: No foundational work needed — all required helpers (`getLeadingWhitespace`, `stripLeadingWhitespace`) already exist in `editor/buffer.go`.

(No tasks.)

---

## Phase 3: User Stories 1 & 2 — Reindent Single/Multiple Lines (Priority: P1) MVP

**Goal**: Add `ReindentRange` buffer method and `=` key binding in visual mode so users can reindent one or more selected lines.

**Independent Test**: Open a file, enter visual mode (`v`), select lines with incorrect indentation (`j` to extend), press `=`. Verify each selected line's indentation matches the line above it.

### Implementation

- [x] T001 [P] [US1] Add `ReindentRange(startRow, endRow int)` method to Buffer in `editor/buffer.go` — place after `UnindentRange`. For each row in range: get previous line's leading whitespace via `getLeadingWhitespace()`, strip current line via `stripLeadingWhitespace()`, concatenate. Row 0 or empty previous line → 0 indentation. Set `b.Modified = true` and increment `b.ModCount`.
- [x] T002 [P] [US1] Add `'='` case in `handleVisualMode()` switch in `editor/editor_visual.go` — place after `'<'` case. Get selection bounds, call `buf.ReindentRange(startRow, endRow)`, exit visual mode, clear selection, reset input state, schedule auto-save. Follow exact pattern of `'>'` and `'<'` cases.

**Checkpoint**: US1 (single line) and US2 (multiple lines) and US3 (first line of buffer) are all functional — the `ReindentRange` method handles all cases including row-0 edge case.

---

## Phase 4: Polish & Cross-Cutting Concerns

- [x] T003 Build binary with `go build -o ef .` and move to `ignore/` directory
- [ ] T004 Manual terminal testing per quickstart.md: test single line, multiple lines, first line of buffer, empty lines, tabs vs spaces, idempotent behavior

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 3 (US1+US2+US3)**: No dependencies — can start immediately
  - T001 and T002 can run in parallel (different files)
- **Phase 4 (Polish)**: Depends on T001 and T002 completion

### Parallel Opportunities

```
# T001 and T002 can run in parallel (different files):
Task T001: "Add ReindentRange method in editor/buffer.go"
Task T002: "Add '=' case in editor/editor_visual.go"
```

---

## Implementation Strategy

### MVP (all stories in one pass)

1. Implement T001 + T002 in parallel → feature complete
2. Build and test (T003 + T004)
3. Done — all 3 user stories are satisfied by the same 2 tasks

### Notes

- This is a minimal feature (~30 lines of new code across 2 files)
- All 3 user stories (single line, multi-line, first-line edge case) are handled by the same `ReindentRange` method
- No undo recording (matches existing `>` / `<` behavior)
- No new dependencies
