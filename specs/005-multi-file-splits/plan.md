# Implementation Plan: Multi-File Splits

**Branch**: `005-multi-file-splits` | **Date**: 2026-02-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/005-multi-file-splits/spec.md`

## Summary

Enable the editor to open more than 2 files simultaneously with configurable split layout. By default, files are displayed in horizontal splits (stacked top-to-bottom). When the `-v` flag is provided, files are displayed in vertical splits (side-by-side left-to-right). The implementation modifies command-line argument parsing, extends the rendering system to support N panes, and ensures all existing features work correctly with multiple files.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: github.com/gdamore/tcell/v2 (terminal rendering)
**Storage**: N/A (in-memory buffer - `Buffer.Lines`)
**Testing**: Manual terminal testing (per constitution requirements)
**Target Platform**: Unix-like terminals (darwin, linux)
**Project Type**: Single CLI application
**Performance Goals**: Pane switching < 100ms, smooth resize handling
**Constraints**: Minimum pane dimensions (3 lines height for horizontal, 20 characters width for vertical)
**Scale/Scope**: Support 4-8 files simultaneously on typical terminals

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Compliance | Notes |
|-----------|------------|-------|
| I. Terminal Native | ✅ PASS | All functionality remains within tcell terminal I/O |
| II. Vim-Style Modal Editing | ✅ PASS | Modal editing unchanged; pane switching via existing Shift+Tab |
| III. Data Integrity | ✅ PASS | Auto-save, undo/redo, file watching preserved across all panes |
| Code Quality | ✅ PASS | Minimal dependencies, Go idioms maintained |
| Governance | ✅ PASS | Binary rebuild required after changes |

**Gate Result**: PASS - No violations, proceeding with implementation planning.

## Project Structure

### Documentation (this feature)

```text
specs/005-multi-file-splits/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
.
├── main.go              # MODIFY: CLI argument parsing, -v flag handling
├── editor/
│   ├── editor.go        # MODIFY: SplitMode field, layout configuration
│   ├── screen.go        # MODIFY: N-pane rendering logic
│   └── buffer.go        # NO CHANGE (existing buffer structure sufficient)
└── go.mod               # NO CHANGE (no new dependencies)
```

**Structure Decision**: Single CLI application with existing flat structure. Changes are localized to main.go, editor.go, and screen.go.

## Complexity Tracking

> No violations - table not needed.

---

## Phase 0: Research Summary

### R1: CLI Flag Parsing in Go

**Decision**: Use standard `os.Args` manual parsing (no external flag library)
**Rationale**:
- Maintains minimal dependencies per constitution
- Simple enough for single optional flag
- Consistent with existing parseFileArg approach

**Implementation**:
```go
// Parse args: ef [-v] file1[:line] file2[:line] ...
verticalSplit := false
fileArgs := os.Args[1:]
if len(fileArgs) > 0 && fileArgs[0] == "-v" {
    verticalSplit = true
    fileArgs = fileArgs[1:]
}
```

### R2: Multi-Pane Rendering Patterns

**Decision**: Array-based pane layout with computed dimensions
**Rationale**:
- Simple to implement and extend
- Works with existing Buffer slice architecture
- Easy to calculate equal division of screen space

**Pattern**:
- Horizontal splits: Divide height by N panes, assign sequential Y offsets
- Vertical splits: Divide width by N panes, assign sequential X offsets
- Separators: Draw `─` for horizontal, `│` for vertical

### R3: Minimum Dimension Constraints

**Decision**: Enforce minimums at render time, warn on startup
**Rationale**:
- Cannot predict terminal resize at startup
- Runtime enforcement handles all edge cases
- Simple stderr warning is sufficient

**Values**:
- Horizontal: min 3 lines per pane (content viewable)
- Vertical: min 20 characters per pane (line numbers + some text)

---

## Phase 1: Data Model & Contracts

### Data Model

See [data-model.md](./data-model.md) for detailed entity definitions.

**Key additions to Editor struct**:
```go
type SplitMode int
const (
    SplitHorizontal SplitMode = iota  // Default: stacked top-to-bottom
    SplitVertical                      // Side-by-side left-to-right
)

type Editor struct {
    // ... existing fields
    splitMode SplitMode  // NEW: layout direction
}
```

### API Contracts

No external API contracts (CLI application). Internal interfaces remain unchanged.

### Critical Files

1. **main.go** - CLI parsing changes
   - Add `-v` flag detection
   - Remove 2-file limit
   - Pass `SplitMode` to `editor.New()`

2. **editor/editor.go** - Editor configuration
   - Add `SplitMode` field
   - Modify `New()` signature: `New(fileInfos []FileInfo, splitMode SplitMode)`
   - No changes to pane switching logic (already uses `% len(buffers)`)

3. **editor/screen.go** - Rendering overhaul
   - Replace hard-coded 2-pane logic with N-pane loop
   - Add `calculatePaneLayout()` function
   - Add `renderHorizontalSplits()` and `renderVerticalSplits()` functions
   - Update separator rendering

### Implementation Approach

**Step 1**: Modify main.go for `-v` flag and unlimited files
**Step 2**: Add `SplitMode` to Editor struct
**Step 3**: Refactor `screen.Render()` to use dynamic pane calculation
**Step 4**: Implement horizontal split rendering (new default)
**Step 5**: Implement vertical split rendering (existing behavior as option)
**Step 6**: Add minimum dimension enforcement
**Step 7**: Test with various file counts and terminal sizes

### Quickstart

See [quickstart.md](./quickstart.md) for build and test instructions.

---

## Post-Design Constitution Re-Check

| Principle | Compliance | Notes |
|-----------|------------|-------|
| I. Terminal Native | ✅ PASS | All rendering via tcell, no external dependencies added |
| II. Vim-Style Modal Editing | ✅ PASS | No modal behavior changes |
| III. Data Integrity | ✅ PASS | All save/undo/watch mechanisms unchanged |
| Code Quality | ✅ PASS | Minimal changes, Go idioms, no new dependencies |
| Governance | ✅ PASS | Plan includes binary rebuild step |

**Final Gate Result**: PASS - Ready for task generation.
