package editor

// SearchMatch represents a single occurrence of the search query in the buffer
type SearchMatch struct {
	Row    int // Line number (0-indexed) where match starts
	Col    int // Column position (0-indexed) where match starts
	Length int // Number of characters in the match
}

// SearchState tracks the active search session (legacy — kept for migration)
type SearchState struct {
	Active        bool
	Query         string
	Confirmed     bool
	CursorZeroRow int
	CursorZeroCol int
	Matches       []SearchMatch
	CurrentIndex  int
	IsReplaceMode bool
	ReplaceText   string
	NoMatches     bool
}

// NewSearchState creates a new search state (legacy)
func NewSearchState(cursorRow, cursorCol int) *SearchState {
	return &SearchState{
		Active:        true,
		CursorZeroRow: cursorRow,
		CursorZeroCol: cursorCol,
		CurrentIndex:  -1,
	}
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
