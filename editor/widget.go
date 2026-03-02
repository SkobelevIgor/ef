package editor

// WidgetKind identifies which widget is active
type WidgetKind int

const (
	WidgetNone        WidgetKind = iota
	WidgetSearch                         // F4
	WidgetFindReplace                    // F3
)

// FocusTarget identifies which part of the widget has focus
type FocusTarget int

const (
	FocusFindBar    FocusTarget = iota
	FocusReplaceBar
	FocusEditor
)

// WidgetSession holds state for a single search or find-replace session
type WidgetSession struct {
	Query        string
	ReplaceText  string
	Matches      []SearchMatch
	CurrentIndex int
	NoMatches    bool
	CursorRow    int // editor cursor when this session was last active
	CursorCol    int
}

// NewWidgetSession creates a session anchored at the given cursor position
func NewWidgetSession(row, col int) *WidgetSession {
	return &WidgetSession{
		CurrentIndex: -1,
		CursorRow:    row,
		CursorCol:    col,
	}
}

// WidgetState holds the full widget state shared across sessions
type WidgetState struct {
	Active             bool
	Kind               WidgetKind
	Focus              FocusTarget
	AnchorRow          int
	AnchorCol          int
	SearchSession      *WidgetSession // F4 state
	FindReplaceSession *WidgetSession // F3 state
}

// NewWidgetState creates a new widget for the given kind
func NewWidgetState(kind WidgetKind, anchorRow, anchorCol int) *WidgetState {
	w := &WidgetState{
		Active:    true,
		Kind:      kind,
		Focus:     FocusFindBar,
		AnchorRow: anchorRow,
		AnchorCol: anchorCol,
	}
	switch kind {
	case WidgetSearch:
		w.SearchSession = NewWidgetSession(anchorRow, anchorCol)
	case WidgetFindReplace:
		w.FindReplaceSession = NewWidgetSession(anchorRow, anchorCol)
	}
	return w
}

// CurrentSession returns the session for the currently active kind
func (w *WidgetState) CurrentSession() *WidgetSession {
	switch w.Kind {
	case WidgetSearch:
		return w.SearchSession
	case WidgetFindReplace:
		return w.FindReplaceSession
	default:
		return nil
	}
}

// CalculateBarRows computes how many screen rows a bar needs
// given the text content and screen width
func CalculateBarRows(text string, width int) int {
	if width <= 0 {
		return 1
	}
	textLen := len([]rune(text))
	if textLen <= width {
		return 1
	}
	return (textLen + width - 1) / width
}

// BarHeight returns the total screen rows consumed by the widget
func (w *WidgetState) BarHeight(screenWidth int) int {
	switch w.Kind {
	case WidgetSearch:
		if w.SearchSession == nil {
			return 1
		}
		return CalculateBarRows(w.SearchSession.Query, screenWidth)
	case WidgetFindReplace:
		if w.FindReplaceSession == nil {
			return 2
		}
		findRows := CalculateBarRows(w.FindReplaceSession.Query, screenWidth)
		replaceRows := CalculateBarRows(w.FindReplaceSession.ReplaceText, screenWidth)
		return findRows + replaceRows
	default:
		return 0
	}
}
