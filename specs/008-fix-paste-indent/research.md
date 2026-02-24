# Research: Fix Paste Indentation

## Decision 1: Batch vs Incremental Reindentation

**Decision**: Batch reindentation after paste completes
**Rationale**: The current incremental approach (`applyPastedIndent`) processes each line as characters stream in, which:
- Cannot fix the first pasted line (it's already inserted before any Enter arrives)
- Depends on `pasteAutoIndent` (auto-generated indent) rather than the previous non-empty line
- Creates complex state management with `pasteBaseIndent`, `pasteAutoIndent`, `pasteCollectedWS`, `pasteSkipWhitespace`

A batch approach runs once when paste ends, operating on the full range of inserted lines. This:
- Naturally handles the first line
- Uses a single context indent derived from previous non-empty line
- Works identically for insert-mode paste and vim p/P commands
- Simplifies the insert-mode paste path by removing incremental whitespace collection

**Alternatives considered**:
- Fix incremental approach: Would still need special-casing for first line, wouldn't apply to p/P commands, maintains complexity
- Post-insert formatter: Overkill — the Formatter interface exists but is unused; introducing full formatting for just indent would be premature

## Decision 2: Context Indent Source

**Decision**: Always derive from previous non-empty line (`GetPrevNonEmptyLineIndent`)
**Rationale**: Per spec clarification (Session 2026-02-21), the user explicitly chose to re-derive from the previous non-empty line rather than trusting the current line's (possibly auto-generated) whitespace. This avoids issues where auto-indent might disagree with the intended context.

**Alternatives considered**:
- Use current line indent: Simpler but can pick up stale auto-generated whitespace
- Use surrounding context (both above and below): More accurate for some cases but adds complexity and heuristics

## Decision 3: Shared Logic for All Paste Types

**Decision**: Single `ReindentLines()` function called from all paste paths
**Rationale**: Insert-mode paste, p/P normal mode, and p/P visual mode all need the same reindentation logic. A shared function in `Buffer` avoids code duplication and ensures consistent behavior (FR-009, SC-005).

**Alternatives considered**:
- Separate functions per paste type: Would duplicate the min-indent scan and reindent loop
- Middleware/hook pattern: Unnecessary abstraction for a single operation

## Decision 4: Removing applyPastedIndent

**Decision**: Remove the incremental `applyPastedIndent()` and related paste state fields
**Rationale**: The batch approach makes the incremental approach redundant. Removing it simplifies `handleInsertMode` significantly — pasted characters can be inserted normally without whitespace collection/interception. The fields `pasteSkipWhitespace`, `pasteBaseIndent`, `pasteAutoIndent`, `pasteCollectedWS` become unnecessary.

The only paste state needed: `pasting bool`, `pasteStartRow int`, `lastKeyTime time.Time`.

**Alternatives considered**:
- Keep both approaches: Redundant and risk of conflicts
- Keep incremental, skip batch: Can't fix first line or support p/P
