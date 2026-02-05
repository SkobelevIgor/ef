# Implementation Plan: Global Marks Navigation

**Branch**: `004-global-marks` | **Date**: 2026-02-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/004-global-marks/spec.md`

## Summary

Extend the existing per-buffer marks system to support global marks that work across files in split view. When a user sets a mark in one file and later jumps to it from another file, the editor will automatically switch the active pane and position the cursor at the marked location.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (existing)
**Storage**: N/A (in-memory global registry)
**Testing**: Manual terminal testing per constitution
**Target Platform**: macOS/Linux terminals
**Project Type**: Single CLI application
**Performance Goals**: Instant mark operations (<1ms)
**Constraints**: Must maintain backwards compatibility with single-file mode
**Scale/Scope**: 62 marks globally (0-9, a-z, A-Z), max 2 buffers

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | ✅ Pass | All mark operations are keyboard-driven within terminal |
| II. Vim-Style Modal Editing | ✅ Pass | Uses existing vim-style `m<id>` and `` `<id> `` commands |
| III. Data Integrity | ✅ Pass | Marks are non-persistent; no file data at risk |
| Code Quality | ✅ Pass | Will follow existing Go patterns in codebase |
| Governance | ✅ Pass | Binary rebuild required after changes |

**Gate Result**: PASS - No violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/004-global-marks/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
editor/
├── marks.go             # MODIFY: Add GlobalMark struct, move registry to Editor
├── editor.go            # MODIFY: Add globalMarks field, update setMark/jumpToMark
├── buffer.go            # MODIFY: Remove per-buffer Marks field (breaking change)
└── input.go             # NO CHANGE: PendingMark/PendingJumpToMark already exists
```

**Structure Decision**: This is a single-package CLI application. All changes are within the `editor/` package. The marks functionality will be moved from per-buffer (Buffer.Marks) to global (Editor.globalMarks).

## Complexity Tracking

No violations to justify - design follows existing patterns.
