# Research: Autocomplete Implementation

**Feature**: 002-autocomplete
**Date**: 2026-02-05

## Research Topics

### 1. Shallow (Subsequence) Matching Algorithm

**Decision**: Use a simple linear scan with character-by-character subsequence check.

**Rationale**:
- Subsequence matching is O(n) per word where n is word length
- No need for complex fuzzy matching libraries
- Go's built-in string/rune operations are sufficient
- Performance is acceptable for files with thousands of words (typical editor use case)

**Algorithm**:
```
For each character in query (case-insensitive):
  Find it in the remaining portion of candidate word
  If not found: no match
  If found: advance position in candidate
If all query chars found in order: match
```

**Alternatives Considered**:
- Levenshtein distance: Rejected - too computationally expensive, not what spec requires
- Regex matching: Rejected - overkill, harder to score relevance
- Trie/prefix tree: Rejected - only helps prefix matching, not subsequence

### 2. Word Extraction from Buffer

**Decision**: Extract on-demand using Unicode-aware tokenization.

**Rationale**:
- Go's `unicode` package provides character classification
- Split on whitespace + common delimiters: `()[]{}.,;:!?<>=+-*/%&|^~`
- Keep underscores and alphanumerics as part of words (common in code)
- Use `map[string]struct{}` for deduplication

**Implementation Pattern**:
```go
func ExtractWords(lines [][]rune) []string {
    seen := make(map[string]struct{})
    var words []string
    for _, line := range lines {
        // tokenize line, add unique words
    }
    return words
}
```

**Alternatives Considered**:
- Pre-indexed word cache: Rejected - adds complexity, must sync on every edit
- Regex-based tokenizer: Rejected - slower than simple rune iteration

### 3. Dropdown Rendering with tcell

**Decision**: Render dropdown as overlay after main content, using tcell's SetContent.

**Rationale**:
- tcell allows writing to any screen position
- Draw dropdown last so it overlays buffer content
- Use distinct background color for visibility
- Border characters from Unicode box-drawing set

**Positioning Logic**:
```
cursorScreenY = buffer cursor row - scroll offset + content start
spaceBelow = screenHeight - cursorScreenY - 1
spaceAbove = cursorScreenY

if spaceBelow >= dropdownHeight:
    position below cursor
else if spaceAbove >= dropdownHeight:
    position above cursor
else:
    truncate to fit available space
```

**Alternatives Considered**:
- Separate overlay buffer: Rejected - tcell doesn't have native overlay support
- Popup window: Rejected - terminal doesn't support true popups

### 4. Relevance Scoring

**Decision**: Three-tier scoring system with stable sort.

**Scoring Algorithm**:
```
score = 0
if word starts with query (prefix match):
    score += 1000
score -= len(word)  // shorter words preferred
// alphabetical via stable sort
```

**Rationale**:
- Prefix matches are most intuitive for users
- Shorter completions save more keystrokes
- Alphabetical tiebreaker provides predictable ordering

**Alternatives Considered**:
- Frequency-based (word count in file): Rejected - adds tracking complexity
- Match position bonus: Rejected - diminishing returns vs. simplicity

### 5. Integration with Existing Input Handling

**Decision**: Add autocomplete check after each character insert in Insert mode.

**Integration Points**:
1. `InputState.Autocomplete` - holds state when dropdown visible
2. `handleInsertMode()` in editor.go - trigger on character input
3. Key interception order: Autocomplete keys checked before normal Insert handling

**Key Handling Priority** (when dropdown visible):
1. Tab → Accept suggestion
2. Ctrl+n → Next suggestion
3. Ctrl+p → Previous suggestion
4. Escape → Close dropdown
5. Other keys → Process normally, update suggestions

**Alternatives Considered**:
- Separate autocomplete mode: Rejected - Insert mode should remain primary
- Async suggestion generation: Rejected - sync is fast enough, simpler

## Resolved Clarifications

All technical unknowns from spec have been resolved through this research. No external dependencies or APIs required.

## Performance Considerations

- Word extraction: O(total characters in file)
- Matching: O(words × query length)
- Sorting: O(n log n) where n ≤ 10 (capped)
- Total expected: <10ms for typical files, well under 100ms target

## Next Steps

Proceed to Phase 1: Data Model and Implementation Guide.
