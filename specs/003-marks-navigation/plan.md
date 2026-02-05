# Implementation Plan: Marks Navigation

**Branch**: `003-marks-navigation` | **Date**: 2026-02-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/003-marks-navigation/spec.md`

## Summary

Implement vim-style marks navigation allowing users to set named positions in a file with `m<identifier>` and jump back to them with `` `<identifier> ``. Marks are stored per-buffer using a simple map structure, following the existing pattern for pending input state in `editor/input.go`.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (existing)
**Storage**: N/A (in-memory per-buffer marks, non-persistent)
**Testing**: Manual terminal testing per constitution (no automated tests)
**Target Platform**: Terminal (macOS/Linux)
**Project Type**: Single Go project with `editor/` package
**Performance Goals**: Instant mark set/jump operations (<1ms)
**Constraints**: Must work in Normal mode only, per-buffer isolation
**Scale/Scope**: Up to 62 marks per buffer (0-9, a-z, A-Z)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | PASS | All mark operations via keyboard, no GUI |
| II. Vim-Style Modal Editing | PASS | Marks only in Normal mode, `m` and `` ` `` bindings match vim |
| III. Data Integrity | PASS | Marks don't affect file content, graceful handling of invalid positions |
| Code Quality | PASS | Will rebuild binary after changes, manual testing |
| Governance | PASS | Minimal changes, follows existing patterns |

## Project Structure

### Documentation (this feature)

```text
specs/003-marks-navigation/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (from /speckit.tasks)
```

### Source Code (repository root)

```text
editor/
├── marks.go             # NEW: Mark struct, MarkRegistry type
├── input.go             # MODIFY: Add PendingMark, PendingJumpToMark state
├── editor.go            # MODIFY: Handle 'm' and '`' in handleNormalMode
├── buffer.go            # MODIFY: Add Marks field to Buffer struct
└── screen.go            # MODIFY: Show pending state in status (m_, `_)
```

**Structure Decision**: Single package modification following existing patterns. Marks logic isolated in new `marks.go` file, integration points in existing files following the pattern used for PendingFind and PendingGotoLine.

## Complexity Tracking

No constitution violations. Implementation follows existing patterns for pending input state.
