# Implementation Plan: Search Mode

**Branch**: `001-search-mode` | **Date**: 2026-02-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-search-mode/spec.md`
**Updated**: 2026-02-04 (Change Request: Preserve query on F4 re-entry)

## Summary

Add Search Mode to the fe editor allowing users to:
1. Search for text occurrences with case-insensitive matching
2. Navigate between matches using n/N keys
3. Perform find-and-replace operations with individual undo support
4. **NEW**: Preserve the last confirmed search query and restore it on F4 re-entry

Technical approach: Extend InputState with SearchState pointer, add lastSearchQuery field to Editor for persistence across search sessions.

## Technical Context

**Language/Version**: Go 1.21+
**Primary Dependencies**: tcell/v2 (terminal rendering)
**Storage**: In-memory (file content in Buffer.Lines)
**Testing**: Manual terminal testing (per constitution)
**Target Platform**: Terminal (macOS, Linux)
**Project Type**: Single project - terminal text editor
**Performance Goals**: Instant search response for typical file sizes
**Constraints**: Terminal-only, keyboard-driven UI
**Scale/Scope**: Single file editing, typical source file sizes (<50k lines)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Terminal Native | ✅ PASS | Search bar renders in terminal via tcell |
| II. Vim-Style Modal | ✅ PASS | Search acts as sub-mode, n/N follow Vim conventions |
| III. Data Integrity | ✅ PASS | Replacements integrated with undo history |
| Code Quality | ✅ PASS | Go idioms, minimal dependencies |
| Governance | ✅ PASS | Manual testing, rebuild binary |

## Project Structure

### Documentation (this feature)

```text
specs/001-search-mode/
├── plan.md              # This file
├── research.md          # Phase 0 output (COMPLETE)
├── data-model.md        # Phase 1 output (COMPLETE)
├── quickstart.md        # Phase 1 output (COMPLETE)
├── tasks.md             # Phase 2 output (ready for update)
└── checklists/
    └── requirements.md  # Spec validation checklist
```

### Source Code (repository root)

```text
editor/
├── buffer.go           # Modify: Add FindAllMatches()
├── editor.go           # Modify: Add F4 handler, search methods, lastSearchQuery
├── input.go            # Modify: Add Search field
├── screen.go           # Modify: Add search bar rendering
└── search.go           # NEW: SearchState, SearchMatch structs
```

**Structure Decision**: Single project following existing `editor/` package layout. All search functionality added to existing files except new `search.go` for data structures.

## Complexity Tracking

No violations - implementation follows existing patterns.

## Change Request Log

### 2026-02-04: Preserve Query on F4 Re-entry

**Request**: When user presses F4 after a confirmed search, preserve the previous query instead of starting a new empty session.

**Impact**:
- Add `lastSearchQuery` field to Editor struct
- Modify `enterSearchMode()` to restore previous query
- Modify `exitSearchMode()` to save confirmed queries
- New acceptance scenarios US1.8 and US1.9
- New functional requirements FR-023, FR-024, FR-025

**Implementation**:
- Save query in `exitSearchMode()` when `Confirmed == true`
- Restore query in `enterSearchMode()` with `Confirmed = true` for immediate navigation
- User can still modify the restored query by typing

## Generated Artifacts

| Artifact | Status | Description |
|----------|--------|-------------|
| research.md | ✅ Complete | 8 research questions resolved |
| data-model.md | ✅ Complete | SearchState, SearchMatch, Editor extensions |
| quickstart.md | ✅ Complete | Implementation guide with code examples |
| tasks.md | 🔄 Needs Update | New tasks for query preservation |

## Next Steps

1. Run `/speckit.tasks` to generate updated task list
2. Implement the `lastSearchQuery` persistence feature
3. Manual testing per quickstart.md checklist
4. Rebuild binary: `rm -f fe && go build -o fe .`
