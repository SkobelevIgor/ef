# Feature Specification: Fix Paste Indentation

**Feature Branch**: `007-fix-paste-indent`
**Created**: 2026-02-21
**Status**: Draft
**Input**: User description: "Fix indentation for inserted code snippets. In any mode, when I insert some content in some place it should be formatted with correct indentation, according to config for a particular file type."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Paste Multi-Line Snippet with Broken Indentation (Priority: P1)

A user copies a multi-line code snippet from an external source (browser, another editor, chat) where the indentation is completely wrong — mixed tabs and spaces, inconsistent levels, or zero indentation. They paste it into a specific location in a file (e.g., inside a function body). After pasting, the editor immediately reformats the indentation of the entire pasted block so that:
- The first line of pasted content receives the correct indentation for the insertion point (e.g., 1 indent level inside a function body)
- Subsequent lines maintain their relative depth compared to the first pasted line
- All indentation uses the correct character (tabs or spaces) per the file type config

**Why this priority**: This is the core use case — pasting broken code and getting it auto-fixed. Without this, the feature has no value.

**Independent Test**: Paste a 5-line code snippet with zero indentation into a function body in a `.go` file (configured with tabs). Verify all lines receive correct tab-based indentation matching their nesting context.

**Acceptance Scenarios**:

1. **Given** a Go file with a function body at indent level 1, **When** the user pastes a 3-line snippet with zero indentation inside the function, **Then** all 3 lines receive indent level 1 (tabs), preserving relative depth between lines.
2. **Given** a Python file configured with 4-space indentation, **When** the user pastes code with mixed tabs and spaces, **Then** all pasted lines use spaces only, with correct indent levels relative to the insertion point.
3. **Given** a file with cursor at indent level 2, **When** the user pastes a block where the first line has 0 indent and the second line has 1 extra indent relative to the first, **Then** the first pasted line gets level 2 and the second gets level 3.

---

### User Story 2 - Paste Preserves Relative Structure (Priority: P1)

A user pastes a well-structured code block (e.g., an if-else with nested body) into a different indent context. The relative nesting within the pasted block must be preserved — only the base indent level shifts to match the insertion point.

**Why this priority**: Equally critical — destroying the internal structure of pasted code would make the feature harmful rather than helpful.

**Independent Test**: Paste a nested if/else block (3 indent levels deep internally) into a location at indent level 1. Verify the internal structure shifts uniformly while preserving relative depth.

**Acceptance Scenarios**:

1. **Given** a pasted block with lines at relative depths 0, 1, 2, 1, 0, **When** pasted at indent level 2, **Then** the resulting lines are at levels 2, 3, 4, 3, 2.
2. **Given** a pasted block where the minimum indent is 3 (e.g., copied from deeply nested code), **When** pasted at indent level 1, **Then** the block is normalized: the minimum becomes level 1, and all other lines shift accordingly.

---

### User Story 3 - Paste As-Is for Unconfigured File Types (Priority: P2)

When the user pastes content into a file whose type has no entry in `efconfig`, the editor pastes the content exactly as-is, without modifying any indentation.

**Why this priority**: Important for correctness — the editor should not guess when it has no config to rely on. But less common than the main paste scenario.

**Independent Test**: Open a `.xyz` file with no config entry, paste indented content, verify it appears exactly as copied.

**Acceptance Scenarios**:

1. **Given** a file with extension `.txt` and no efconfig entry for text files, **When** the user pastes content with arbitrary indentation, **Then** the content is inserted exactly as-is.
2. **Given** a file with a recognized extension and valid efconfig entry, **When** the user pastes content, **Then** indentation reformatting is applied.

---

### User Story 4 - Single-Line Paste (Priority: P2)

When the user pastes a single line of code, the indentation of that line should be adjusted to match the insertion point's expected indent level, following the same config rules.

**Why this priority**: Common operation but simpler than multi-line; builds on the same logic.

**Independent Test**: Paste a single line with 0 indent into a function body at indent level 2. Verify the line receives level 2 indentation.

**Acceptance Scenarios**:

1. **Given** cursor at indent level 2 in a configured file, **When** the user pastes a single line with no leading whitespace, **Then** the line receives indent level 2.
2. **Given** cursor at indent level 0 (top of file), **When** the user pastes a single line with 3 tabs of indentation, **Then** the line receives indent level 0 (stripped to match context).

---

### Edge Cases

- What happens when pasting into an empty file? The insertion point is at level 0; paste should normalize the block to start at level 0.
- What happens when pasting into an empty line inside a block? The indent level should be determined by the surrounding context (previous non-empty line's indent level).
- What happens when the pasted content contains blank lines? Blank lines within the pasted block should remain blank (no indentation added to empty lines).
- What happens when the pasted content has only whitespace lines? Treat as blank lines — insert as empty.
- What happens when pasting at the middle of existing text on a line? Indentation fixing applies only to the 2nd+ pasted lines. The first pasted line's content joins the existing text inline without modifying the current line's indentation.
- How does this interact with undo? A paste-then-reformat should be a single undoable operation.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST detect the indent level of the insertion point when a paste operation begins. The indent level is always derived from the previous non-empty line's indentation (ignoring any auto-generated whitespace on the current line).
- **FR-002**: System MUST normalize pasted content by determining the minimum indent level across all non-empty pasted lines and treating that as the block's "base indent."
- **FR-003**: System MUST reindent each pasted line so that: `new_indent = insertion_indent + (original_indent - base_indent)`, preserving relative depth within the block.
- **FR-004**: System MUST use the file type's configured indentation style (tabs vs spaces) and width (shift width) when generating the new indentation.
- **FR-005**: System MUST NOT modify indentation when: (a) the file type has no entry in efconfig, OR (b) the file type's `AutoIndentation` setting is disabled. In both cases, content is pasted as-is.
- **FR-006**: System MUST treat empty lines within pasted content as empty (no indentation characters added).
- **FR-007**: System MUST handle mixed indentation in the pasted source (e.g., some lines with tabs, others with spaces) by converting all indentation to the target file's configured style.
- **FR-008**: The entire paste-and-reformat operation MUST be a single undoable action.
- **FR-009**: System MUST apply indentation fixing for all paste/put operations: bracketed paste, timing-detected paste, AND vim-style put commands (`p`/`P` from yank register).
- **FR-010**: System MUST apply indentation fixing to the first line of pasted content, not just subsequent lines — but only when the cursor is at the beginning of a line or on an empty line. When pasting in the middle of existing text, the first pasted line joins inline and only the 2nd+ lines are reindented.

### Key Entities

- **Pasted Block**: A sequence of one or more lines inserted via a paste operation, with a computed base indent and per-line relative depths.
- **Insertion Context**: The indent level at the cursor position where paste occurs, derived from the current or surrounding lines.
- **File Type Config**: The per-extension indentation settings (tab vs space, shift width) from efconfig that determine how indent characters are generated.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of paste operations into configured file types produce correctly indented output matching the file's tab/space and width settings.
- **SC-002**: Relative indentation structure within a pasted block is always preserved — no lines shift relative to their siblings.
- **SC-003**: Paste into unconfigured file types produces byte-identical output to the clipboard content (no modification).
- **SC-004**: Users can undo a paste operation with a single undo action, restoring the buffer to its pre-paste state.
- **SC-005**: Paste indentation fixing works identically for bracketed paste, timing-detected paste, and vim-style `p`/`P` put commands.

## Clarifications

### Session 2026-02-21

- Q: Does paste indent fixing apply to vim `p`/`P` put commands or only OS clipboard paste? → A: Both OS clipboard paste AND vim `p`/`P` put commands.
- Q: When `AutoIndentation` is disabled for a configured file type, should paste indent fixing still apply? → A: No — paste indent fixing is gated by `AutoIndentation`; if disabled, paste as-is.
- Q: Should insertion context use the current line's auto-generated indent or re-derive from previous non-empty line? → A: Always re-derive from the previous non-empty line.

## Assumptions

- The "correct" indent level for the insertion point is always derived from the previous non-empty line's indentation (not the current line's auto-generated whitespace), without language-specific syntax analysis (no AST parsing).
- The editor does not need to fix indentation structure within the pasted block (e.g., it won't detect that a closing brace should be at a lower indent than the body) — it only shifts the entire block to match the insertion context while preserving internal relative indentation.
- This feature applies to all paste/put operations (OS clipboard paste and vim `p`/`P` put commands), not to typing or other forms of text insertion.
