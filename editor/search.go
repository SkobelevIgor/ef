package editor

import "strings"

// SearchMatch represents a single occurrence of the search query in the buffer
type SearchMatch struct {
	Row    int // Line number (0-indexed) where match starts
	Col    int // Column position (0-indexed) where match starts
	Length int // Number of characters in the match
}

// SearchState tracks the active search session
type SearchState struct {
	Active        bool          // Whether search mode is currently active
	Query         string        // Current search query text
	Confirmed     bool          // Whether Enter has been pressed to confirm search
	CursorZeroRow int           // Row position before entering search mode
	CursorZeroCol int           // Column position before entering search mode
	Matches       []SearchMatch // All matches found for current query
	CurrentIndex  int           // Index of currently highlighted match (-1 if none)
	IsReplaceMode bool          // Whether in find-and-replace sub-mode
	ReplaceText   string        // Replacement text (when IsReplaceMode is true)
	NoMatches     bool          // Flag indicating query returned no results
}

// NewSearchState creates a new search state with cursor position saved as CursorZero
func NewSearchState(cursorRow, cursorCol int) *SearchState {
	return &SearchState{
		Active:        true,
		CursorZeroRow: cursorRow,
		CursorZeroCol: cursorCol,
		CurrentIndex:  -1,
	}
}

// ParseQuery parses the search input for replace syntax
// Returns: query, replacement, isReplace
// Pattern: replace::<search>::<replacement>
func ParseQuery(input string) (query, replacement string, isReplace bool) {
	if strings.HasPrefix(input, "replace::") {
		rest := input[9:] // Skip "replace::"
		parts := strings.SplitN(rest, "::", 2)
		if len(parts) == 2 {
			return parts[0], parts[1], true
		}
	}
	// Not a valid replace pattern, treat as literal search
	return input, "", false
}

// FindFirstMatchAfterCursor returns the index of the first match after the given cursor position
// Returns -1 if no matches exist or all matches are before cursor
func FindFirstMatchAfterCursor(matches []SearchMatch, cursorRow, cursorCol int) int {
	if len(matches) == 0 {
		return -1
	}

	for i, match := range matches {
		// Match is after cursor if it's on a later row,
		// or same row but at or after cursor column
		if match.Row > cursorRow || (match.Row == cursorRow && match.Col >= cursorCol) {
			return i
		}
	}

	// No match found after cursor, wrap to first match
	return 0
}
