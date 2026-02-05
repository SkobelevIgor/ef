package editor

// InputState tracks multi-key sequence state
type InputState struct {
	// Numeric prefix accumulator
	Count    int
	HasCount bool

	// Pending character find
	PendingFindForward  bool
	PendingFindBackward bool

	// Pending goto line (after typing :)
	PendingGotoLine bool
	GotoLineBuffer  string

	// Last find character for ; and , repeat
	LastFindChar    rune
	LastFindForward bool
	HasLastFind     bool

	// Pending operator for commands like dd, yy, dw, etc.
	PendingOperator rune

	// Search mode state
	Search *SearchState

	// Autocomplete state (nil when not active)
	Autocomplete *AutocompleteState

	// Pending mark operations
	PendingMark       bool // True when waiting for mark identifier after 'm'
	PendingJumpToMark bool // True when waiting for mark identifier after '`'
}

// NewInputState creates a new input state
func NewInputState() *InputState {
	return &InputState{}
}

// Reset clears all pending state except last find memory
func (s *InputState) Reset() {
	s.Count = 0
	s.HasCount = false
	s.PendingFindForward = false
	s.PendingFindBackward = false
	s.PendingGotoLine = false
	s.GotoLineBuffer = ""
	s.PendingOperator = 0
	s.PendingMark = false
	s.PendingJumpToMark = false
}

// GetCount returns the count, defaulting to 1 if not set
func (s *InputState) GetCount() int {
	if s.HasCount {
		return s.Count
	}
	return 1
}

// AddDigit appends a digit to the count
func (s *InputState) AddDigit(d int) {
	s.Count = s.Count*10 + d
	s.HasCount = true
}

// HasPending returns true if any pending state is active
func (s *InputState) HasPending() bool {
	return s.PendingFindForward || s.PendingFindBackward || s.PendingGotoLine || s.PendingOperator != 0 || s.PendingMark || s.PendingJumpToMark
}

// PendingString returns a string representation of pending input
func (s *InputState) PendingString() string {
	if s.PendingFindForward {
		return "f_"
	}
	if s.PendingFindBackward {
		return "F_"
	}
	if s.PendingGotoLine {
		return ":" + s.GotoLineBuffer
	}
	if s.PendingOperator != 0 {
		return string(s.PendingOperator)
	}
	if s.PendingMark {
		return "m_"
	}
	if s.PendingJumpToMark {
		return "`_"
	}
	return ""
}

// SaveLastFind stores the last find character and direction
func (s *InputState) SaveLastFind(ch rune, forward bool) {
	s.LastFindChar = ch
	s.LastFindForward = forward
	s.HasLastFind = true
}
