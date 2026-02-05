# Data Model: Autocomplete

**Feature**: 002-autocomplete
**Date**: 2026-02-05

## Entities

### AutocompleteState

Represents an active autocomplete session.

```go
type AutocompleteState struct {
    Active       bool          // Whether autocomplete dropdown is visible
    Prefix       string        // The typed characters triggering completion
    PrefixCol    int           // Column position where prefix starts
    Suggestions  []Suggestion  // Matching words, sorted by relevance
    SelectedIdx  int           // Currently highlighted suggestion (0-based)
    DropdownRow  int           // Screen row where dropdown starts
    DropdownCol  int           // Screen column where dropdown starts
    ShowAbove    bool          // True if dropdown appears above cursor
}
```

**Lifecycle**:
1. Created when user types 2+ characters at word boundary in Insert mode
2. Updated on each additional character (re-filter suggestions)
3. Destroyed on: Tab (accept), Escape (dismiss), cursor move, mode change

### Suggestion

Represents a single autocomplete candidate.

```go
type Suggestion struct {
    Word    string  // The complete word
    Score   int     // Relevance score (higher = better match)
}
```

**Scoring Rules** (from spec clarification):
- Prefix match: +1000 points
- Length penalty: -len(word) points
- Alphabetical: handled by stable sort

### InputState Extension

Add autocomplete field to existing InputState.

```go
type InputState struct {
    // ... existing fields ...

    // Autocomplete state (nil when not active)
    Autocomplete *AutocompleteState
}
```

## Functions

### Word Extraction

```go
// ExtractWords returns all unique words from buffer lines
// Words are tokenized by whitespace and delimiters
func ExtractWords(lines [][]rune, excludeRow, excludeCol int) []string
```

Parameters:
- `lines`: Buffer content
- `excludeRow`, `excludeCol`: Current cursor position (to exclude word being typed)

Returns: Deduplicated list of words

### Matching

```go
// MatchSubsequence checks if query chars appear in order within word
// Case-insensitive comparison
func MatchSubsequence(word, query string) bool

// ScoreSuggestion calculates relevance score for a match
func ScoreSuggestion(word, query string) int

// FindSuggestions returns sorted, limited suggestions for a prefix
func FindSuggestions(words []string, prefix string, maxResults int) []Suggestion
```

### State Management

```go
// NewAutocompleteState creates state for a new autocomplete session
func NewAutocompleteState(prefix string, prefixCol int, suggestions []Suggestion) *AutocompleteState

// UpdateSuggestions refreshes suggestions for new prefix
func (a *AutocompleteState) UpdateSuggestions(suggestions []Suggestion)

// Next moves selection to next suggestion (wrapping)
func (a *AutocompleteState) Next()

// Prev moves selection to previous suggestion (wrapping)
func (a *AutocompleteState) Prev()

// Selected returns the currently highlighted suggestion
func (a *AutocompleteState) Selected() *Suggestion
```

### Rendering

```go
// RenderAutocomplete draws the dropdown overlay
// Called from Screen.Render after main content
func (s *Screen) RenderAutocomplete(state *AutocompleteState, cursorX, cursorY, screenHeight int)
```

## State Transitions

```
[No Autocomplete]
    │
    ▼ (user types 2+ chars at word boundary in Insert mode)
    │
[Autocomplete Active]
    │
    ├──► Tab pressed ──────► Insert selected word ──► [No Autocomplete]
    │
    ├──► Escape pressed ───► Dismiss ──────────────► [No Autocomplete]
    │
    ├──► Cursor moved ─────► Dismiss ──────────────► [No Autocomplete]
    │
    ├──► Mode changed ─────► Dismiss ──────────────► [No Autocomplete]
    │
    ├──► Ctrl+n pressed ───► Select next ──────────► [Autocomplete Active]
    │
    ├──► Ctrl+p pressed ───► Select previous ──────► [Autocomplete Active]
    │
    ├──► Character typed ──► Update prefix & filter ► [Autocomplete Active]
    │                                                  (or dismiss if no matches)
    │
    └──► Backspace ────────► Shorten prefix ───────► [Autocomplete Active]
                                                      (or dismiss if prefix < 2)
```

## Validation Rules

1. **Prefix minimum**: Autocomplete only triggers with 2+ characters (FR-002)
2. **Max suggestions**: Limited to 10 items (FR-019)
3. **Mode restriction**: Only active in Insert mode (FR-001)
4. **Self-exclusion**: Current word being typed excluded from suggestions (FR-018)
5. **Uniqueness**: Each word appears only once in suggestions (FR-017)

## Relationships

```
Editor
  └── inputState: InputState
        └── Autocomplete: *AutocompleteState
              └── Suggestions: []Suggestion

Buffer
  └── Lines: [][]rune  ──(extracted)──► words for matching
```
