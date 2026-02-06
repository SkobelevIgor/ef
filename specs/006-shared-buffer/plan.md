# Implementation Plan: Shared Buffer Synchronization

**Branch**: `006-shared-buffer` | **Date**: 2026-02-06 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/006-shared-buffer/spec.md`

## Summary

Enable multiple panes to share the same underlying buffer when opening the same file multiple times (e.g., `ef file.txt file.txt`). Changes made in one pane appear immediately in all other panes showing the same file, while each pane maintains independent cursor position, scroll offset, and selection state. This requires refactoring the current 1:1 buffer-per-pane model to a many-panes-to-one-buffer model with buffer deduplication at file open time.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (terminal rendering)
**Storage**: In-memory buffers with file persistence via existing Save/Load
**Testing**: Manual terminal testing (per constitution)
**Target Platform**: Unix terminals (macOS, Linux)
**Project Type**: Single project CLI application
**Performance Goals**: <100ms sync propagation between panes (perceived instant)
**Constraints**: Single-threaded event loop, no external processes
**Scale/Scope**: Support 5+ panes viewing same file without degradation

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | ✅ PASS | No GUI dependencies; all rendering via tcell |
| II. Vim-Style Modal Editing | ✅ PASS | Modes unchanged; each pane has independent mode state |
| III. Data Integrity | ✅ PASS | Shared buffer ensures no conflicting states; unified undo history |
| Code Quality | ✅ PASS | Changes in editor package using Go idioms |
| Minimal Dependencies | ✅ PASS | No new dependencies required |

**Gate Status**: PASS - All constitution principles satisfied.

## Project Structure

### Documentation (this feature)

```text
specs/006-shared-buffer/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
editor/
├── buffer.go            # Buffer struct - shared data (MODIFY)
├── editor.go            # Editor struct - buffer registry, pane management (MODIFY)
├── pane.go              # NEW: Pane struct - per-view state (cursor, scroll, selection)
├── history.go           # History - already tracks buffer reference (MINOR MODIFY)
├── screen.go            # Screen rendering - adapt to pane model (MODIFY)
├── watcher.go           # FileWatcher - unchanged (buffers already tracked by path)
└── [other files]        # Minimal or no changes
```

**Structure Decision**: Existing single-project structure maintained. New `pane.go` file introduced to separate per-view state from shared buffer state. This follows Go's file-per-concept convention.

## Complexity Tracking

> No constitution violations requiring justification.

## Architecture Overview

### Current Model (1 Buffer : 1 Pane)

```
Editor
├── buffers: []*Buffer     # One buffer per file opened
├── activePane: int        # Index into buffers array
└── Buffer contains:
    ├── Lines, Modified, Filename     # File data
    ├── CursorRow, CursorCol          # View state (PROBLEM: tied to buffer)
    ├── ScrollOffset                   # View state (PROBLEM: tied to buffer)
    └── Selection*                     # View state (PROBLEM: tied to buffer)
```

### Target Model (N Panes : 1 Buffer)

```
Editor
├── buffers: map[string]*Buffer   # Deduplicated by absolute path
├── panes: []*Pane                # One pane per opened file arg
├── activePane: int               # Index into panes array
│
├── Buffer (shared):
│   ├── Lines [][]rune            # Shared content
│   ├── Modified bool             # Shared modification flag
│   ├── Filename string           # File identity
│   ├── Marks map[rune]Mark       # Shared local marks
│   └── FileType, Config          # Shared settings
│
└── Pane (per-view):
    ├── Buffer *Buffer            # Reference to shared buffer
    ├── CursorRow, CursorCol      # Independent cursor
    ├── ScrollOffset              # Independent scroll
    ├── SelectionActive           # Independent selection
    ├── SelectionStart*           # Independent selection bounds
    └── Mode Mode                 # Independent editing mode
```

### Key Design Decisions

1. **Buffer Deduplication**: Use `map[string]*Buffer` keyed by absolute file path to ensure same file always returns same buffer instance.

2. **Pane State Extraction**: Move cursor, scroll, and selection from Buffer to new Pane struct. Buffer becomes pure data container.

3. **Cursor Adjustment**: When buffer content changes, all panes viewing that buffer must have cursors adjusted if they fall within or after the modified region.

4. **Unified History**: History already stores Buffer reference in Change struct. No changes needed for cross-pane undo.

5. **Mode Per Pane**: Each pane tracks its own mode (Normal/Insert/Visual) so one pane can be in Insert while another is in Normal.
