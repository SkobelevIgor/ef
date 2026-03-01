# Feature Specification: Simplify Indentation Logic

**Feature Branch**: `010-simplify-indentation`
**Created**: 2026-03-01
**Status**: Draft
**Input**: User description: "Simplify indentation logic. Remove any indentation correction (after multi-line paste etc.) in code. We need only auto-indentation on/off."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Remove Post-Paste Reindentation (Priority: P1)

When a user pastes text (single-line or multi-line), the editor must preserve the pasted content exactly as-is in the clipboard. No automatic reindentation or indentation correction should occur after paste operations.

**Why this priority**: This is the core request — removing the most complex and surprising indentation behavior. Pasted code should appear exactly as copied, giving users full control.

**Independent Test**: Paste multi-line code with mixed indentation levels. The pasted text must appear character-for-character identical to the clipboard content.

**Acceptance Scenarios**:

1. **Given** a user has copied 5 lines of code with various indentation, **When** they paste with `p` or `P` in normal mode, **Then** the pasted lines appear with their original indentation unchanged.
2. **Given** a user pastes code in visual mode (replacing a selection), **When** the paste completes, **Then** no reindentation is applied to the pasted content.
3. **Given** a user pastes text while in insert mode (terminal paste), **When** the insert session ends, **Then** no reindentation correction is applied to the inserted lines.

---

### User Story 2 - Remove Insert Session Reindentation (Priority: P2)

When a user exits insert mode after typing multiple lines, the editor must not retroactively adjust indentation of the lines typed during that session. The debounced reindent event should also be removed.

**Why this priority**: This eliminates the other automatic indentation correction path. Users expect their typed indentation to stay exactly as they entered it.

**Independent Test**: Enter insert mode, type several lines with manual indentation, press Escape. All lines must retain the exact indentation the user typed.

**Acceptance Scenarios**:

1. **Given** a user enters insert mode and types 3 lines with custom indentation, **When** they press Escape, **Then** all lines retain their exact indentation as typed.
2. **Given** a user is in insert mode and the auto-save timer fires, **When** the debounced reindent event would have triggered, **Then** no reindentation occurs.

---

### User Story 3 - Keep Auto-Indentation on New Lines (Priority: P3)

The editor must continue to provide automatic indentation when creating new lines (Enter key, `o`, `O` commands). This "auto-indent" behavior copies or computes indentation for the new line based on the previous line's context (braces, colons, etc.). This feature should remain controllable via the existing `AutoIndentation` configuration toggle.

**Why this priority**: Auto-indent on new lines is essential for comfortable editing. This story clarifies what indentation behavior is intentionally preserved.

**Independent Test**: With auto-indentation enabled, press Enter after a line ending with `{`. The new line must be indented one level deeper. With auto-indentation disabled, the new line must have no indentation.

**Acceptance Scenarios**:

1. **Given** auto-indentation is enabled and cursor is at end of a line ending with `{`, **When** user presses Enter, **Then** the new line is indented one level deeper.
2. **Given** auto-indentation is enabled and cursor is on a normally indented line, **When** user presses `o`, **Then** the new line receives smart indentation based on context.
3. **Given** auto-indentation is disabled, **When** user presses Enter, **Then** the new line starts at column 0 with no indentation.

---

### Edge Cases

- What happens when pasting a single line? No reindentation (same as multi-line — paste is always verbatim).
- What happens when `=` (reindent) command is used in visual mode? This feature is about removing *automatic* reindentation. If a manual reindent command exists, it should be evaluated for removal or retention.
- What happens when pasting into an empty buffer? Pasted content appears exactly as-is.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The editor MUST NOT modify indentation of pasted content after any paste operation (`p`, `P`, visual paste).
- **FR-002**: The editor MUST NOT reindent lines when exiting insert mode after a multi-line session.
- **FR-003**: The editor MUST NOT fire debounced reindentation events during insert mode.
- **FR-004**: The editor MUST continue to provide smart auto-indentation when creating new lines (Enter, `o`, `O`) when auto-indentation is enabled.
- **FR-005**: The editor MUST respect the existing `AutoIndentation` configuration toggle for new-line indentation behavior.
- **FR-006**: The `ReindentLines` function and related reindentation infrastructure MUST be removed from the codebase since no caller will remain.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Pasted text is character-for-character identical to clipboard content in 100% of paste operations.
- **SC-002**: Indentation-related code is reduced by at least 50% (measured by lines removed from indentation module).
- **SC-003**: All existing editor tests pass without modification (excluding any tests specifically testing removed reindentation behavior).
- **SC-004**: Auto-indentation on new lines continues to work correctly for all supported file types.

## Assumptions

- The `smartIndentForNewLine` function (used by Enter/o/O) is preserved since it handles new-line auto-indentation, which is the desired "auto-indentation on/off" behavior.
- Helper functions (`getIndentLevel`, `makeIndent`, `GetPrevNonEmptyLineIndent`, `countRune`) are preserved only if still needed by `smartIndentForNewLine`. Those only needed by `ReindentLines` are removed.
- The `ReindentEvent` custom event type and its handler in the main loop are removed since they solely served debounced reindentation.
- The F10 quit handler's call to `endInsertSession` is replaced with a simpler path that doesn't reindent.
