# Implementation Plan: Autocomplete

**Branch**: `002-autocomplete` | **Date**: 2026-02-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-autocomplete/spec.md`

## Summary

Add in-file word autocomplete to the FE editor. When users type 2+ characters in Insert mode, a dropdown appears with matching words from the current file using shallow (subsequence) matching. Users navigate with Ctrl+n/Ctrl+p and accept with Tab.

## Technical Context

**Language/Version**: Go 1.21+ (existing project)
**Primary Dependencies**: github.com/gdamore/tcell/v2 (existing)
**Storage**: N/A (in-memory buffer - `Buffer.Lines`)
**Testing**: Manual terminal testing (per constitution)
**Target Platform**: Terminal (any OS with terminal support)
**Project Type**: Single CLI application
**Performance Goals**: <100ms suggestion appearance (per SC-002)
**Constraints**: Single file scope, max 10 suggestions, Insert mode only
**Scale/Scope**: Files with thousands of words

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | PASS | Dropdown rendered via tcell, keyboard-only interaction |
| II. Vim-Style Modal | PASS | Only active in Insert mode, uses standard shortcuts (Tab, Ctrl+n/p, Escape) |
| III. Data Integrity | PASS | Autocomplete only inserts text, uses existing undo system |
| Code Quality | PASS | Go idioms, minimal deps (tcell only) |
| Governance | PASS | Binary rebuild required after changes |

**All gates pass. Proceeding to Phase 0.**

## Project Structure

### Documentation (this feature)

```text
specs/002-autocomplete/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
editor/
├── autocomplete.go      # NEW: AutocompleteState, word extraction, matching logic
├── editor.go            # MODIFY: Add autocomplete state, integrate in Insert mode
├── input.go             # MODIFY: Add Autocomplete field to InputState
├── screen.go            # MODIFY: Add dropdown rendering
├── buffer.go            # Reference: Word source (Lines)
└── mode.go              # Reference: ModeInsert constant
```

**Structure Decision**: Single project structure. New `autocomplete.go` file for autocomplete logic, modifications to existing files for integration.

## Complexity Tracking

> No constitution violations. Table not required.

## Design Decisions

### Word Extraction Strategy

Extract words from `Buffer.Lines` on-demand when autocomplete triggers. Use a simple tokenizer that splits on whitespace and common programming delimiters.

### Matching Algorithm

Implement shallow (subsequence) matching: each typed character must appear in order within the candidate word, but not necessarily consecutively. Score matches by:
1. Prefix match bonus (highest priority)
2. Word length (shorter = better)
3. Alphabetical (tiebreaker)

### Dropdown Positioning

Calculate available space below cursor. If fewer than needed rows available below, position dropdown above. Use tcell's coordinate system for precise placement.

### Integration Points

1. **InputState**: Add `Autocomplete *AutocompleteState` field
2. **Insert mode handler**: Trigger autocomplete after 2+ chars typed
3. **Key handling**: Intercept Tab, Ctrl+n, Ctrl+p, Escape when dropdown visible
4. **Screen.Render**: Draw dropdown overlay after main content

## Artifacts Generated

- [research.md](./research.md) - Research findings
- [data-model.md](./data-model.md) - Data structures
- [quickstart.md](./quickstart.md) - Implementation guide
