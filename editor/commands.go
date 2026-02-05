package editor

import "unicode"

// MoveToLineStart moves cursor to the beginning of the line
func (b *Buffer) MoveToLineStart() {
	b.CursorCol = 0
}

// MoveToLineEnd moves cursor to the end of the line
func (b *Buffer) MoveToLineEnd() {
	b.CursorCol = len(b.Lines[b.CursorRow])
}

// MoveUpN moves the cursor up n lines
func (b *Buffer) MoveUpN(n int) {
	for i := 0; i < n && b.CursorRow > 0; i++ {
		b.CursorRow--
	}
	b.clampCursorCol()
}

// MoveDownN moves the cursor down n lines
func (b *Buffer) MoveDownN(n int) {
	for i := 0; i < n && b.CursorRow < len(b.Lines)-1; i++ {
		b.CursorRow++
	}
	b.clampCursorCol()
}

// MoveLeftN moves the cursor left n characters
func (b *Buffer) MoveLeftN(n int) {
	for i := 0; i < n; i++ {
		b.MoveLeft()
	}
}

// MoveRightN moves the cursor right n characters
func (b *Buffer) MoveRightN(n int) {
	for i := 0; i < n; i++ {
		b.MoveRight()
	}
}

// GotoLine moves cursor to the specified line number (1-based)
func (b *Buffer) GotoLine(lineNum int) {
	if lineNum < 1 {
		lineNum = 1
	}
	if lineNum > len(b.Lines) {
		lineNum = len(b.Lines)
	}
	b.CursorRow = lineNum - 1
	b.CursorCol = 0
	b.clampCursorCol()
}

// FindCharForward finds the next occurrence of ch on the current line
// Returns true if found
func (b *Buffer) FindCharForward(ch rune) bool {
	line := b.Lines[b.CursorRow]
	for i := b.CursorCol + 1; i < len(line); i++ {
		if line[i] == ch {
			b.CursorCol = i
			return true
		}
	}
	return false
}

// FindCharBackward finds the previous occurrence of ch on the current line
// Returns true if found
func (b *Buffer) FindCharBackward(ch rune) bool {
	line := b.Lines[b.CursorRow]
	for i := b.CursorCol - 1; i >= 0; i-- {
		if line[i] == ch {
			b.CursorCol = i
			return true
		}
	}
	return false
}

// PageDown moves cursor down by half-page (like Ctrl+d in Vim)
// Uses (height-5)/2 to match Vim's scroll which excludes status, command line, and other UI
func (b *Buffer) PageDown(screenHeight int) {
	n := (screenHeight - 5) / 2
	if n < 1 {
		n = 1
	}
	b.MoveDownN(n)
}

// PageUp moves cursor up by half-page (like Ctrl+u in Vim)
// Uses (height-5)/2 to match Vim's scroll which excludes status, command line, and other UI
func (b *Buffer) PageUp(screenHeight int) {
	n := (screenHeight - 5) / 2
	if n < 1 {
		n = 1
	}
	b.MoveUpN(n)
}

// Selection support for Visual mode

// StartSelection marks the current cursor position as selection start
func (b *Buffer) StartSelection() {
	b.SelectionActive = true
	b.SelectionStartRow = b.CursorRow
	b.SelectionStartCol = b.CursorCol
}

// ClearSelection deactivates selection
func (b *Buffer) ClearSelection() {
	b.SelectionActive = false
}

// GetSelection returns the normalized selection bounds (start always before end)
// Returns startRow, startCol, endRow, endCol
func (b *Buffer) GetSelection() (int, int, int, int) {
	if !b.SelectionActive {
		return 0, 0, 0, 0
	}

	startRow, startCol := b.SelectionStartRow, b.SelectionStartCol
	endRow, endCol := b.CursorRow, b.CursorCol

	// Normalize: ensure start is before end
	if startRow > endRow || (startRow == endRow && startCol > endCol) {
		startRow, endRow = endRow, startRow
		startCol, endCol = endCol, startCol
	}

	return startRow, startCol, endRow, endCol
}

// IsInSelection returns true if the given position is within the selection
func (b *Buffer) IsInSelection(row, col int) bool {
	if !b.SelectionActive {
		return false
	}

	startRow, startCol, endRow, endCol := b.GetSelection()

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

// isWordChar returns true if r is a word character (letter, digit, or underscore)
func isWordChar(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r) || r == '_'
}

// MoveToNextWord moves cursor to the start of the next word (Vim 'w' command)
func (b *Buffer) MoveToNextWord() {
	line := b.Lines[b.CursorRow]
	col := b.CursorCol

	// If at end of line, move to next line
	if col >= len(line) {
		if b.CursorRow < len(b.Lines)-1 {
			b.CursorRow++
			b.CursorCol = 0
			// Skip leading whitespace on new line
			line = b.Lines[b.CursorRow]
			for b.CursorCol < len(line) && unicode.IsSpace(line[b.CursorCol]) {
				b.CursorCol++
			}
		}
		return
	}

	// Determine what type of character we're on
	onWord := isWordChar(line[col])
	onPunct := !onWord && !unicode.IsSpace(line[col])

	// Skip current word/punctuation sequence
	if onWord {
		for col < len(line) && isWordChar(line[col]) {
			col++
		}
	} else if onPunct {
		for col < len(line) && !isWordChar(line[col]) && !unicode.IsSpace(line[col]) {
			col++
		}
	}

	// Skip whitespace
	for col < len(line) && unicode.IsSpace(line[col]) {
		col++
	}

	// If we hit end of line, move to next line
	if col >= len(line) {
		if b.CursorRow < len(b.Lines)-1 {
			b.CursorRow++
			b.CursorCol = 0
			line = b.Lines[b.CursorRow]
			// Skip leading whitespace on new line
			for b.CursorCol < len(line) && unicode.IsSpace(line[b.CursorCol]) {
				b.CursorCol++
			}
		} else {
			b.CursorCol = len(line)
		}
		return
	}

	b.CursorCol = col
}

// MoveToPrevWord moves cursor to the start of the previous word (Vim 'b' command)
func (b *Buffer) MoveToPrevWord() {
	line := b.Lines[b.CursorRow]
	col := b.CursorCol

	// If at start of line, move to previous line
	if col == 0 {
		if b.CursorRow > 0 {
			b.CursorRow--
			b.CursorCol = len(b.Lines[b.CursorRow])
			b.MoveToPrevWord()
		}
		return
	}

	// Move back one position first
	col--

	// Skip whitespace going backward
	for col > 0 && unicode.IsSpace(line[col]) {
		col--
	}

	// If we hit start of line after skipping whitespace
	if col == 0 && unicode.IsSpace(line[col]) {
		if b.CursorRow > 0 {
			b.CursorRow--
			b.CursorCol = len(b.Lines[b.CursorRow])
			b.MoveToPrevWord()
		} else {
			b.CursorCol = 0
		}
		return
	}

	// Determine what type of character we're on
	onWord := isWordChar(line[col])

	// Skip backward through current word/punctuation sequence
	if onWord {
		for col > 0 && isWordChar(line[col-1]) {
			col--
		}
	} else {
		for col > 0 && !isWordChar(line[col-1]) && !unicode.IsSpace(line[col-1]) {
			col--
		}
	}

	b.CursorCol = col
}
