# Research: Simplify Indentation Logic

## R1: What to Remove

**Decision**: Remove all retroactive reindentation — `ReindentLines`, `reindentPastedRange`, `endInsertSession`, `reindentInsertSession`, `GetPrevNonEmptyLineIndent`, `ReindentEvent`, and insert session tracking (`insertStartRow`, `insertStartCol`, `startInsertSession`).

**Rationale**: These functions exist solely to correct indentation after paste or multi-line insert. The user wants paste to be verbatim and insert-mode typing to stay as-entered. With all callers removed, the functions become dead code.

**Callers to update**:
- `editor_normal.go:217,234` — `reindentPastedRange` in `p` and `P` handlers → remove calls
- `editor_visual.go:162` — `reindentPastedRange` in visual paste → remove call
- `editor.go:217-220` — `ReindentEvent` handler in main loop → remove case
- `editor.go:236` — `ReindentEvent` posting in `scheduleAutoSave` → remove line
- `editor.go:396` — `endInsertSession()` call in F10 quit → remove call
- `editor_insert.go:94` — `endInsertSession()` call on Escape → remove call
- `editor_insert.go:23` — `startInsertSession()` call in `enterInsertMode` → remove call

## R2: What to Keep

**Decision**: Keep `smartIndentForNewLine` and its direct helpers (`getIndentLevel`, `makeIndent`, `countRune`).

**Rationale**: These power the new-line auto-indentation (Enter, `o`, `O`) which the user wants to keep. They are small (~80 lines total), well-scoped, and already behind the `AutoIndentation` config toggle.

## R3: scheduleAutoSave Simplification

**Decision**: Remove `ReindentEvent` posting from `scheduleAutoSave`. The function becomes pure auto-save.

**Rationale**: `scheduleAutoSave` currently does two things: (1) save after delay, (2) post `ReindentEvent`. With reindentation removed, only save remains. The `ReindentEvent` type can also be deleted.

## R4: Insert Session Tracking Cleanup

**Decision**: Remove `insertStartRow`, `insertStartCol` fields from Editor struct and `startInsertSession()` function.

**Rationale**: These fields exist only to track where insert mode started for `endInsertSession`/`reindentInsertSession`. With those functions removed, the tracking has no purpose.
