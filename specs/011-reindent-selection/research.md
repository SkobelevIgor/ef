# Research: Reindent Selection

**Feature**: 011-reindent-selection
**Date**: 2026-03-01

## Findings

### 1. Existing Indentation Infrastructure

**Decision**: Reuse existing `getLeadingWhitespace()` and `stripLeadingWhitespace()` functions.
**Rationale**: Both functions are already implemented in `buffer.go` and do exactly what the feature needs — extracting leading whitespace and left-trimming. No new helpers required.
**Alternatives considered**: Creating new utility functions — rejected because existing ones are sufficient.

### 2. Visual Mode Operation Pattern

**Decision**: Follow the exact pattern used by `>` (indent) and `<` (unindent) in `editor_visual.go`.
**Rationale**: Consistency with existing codebase. The pattern is: get selection bounds → call buffer method → exit visual mode → clear selection → reset input → schedule auto-save.
**Alternatives considered**: None — the existing pattern is clean and appropriate.

### 3. `=` Key Binding Availability

**Decision**: `=` is not currently bound in visual mode — safe to add.
**Rationale**: Reviewed all cases in `handleVisualMode()` switch statement. The `=` rune is not handled.
**Alternatives considered**: N/A.

### 4. Undo Integration

**Decision**: Do not add undo recording for `=`, matching `>` and `<` behavior.
**Rationale**: The existing indent/unindent operations (`>`, `<`) do not record explicit undo history. Adding undo only for `=` would be inconsistent. If undo is desired, it should be added to all three operations uniformly in a separate feature.
**Alternatives considered**: Recording undo via `history.Push()` — deferred for consistency.

### 5. Empty Line Handling

**Decision**: Empty reference line → 0 indentation. Empty selected line → previous line's indentation only.
**Rationale**: Aligns with spec FR-006. An empty line has no meaningful indentation to copy, so 0 is the sensible default. For selected empty lines, applying the reference indentation produces whitespace-only lines which is the expected behavior for maintaining block structure.
**Alternatives considered**: Skipping empty lines entirely — rejected because it would break the sequential processing guarantee (FR-004).
