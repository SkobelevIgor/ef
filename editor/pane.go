package editor

// Pane represents a visual view of a buffer with independent navigation state.
// Multiple panes can reference the same buffer (shared buffer model).
type Pane struct {
	Buffer *Buffer // Reference to shared buffer

	// Independent cursor position (each pane has its own cursor)
	CursorRow int
	CursorCol int

	// Independent scroll position
	ScrollOffset int // First line displayed at top of screen

	// Independent selection state for Visual mode
	SelectionActive   bool
	SelectionStartRow int
	SelectionStartCol int
}

// NewPane creates a new pane viewing the given buffer
func NewPane(buf *Buffer) *Pane {
	return &Pane{
		Buffer:    buf,
		CursorRow: 0,
		CursorCol: 0,
	}
}

// NewPaneAtLine creates a new pane viewing the given buffer, positioned at a specific line
func NewPaneAtLine(buf *Buffer, line int) *Pane {
	p := NewPane(buf)
	if line > 0 {
		p.GotoLine(line)
	}
	return p
}

// GotoLine moves the cursor to a specific line (1-based for user, converts to 0-based)
func (p *Pane) GotoLine(line int) {
	if line < 1 {
		line = 1
	}
	if line > len(p.Buffer.Lines) {
		line = len(p.Buffer.Lines)
	}
	p.CursorRow = line - 1
	p.CursorCol = 0
	p.clampCursor()
}

// clampCursor ensures the cursor position is within valid bounds
func (p *Pane) clampCursor() {
	// Clamp row to buffer bounds
	if p.CursorRow < 0 {
		p.CursorRow = 0
	}
	if p.CursorRow >= len(p.Buffer.Lines) {
		p.CursorRow = len(p.Buffer.Lines) - 1
	}
	if p.CursorRow < 0 {
		p.CursorRow = 0 // Handle empty buffer edge case
	}

	// Clamp column to line length
	p.clampCursorCol()
}

// clampCursorCol ensures the cursor column is within the current line bounds
func (p *Pane) clampCursorCol() {
	if len(p.Buffer.Lines) == 0 {
		p.CursorCol = 0
		return
	}
	lineLen := len(p.Buffer.Lines[p.CursorRow])
	if p.CursorCol > lineLen {
		p.CursorCol = lineLen
	}
	if p.CursorCol < 0 {
		p.CursorCol = 0
	}
}

// AdjustCursorForEdit adjusts the cursor position when lines are added or removed
// editRow is the row where the edit occurred
// linesDelta is positive for insertions, negative for deletions
func (p *Pane) AdjustCursorForEdit(editRow, linesDelta int) {
	if linesDelta == 0 {
		return
	}

	if linesDelta > 0 {
		// Lines inserted: shift cursor down if it's at or after the edit
		if p.CursorRow >= editRow {
			p.CursorRow += linesDelta
		}
	} else {
		// Lines deleted
		deletedStart := editRow
		deletedEnd := editRow + (-linesDelta) - 1

		if p.CursorRow > deletedEnd {
			// Cursor is after deleted range: shift up
			p.CursorRow += linesDelta
		} else if p.CursorRow >= deletedStart {
			// Cursor is within deleted range: move to deletion point
			p.CursorRow = deletedStart
		}
		// If cursor is before deletion, no change needed
	}

	// Always clamp after adjustment
	p.clampCursor()
}

// AdjustScroll adjusts the scroll offset to keep the cursor visible
// with a margin from the top and bottom edges
// textWidth is the available width for text (excluding line numbers)
// screenHeight is the number of visible lines
func (p *Pane) AdjustScroll(textWidth, screenHeight int) {
	if textWidth < 1 {
		textWidth = 1
	}

	// Scroll margin - lines to keep visible above/below cursor
	const scrollMargin = ScrollMargin

	// Adjust margin if screen is too small
	margin := scrollMargin
	if screenHeight < margin*2+1 {
		margin = (screenHeight - 1) / 2
	}
	if margin < 0 {
		margin = 0
	}

	// Calculate which wrapped row the cursor is on within its line
	cursorWrapRow := 0
	if p.CursorCol > 0 && textWidth > 0 {
		visualCol := p.Buffer.GetVisualColumn(p.Buffer.Lines[p.CursorRow], p.CursorCol)
		cursorWrapRow = visualCol / textWidth
	}

	// Calculate screen rows from ScrollOffset to cursor (top margin check)
	screenRowsFromTop := 0
	for lineIdx := p.ScrollOffset; lineIdx < p.CursorRow && lineIdx < len(p.Buffer.Lines); lineIdx++ {
		visualWidth := p.Buffer.GetVisualLineWidth(p.Buffer.Lines[lineIdx])
		if visualWidth == 0 {
			screenRowsFromTop++
		} else {
			screenRowsFromTop += (visualWidth + textWidth - 1) / textWidth
		}
	}
	screenRowsFromTop += cursorWrapRow

	// If cursor is too close to top, scroll up
	if screenRowsFromTop < margin && p.ScrollOffset > 0 {
		for screenRowsFromTop < margin && p.ScrollOffset > 0 {
			p.ScrollOffset--
			visualWidth := p.Buffer.GetVisualLineWidth(p.Buffer.Lines[p.ScrollOffset])
			if visualWidth == 0 {
				screenRowsFromTop++
			} else {
				screenRowsFromTop += (visualWidth + textWidth - 1) / textWidth
			}
		}
	}

	// Calculate total screen rows used including cursor position
	screenRowsUsed := screenRowsFromTop + 1

	// If cursor is too close to bottom, scroll down
	if screenRowsUsed > screenHeight-margin {
		excess := screenRowsUsed - (screenHeight - margin)
		for excess > 0 && p.ScrollOffset < p.CursorRow {
			visualWidth := p.Buffer.GetVisualLineWidth(p.Buffer.Lines[p.ScrollOffset])
			lineRows := 1
			if visualWidth > 0 {
				lineRows = (visualWidth + textWidth - 1) / textWidth
			}
			if lineRows <= excess {
				excess -= lineRows
				p.ScrollOffset++
			} else {
				break
			}
		}
		if excess > 0 {
			p.ScrollOffset++
		}
	}

	// Ensure cursor is never above ScrollOffset
	if p.CursorRow < p.ScrollOffset {
		p.ScrollOffset = p.CursorRow - margin
		if p.ScrollOffset < 0 {
			p.ScrollOffset = 0
		}
	}
}

// StartSelection begins a visual selection at the current cursor position
func (p *Pane) StartSelection() {
	p.SelectionActive = true
	p.SelectionStartRow = p.CursorRow
	p.SelectionStartCol = p.CursorCol
}

// ClearSelection ends the visual selection
func (p *Pane) ClearSelection() {
	p.SelectionActive = false
}

// GetSelection returns the selection bounds normalized (start before end)
func (p *Pane) GetSelection() (startRow, startCol, endRow, endCol int) {
	return NormalizeRange(p.SelectionStartRow, p.SelectionStartCol, p.CursorRow, p.CursorCol)
}

// IsInSelection returns true if the given position is within the selection
func (p *Pane) IsInSelection(row, col int) bool {
	if !p.SelectionActive {
		return false
	}

	startRow, startCol, endRow, endCol := p.GetSelection()

	// Before start
	if row < startRow || (row == startRow && col < startCol) {
		return false
	}

	// After end
	if row > endRow || (row == endRow && col > endCol) {
		return false
	}

	return true
}

// Movement methods that delegate to buffer but update pane's cursor

// MoveUp moves the cursor up one line
func (p *Pane) MoveUp() {
	if p.CursorRow > 0 {
		p.CursorRow--
		p.clampCursorCol()
	}
}

// MoveDown moves the cursor down one line
func (p *Pane) MoveDown() {
	if p.CursorRow < len(p.Buffer.Lines)-1 {
		p.CursorRow++
		p.clampCursorCol()
	}
}

// MoveLeft moves the cursor left one character
func (p *Pane) MoveLeft() {
	if p.CursorCol > 0 {
		p.CursorCol--
	} else if p.CursorRow > 0 {
		p.CursorRow--
		p.CursorCol = len(p.Buffer.Lines[p.CursorRow])
	}
}

// MoveRight moves the cursor right one character
func (p *Pane) MoveRight() {
	if p.CursorCol < len(p.Buffer.Lines[p.CursorRow]) {
		p.CursorCol++
	} else if p.CursorRow < len(p.Buffer.Lines)-1 {
		p.CursorRow++
		p.CursorCol = 0
	}
}

// MoveUpN moves the cursor up N lines
func (p *Pane) MoveUpN(n int) {
	for i := 0; i < n && p.CursorRow > 0; i++ {
		p.CursorRow--
	}
	p.clampCursorCol()
}

// MoveDownN moves the cursor down N lines
func (p *Pane) MoveDownN(n int) {
	for i := 0; i < n && p.CursorRow < len(p.Buffer.Lines)-1; i++ {
		p.CursorRow++
	}
	p.clampCursorCol()
}

// MoveLeftN moves the cursor left N characters
func (p *Pane) MoveLeftN(n int) {
	for i := 0; i < n; i++ {
		p.MoveLeft()
	}
}

// MoveRightN moves the cursor right N characters
func (p *Pane) MoveRightN(n int) {
	for i := 0; i < n; i++ {
		p.MoveRight()
	}
}

// MoveToLineStart moves the cursor to the beginning of the line
func (p *Pane) MoveToLineStart() {
	p.CursorCol = 0
}

// MoveToLineEnd moves the cursor to the end of the line
func (p *Pane) MoveToLineEnd() {
	p.CursorCol = len(p.Buffer.Lines[p.CursorRow])
}

// PageDown moves the cursor down by height lines
func (p *Pane) PageDown(height int) {
	p.CursorRow += height / 2
	if p.CursorRow >= len(p.Buffer.Lines) {
		p.CursorRow = len(p.Buffer.Lines) - 1
	}
	p.clampCursorCol()
}

// PageUp moves the cursor up by height lines
func (p *Pane) PageUp(height int) {
	p.CursorRow -= height / 2
	if p.CursorRow < 0 {
		p.CursorRow = 0
	}
	p.clampCursorCol()
}

// MoveToNextWord moves the cursor to the start of the next word
func (p *Pane) MoveToNextWord() {
	line := p.Buffer.Lines[p.CursorRow]

	if p.CursorCol < len(line) {
		ch := line[p.CursorCol]
		if isWordChar(ch) {
			// Skip word characters
			for p.CursorCol < len(line) && isWordChar(line[p.CursorCol]) {
				p.CursorCol++
			}
		} else if !isWhitespace(ch) {
			// Skip punctuation characters
			for p.CursorCol < len(line) && !isWordChar(line[p.CursorCol]) && !isWhitespace(line[p.CursorCol]) {
				p.CursorCol++
			}
		}
	}

	// Skip whitespace
	for p.CursorCol < len(line) && isWhitespace(line[p.CursorCol]) {
		p.CursorCol++
	}

	// If at end of line, move to start of next line
	if p.CursorCol >= len(line) && p.CursorRow < len(p.Buffer.Lines)-1 {
		p.CursorRow++
		p.CursorCol = 0
		// Skip leading whitespace on new line
		line = p.Buffer.Lines[p.CursorRow]
		for p.CursorCol < len(line) && isWhitespace(line[p.CursorCol]) {
			p.CursorCol++
		}
	}
}

// MoveToPrevWord moves the cursor to the start of the previous word
func (p *Pane) MoveToPrevWord() {
	// If at start of line, move to end of previous line
	if p.CursorCol == 0 && p.CursorRow > 0 {
		p.CursorRow--
		p.CursorCol = len(p.Buffer.Lines[p.CursorRow])
	}

	line := p.Buffer.Lines[p.CursorRow]

	// Move back one if not at start
	if p.CursorCol > 0 {
		p.CursorCol--
	}

	// Skip whitespace backwards
	for p.CursorCol > 0 && isWhitespace(line[p.CursorCol]) {
		p.CursorCol--
	}

	if p.CursorCol < len(line) {
		ch := line[p.CursorCol]
		if isWordChar(ch) {
			// Find start of word
			for p.CursorCol > 0 && isWordChar(line[p.CursorCol-1]) {
				p.CursorCol--
			}
		} else if !isWhitespace(ch) {
			// Find start of punctuation group
			for p.CursorCol > 0 && !isWordChar(line[p.CursorCol-1]) && !isWhitespace(line[p.CursorCol-1]) {
				p.CursorCol--
			}
		}
	}
}

// FindCharForward finds the next occurrence of ch on the current line
func (p *Pane) FindCharForward(ch rune) {
	line := p.Buffer.Lines[p.CursorRow]
	for i := p.CursorCol + 1; i < len(line); i++ {
		if line[i] == ch {
			p.CursorCol = i
			return
		}
	}
}

// FindCharBackward finds the previous occurrence of ch on the current line
func (p *Pane) FindCharBackward(ch rune) {
	line := p.Buffer.Lines[p.CursorRow]
	for i := p.CursorCol - 1; i >= 0; i-- {
		if line[i] == ch {
			p.CursorCol = i
			return
		}
	}
}
