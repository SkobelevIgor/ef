package editor

import (
	"bufio"
	"os"
	"strings"
	"time"
)

// Buffer represents the text content being edited
type Buffer struct {
	Lines        [][]rune
	CursorRow    int
	CursorCol    int
	ScrollOffset int // First line displayed at top of screen
	Filename     string
	Modified     bool
	LastModTime  time.Time // Last known modification time of the file

	// Selection support for Visual mode
	SelectionActive   bool
	SelectionStartRow int
	SelectionStartCol int

	// Syntax highlighting support
	FileType       string
	HighlightCache *HighlightCache

	// Filetype configuration
	Config FileTypeConfig
}

// NewBuffer creates a new buffer and loads the file if it exists
func NewBuffer(filename string) (*Buffer, error) {
	b := &Buffer{
		Filename: filename,
		Lines:    [][]rune{{}},
		FileType: "", // Will be detected by registry
		Config: FileTypeConfig{
			TabStop:         4,
			ShiftWidth:      4,
			AutoIndentation: false,
			ExpandTab:       false,
		},
	}

	if _, err := os.Stat(filename); err == nil {
		if err := b.Load(); err != nil {
			return nil, err
		}
	}

	return b, nil
}

// NewBufferWithRegistry creates a new buffer with filetype detection from a registry
func NewBufferWithRegistry(filename string, registry *FileTypeRegistry) (*Buffer, error) {
	b, err := NewBuffer(filename)
	if err != nil {
		return nil, err
	}

	if registry != nil {
		b.FileType = registry.DetectFileType(filename)
		b.Config = registry.GetConfig(b.FileType)
		highlighter := registry.GetHighlighter(b.FileType)
		if highlighter != nil {
			b.HighlightCache = NewHighlightCache(highlighter)
		}
	}

	return b, nil
}

// Load reads the file content into the buffer
func (b *Buffer) Load() error {
	file, err := os.Open(b.Filename)
	if err != nil {
		return err
	}
	defer file.Close()

	b.Lines = [][]rune{}
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		b.Lines = append(b.Lines, []rune(scanner.Text()))
	}

	if err := scanner.Err(); err != nil {
		return err
	}

	// Ensure at least one line exists
	if len(b.Lines) == 0 {
		b.Lines = [][]rune{{}}
	}

	// Record modification time
	if info, err := os.Stat(b.Filename); err == nil {
		b.LastModTime = info.ModTime()
	}

	b.Modified = false
	return nil
}

// Save writes the buffer content to the file
func (b *Buffer) Save() error {
	file, err := os.Create(b.Filename)
	if err != nil {
		return err
	}
	defer file.Close()

	writer := bufio.NewWriter(file)
	for i, line := range b.Lines {
		if _, err := writer.WriteString(string(line)); err != nil {
			return err
		}
		if i < len(b.Lines)-1 {
			if _, err := writer.WriteString("\n"); err != nil {
				return err
			}
		}
	}

	if err := writer.Flush(); err != nil {
		return err
	}

	// Record modification time after save
	if info, err := os.Stat(b.Filename); err == nil {
		b.LastModTime = info.ModTime()
	}

	b.Modified = false
	return nil
}

// InsertChar inserts a character at the cursor position
func (b *Buffer) InsertChar(ch rune) {
	line := b.Lines[b.CursorRow]
	newLine := make([]rune, len(line)+1)
	copy(newLine[:b.CursorCol], line[:b.CursorCol])
	newLine[b.CursorCol] = ch
	copy(newLine[b.CursorCol+1:], line[b.CursorCol:])
	b.Lines[b.CursorRow] = newLine
	b.CursorCol++
	b.Modified = true
}

// DeleteChar deletes the character before the cursor (backspace)
func (b *Buffer) DeleteChar() {
	if b.CursorCol > 0 {
		line := b.Lines[b.CursorRow]
		newLine := make([]rune, len(line)-1)
		copy(newLine[:b.CursorCol-1], line[:b.CursorCol-1])
		copy(newLine[b.CursorCol-1:], line[b.CursorCol:])
		b.Lines[b.CursorRow] = newLine
		b.CursorCol--
		b.Modified = true
	} else if b.CursorRow > 0 {
		// Join with previous line
		prevLine := b.Lines[b.CursorRow-1]
		currLine := b.Lines[b.CursorRow]
		b.CursorCol = len(prevLine)
		b.Lines[b.CursorRow-1] = append(prevLine, currLine...)
		b.Lines = append(b.Lines[:b.CursorRow], b.Lines[b.CursorRow+1:]...)
		b.CursorRow--
		b.Modified = true
	}
}

// DeleteCharForward deletes the character at the cursor (delete key)
func (b *Buffer) DeleteCharForward() {
	line := b.Lines[b.CursorRow]
	if b.CursorCol < len(line) {
		newLine := make([]rune, len(line)-1)
		copy(newLine[:b.CursorCol], line[:b.CursorCol])
		copy(newLine[b.CursorCol:], line[b.CursorCol+1:])
		b.Lines[b.CursorRow] = newLine
		b.Modified = true
	} else if b.CursorRow < len(b.Lines)-1 {
		// Join with next line
		nextLine := b.Lines[b.CursorRow+1]
		b.Lines[b.CursorRow] = append(line, nextLine...)
		b.Lines = append(b.Lines[:b.CursorRow+1], b.Lines[b.CursorRow+2:]...)
		b.Modified = true
	}
}

// InsertNewline splits the current line at the cursor position
func (b *Buffer) InsertNewline() {
	line := b.Lines[b.CursorRow]
	left := make([]rune, b.CursorCol)
	copy(left, line[:b.CursorCol])
	right := make([]rune, len(line)-b.CursorCol)
	copy(right, line[b.CursorCol:])

	b.Lines[b.CursorRow] = left

	// Insert new line after current
	newLines := make([][]rune, len(b.Lines)+1)
	copy(newLines[:b.CursorRow+1], b.Lines[:b.CursorRow+1])
	newLines[b.CursorRow+1] = right
	copy(newLines[b.CursorRow+2:], b.Lines[b.CursorRow+1:])
	b.Lines = newLines

	b.CursorRow++
	b.CursorCol = 0
	b.Modified = true
}

// InsertNewlineWithIndent splits the line and applies auto-indentation if enabled
func (b *Buffer) InsertNewlineWithIndent() {
	// Get indentation from current line before splitting
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[b.CursorRow])
	}

	// Perform the normal newline
	b.InsertNewline()

	// Apply indentation to the new line
	if len(indent) > 0 {
		line := b.Lines[b.CursorRow]
		newLine := make([]rune, len(indent)+len(line))
		copy(newLine, indent)
		copy(newLine[len(indent):], line)
		b.Lines[b.CursorRow] = newLine
		b.CursorCol = len(indent)
	}
}

// OpenLineBelow opens a new line below the current line with auto-indentation (o command)
func (b *Buffer) OpenLineBelow() {
	// Get indentation from current line
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[b.CursorRow])
	}

	// Create new line with indentation
	newLine := make([]rune, len(indent))
	copy(newLine, indent)

	// Insert new line after current
	b.InsertLineAfter(b.CursorRow, newLine)

	// Move cursor to new line, at end of indentation
	b.CursorRow++
	b.CursorCol = len(indent)
}

// OpenLineAbove opens a new line above the current line with auto-indentation (O command)
func (b *Buffer) OpenLineAbove() {
	// Get indentation from current line
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[b.CursorRow])
	}

	// Create new line with indentation
	newLine := make([]rune, len(indent))
	copy(newLine, indent)

	// Insert new line before current
	b.InsertLineBefore(b.CursorRow, newLine)

	// Cursor stays on the same row number (which is now the new line)
	b.CursorCol = len(indent)
}

// InsertTab inserts a tab character or spaces based on ExpandTab setting
func (b *Buffer) InsertTab() {
	if b.Config.ExpandTab {
		// Insert spaces instead of tab
		tabStop := b.Config.TabStop
		if tabStop <= 0 {
			tabStop = 4
		}
		// Calculate spaces needed to reach next tab stop
		// Use visual column (accounting for existing tabs) not character column
		visualCol := b.GetVisualColumn(b.Lines[b.CursorRow], b.CursorCol)
		spacesNeeded := tabStop - (visualCol % tabStop)
		for i := 0; i < spacesNeeded; i++ {
			b.InsertChar(' ')
		}
	} else {
		b.InsertChar('\t')
	}
}

// getLeadingWhitespace returns the leading whitespace from a line
func (b *Buffer) getLeadingWhitespace(line []rune) []rune {
	var ws []rune
	for _, ch := range line {
		if ch == ' ' || ch == '\t' {
			ws = append(ws, ch)
		} else {
			break
		}
	}
	return ws
}

// GetShiftWidth returns the shift width for indentation
func (b *Buffer) GetShiftWidth() int {
	if b.Config.ShiftWidth > 0 {
		return b.Config.ShiftWidth
	}
	return 4
}

// GetVisualColumn calculates the visual column position accounting for tab expansion
func (b *Buffer) GetVisualColumn(line []rune, charCol int) int {
	tabStop := b.Config.TabStop
	if tabStop <= 0 {
		tabStop = 4
	}
	visualCol := 0
	for i := 0; i < charCol && i < len(line); i++ {
		if line[i] == '\t' {
			visualCol += tabStop - (visualCol % tabStop)
		} else {
			visualCol++
		}
	}
	return visualCol
}

// GetVisualLineWidth calculates the visual width of a line accounting for tabs
func (b *Buffer) GetVisualLineWidth(line []rune) int {
	return b.GetVisualColumn(line, len(line))
}

// MoveUp moves the cursor up one line
func (b *Buffer) MoveUp() {
	if b.CursorRow > 0 {
		b.CursorRow--
		b.clampCursorCol()
	}
}

// MoveDown moves the cursor down one line
func (b *Buffer) MoveDown() {
	if b.CursorRow < len(b.Lines)-1 {
		b.CursorRow++
		b.clampCursorCol()
	}
}

// MoveLeft moves the cursor left one character
func (b *Buffer) MoveLeft() {
	if b.CursorCol > 0 {
		b.CursorCol--
	} else if b.CursorRow > 0 {
		b.CursorRow--
		b.CursorCol = len(b.Lines[b.CursorRow])
	}
}

// MoveRight moves the cursor right one character
func (b *Buffer) MoveRight() {
	if b.CursorCol < len(b.Lines[b.CursorRow]) {
		b.CursorCol++
	} else if b.CursorRow < len(b.Lines)-1 {
		b.CursorRow++
		b.CursorCol = 0
	}
}

// clampCursorCol ensures the cursor column is within the line bounds
func (b *Buffer) clampCursorCol() {
	lineLen := len(b.Lines[b.CursorRow])
	if b.CursorCol > lineLen {
		b.CursorCol = lineLen
	}
}

// DeleteLine deletes the specified row and returns its content
func (b *Buffer) DeleteLine(row int) []rune {
	if row < 0 || row >= len(b.Lines) {
		return nil
	}

	deleted := make([]rune, len(b.Lines[row]))
	copy(deleted, b.Lines[row])

	if len(b.Lines) == 1 {
		// If only one line, just clear it
		b.Lines[0] = []rune{}
	} else {
		b.Lines = append(b.Lines[:row], b.Lines[row+1:]...)
	}

	// Adjust cursor if needed
	if b.CursorRow >= len(b.Lines) {
		b.CursorRow = len(b.Lines) - 1
	}
	b.clampCursorCol()
	b.Modified = true

	return deleted
}

// DeleteCharAtCursor deletes the character under the cursor (x command)
func (b *Buffer) DeleteCharAtCursor() rune {
	line := b.Lines[b.CursorRow]
	if b.CursorCol >= len(line) {
		return 0
	}

	deleted := line[b.CursorCol]
	newLine := make([]rune, len(line)-1)
	copy(newLine[:b.CursorCol], line[:b.CursorCol])
	copy(newLine[b.CursorCol:], line[b.CursorCol+1:])
	b.Lines[b.CursorRow] = newLine
	b.Modified = true

	// Clamp cursor if now past end of line
	b.clampCursorCol()

	return deleted
}

// CopyLine returns a copy of the specified row
func (b *Buffer) CopyLine(row int) []rune {
	if row < 0 || row >= len(b.Lines) {
		return nil
	}
	copied := make([]rune, len(b.Lines[row]))
	copy(copied, b.Lines[row])
	return copied
}

// InsertLineAfter inserts a line after the specified row
func (b *Buffer) InsertLineAfter(row int, line []rune) {
	if row < 0 {
		row = 0
	}
	if row >= len(b.Lines) {
		row = len(b.Lines) - 1
	}

	newLines := make([][]rune, len(b.Lines)+1)
	copy(newLines[:row+1], b.Lines[:row+1])
	newLines[row+1] = line
	copy(newLines[row+2:], b.Lines[row+1:])
	b.Lines = newLines
	b.Modified = true
}

// InsertLineBefore inserts a line before the specified row
func (b *Buffer) InsertLineBefore(row int, line []rune) {
	if row < 0 {
		row = 0
	}
	if row > len(b.Lines) {
		row = len(b.Lines)
	}

	newLines := make([][]rune, len(b.Lines)+1)
	copy(newLines[:row], b.Lines[:row])
	newLines[row] = line
	copy(newLines[row+1:], b.Lines[row:])
	b.Lines = newLines
	b.Modified = true
}

// DeleteRange deletes text from (startRow, startCol) to (endRow, endCol) exclusive
// Returns the deleted text as a slice of lines
func (b *Buffer) DeleteRange(startRow, startCol, endRow, endCol int) [][]rune {
	// Normalize: ensure start is before end
	if startRow > endRow || (startRow == endRow && startCol > endCol) {
		startRow, endRow = endRow, startRow
		startCol, endCol = endCol, startCol
	}

	// Clamp bounds
	if startRow < 0 {
		startRow = 0
	}
	if endRow >= len(b.Lines) {
		endRow = len(b.Lines) - 1
	}
	if startCol < 0 {
		startCol = 0
	}
	if startCol > len(b.Lines[startRow]) {
		startCol = len(b.Lines[startRow])
	}
	if endCol > len(b.Lines[endRow]) {
		endCol = len(b.Lines[endRow])
	}

	// Same line deletion
	if startRow == endRow {
		line := b.Lines[startRow]
		deleted := make([]rune, endCol-startCol)
		copy(deleted, line[startCol:endCol])

		newLine := make([]rune, len(line)-(endCol-startCol))
		copy(newLine[:startCol], line[:startCol])
		copy(newLine[startCol:], line[endCol:])
		b.Lines[startRow] = newLine

		b.CursorRow = startRow
		b.CursorCol = startCol
		b.clampCursorCol()
		b.Modified = true

		return [][]rune{deleted}
	}

	// Multi-line deletion
	var deleted [][]rune

	// First line: from startCol to end
	firstLine := b.Lines[startRow]
	deleted = append(deleted, firstLine[startCol:])

	// Middle lines: entire lines
	for row := startRow + 1; row < endRow; row++ {
		lineCopy := make([]rune, len(b.Lines[row]))
		copy(lineCopy, b.Lines[row])
		deleted = append(deleted, lineCopy)
	}

	// Last line: from start to endCol
	lastLine := b.Lines[endRow]
	deleted = append(deleted, lastLine[:endCol])

	// Create the merged line
	newLine := make([]rune, startCol+len(lastLine)-endCol)
	copy(newLine[:startCol], firstLine[:startCol])
	copy(newLine[startCol:], lastLine[endCol:])

	// Remove lines and insert merged line
	b.Lines = append(b.Lines[:startRow], append([][]rune{newLine}, b.Lines[endRow+1:]...)...)

	b.CursorRow = startRow
	b.CursorCol = startCol
	b.clampCursorCol()
	b.Modified = true

	return deleted
}

// GetRange returns text from (startRow, startCol) to (endRow, endCol) inclusive
func (b *Buffer) GetRange(startRow, startCol, endRow, endCol int) [][]rune {
	// Normalize: ensure start is before end
	if startRow > endRow || (startRow == endRow && startCol > endCol) {
		startRow, endRow = endRow, startRow
		startCol, endCol = endCol, startCol
	}

	// Clamp bounds
	if startRow < 0 {
		startRow = 0
	}
	if endRow >= len(b.Lines) {
		endRow = len(b.Lines) - 1
	}
	if startCol < 0 {
		startCol = 0
	}
	if startCol > len(b.Lines[startRow]) {
		startCol = len(b.Lines[startRow])
	}
	// endCol is inclusive, so clamp to last valid index
	lineLen := len(b.Lines[endRow])
	if endCol >= lineLen {
		endCol = lineLen - 1
	}

	// Same line
	if startRow == endRow {
		line := b.Lines[startRow]
		if startCol > endCol || endCol < 0 {
			return [][]rune{{}} // Empty selection
		}
		copied := make([]rune, endCol-startCol+1)
		copy(copied, line[startCol:endCol+1])
		return [][]rune{copied}
	}

	// Multi-line
	var result [][]rune

	// First line: from startCol to end
	firstLine := b.Lines[startRow]
	first := make([]rune, len(firstLine)-startCol)
	copy(first, firstLine[startCol:])
	result = append(result, first)

	// Middle lines: entire lines
	for row := startRow + 1; row < endRow; row++ {
		lineCopy := make([]rune, len(b.Lines[row]))
		copy(lineCopy, b.Lines[row])
		result = append(result, lineCopy)
	}

	// Last line: from start to endCol (inclusive)
	lastLine := b.Lines[endRow]
	if endCol >= 0 && endCol < len(lastLine) {
		last := make([]rune, endCol+1)
		copy(last, lastLine[:endCol+1])
		result = append(result, last)
	} else {
		result = append(result, []rune{})
	}

	return result
}

// IndentRange prepends indentation to each line in the range
func (b *Buffer) IndentRange(startRow, endRow int) {
	if startRow > endRow {
		startRow, endRow = endRow, startRow
	}
	if startRow < 0 {
		startRow = 0
	}
	if endRow >= len(b.Lines) {
		endRow = len(b.Lines) - 1
	}

	// Determine what to indent with
	var indent []rune
	if b.Config.ExpandTab {
		shiftWidth := b.GetShiftWidth()
		indent = make([]rune, shiftWidth)
		for i := range indent {
			indent[i] = ' '
		}
	} else {
		indent = []rune{'\t'}
	}

	for row := startRow; row <= endRow; row++ {
		newLine := make([]rune, len(indent)+len(b.Lines[row]))
		copy(newLine, indent)
		copy(newLine[len(indent):], b.Lines[row])
		b.Lines[row] = newLine
	}
	b.Modified = true
}

// UnindentRange removes leading indentation from each line in the range
func (b *Buffer) UnindentRange(startRow, endRow int) {
	if startRow > endRow {
		startRow, endRow = endRow, startRow
	}
	if startRow < 0 {
		startRow = 0
	}
	if endRow >= len(b.Lines) {
		endRow = len(b.Lines) - 1
	}

	shiftWidth := b.GetShiftWidth()

	for row := startRow; row <= endRow; row++ {
		line := b.Lines[row]
		if len(line) == 0 {
			continue
		}

		if line[0] == '\t' {
			b.Lines[row] = line[1:]
		} else {
			// Remove up to shiftWidth leading spaces
			spaces := 0
			for i := 0; i < len(line) && i < shiftWidth && line[i] == ' '; i++ {
				spaces++
			}
			if spaces > 0 {
				b.Lines[row] = line[spaces:]
			}
		}
	}
	b.Modified = true
}

// scrollMargin is the number of lines to keep visible above/below cursor
const scrollMargin = 5

// AdjustScroll adjusts the scroll offset to keep the cursor visible
// with a margin from the top and bottom edges
// textWidth is the available width for text (excluding line numbers)
// screenHeight is the number of visible lines
func (b *Buffer) AdjustScroll(textWidth, screenHeight int) {
	if textWidth < 1 {
		textWidth = 1
	}

	// Adjust margin if screen is too small
	margin := scrollMargin
	if screenHeight < margin*2+1 {
		margin = (screenHeight - 1) / 2
	}
	if margin < 0 {
		margin = 0
	}

	// Calculate which wrapped row the cursor is on within its line
	// Account for tabs when calculating visual width
	cursorWrapRow := 0
	if b.CursorCol > 0 && textWidth > 0 {
		visualCol := b.GetVisualColumn(b.Lines[b.CursorRow], b.CursorCol)
		cursorWrapRow = visualCol / textWidth
	}

	// Calculate screen rows from ScrollOffset to cursor (top margin check)
	screenRowsFromTop := 0
	for lineIdx := b.ScrollOffset; lineIdx < b.CursorRow && lineIdx < len(b.Lines); lineIdx++ {
		visualWidth := b.GetVisualLineWidth(b.Lines[lineIdx])
		if visualWidth == 0 {
			screenRowsFromTop++
		} else {
			screenRowsFromTop += (visualWidth + textWidth - 1) / textWidth
		}
	}
	screenRowsFromTop += cursorWrapRow

	// If cursor is too close to top, scroll up
	if screenRowsFromTop < margin && b.ScrollOffset > 0 {
		// Scroll up to maintain margin
		for screenRowsFromTop < margin && b.ScrollOffset > 0 {
			b.ScrollOffset--
			visualWidth := b.GetVisualLineWidth(b.Lines[b.ScrollOffset])
			if visualWidth == 0 {
				screenRowsFromTop++
			} else {
				screenRowsFromTop += (visualWidth + textWidth - 1) / textWidth
			}
		}
		// Don't return here - need to run the final safety check below
	}

	// Calculate total screen rows used including cursor position
	screenRowsUsed := screenRowsFromTop + 1

	// If cursor is too close to bottom, scroll down
	if screenRowsUsed > screenHeight-margin {
		// Need to scroll down - find new ScrollOffset
		excess := screenRowsUsed - (screenHeight - margin)
		for excess > 0 && b.ScrollOffset < b.CursorRow {
			visualWidth := b.GetVisualLineWidth(b.Lines[b.ScrollOffset])
			lineRows := 1
			if visualWidth > 0 {
				lineRows = (visualWidth + textWidth - 1) / textWidth
			}
			if lineRows <= excess {
				excess -= lineRows
				b.ScrollOffset++
			} else {
				break
			}
		}
		// Make sure we scroll at least enough
		if excess > 0 {
			b.ScrollOffset++
		}
	}

	// Ensure cursor is never above ScrollOffset
	// Also try to maintain margin when possible
	if b.CursorRow < b.ScrollOffset {
		// Cursor jumped above visible area, reset scroll to show cursor with margin
		b.ScrollOffset = b.CursorRow - margin
		if b.ScrollOffset < 0 {
			b.ScrollOffset = 0
		}
	}
}

// FindAllMatches finds all case-insensitive occurrences of query in the buffer
// Returns a slice of SearchMatch sorted by position (row, then col)
func (b *Buffer) FindAllMatches(query string) []SearchMatch {
	if query == "" {
		return nil
	}

	var matches []SearchMatch
	queryLower := strings.ToLower(query)
	queryLen := len([]rune(query))

	for row, line := range b.Lines {
		lineStr := string(line)
		lineLower := strings.ToLower(lineStr)
		col := 0
		for {
			idx := strings.Index(lineLower[col:], queryLower)
			if idx == -1 {
				break
			}
			// Convert byte index to rune index for proper column position
			runeCol := len([]rune(lineStr[:col+idx]))
			matches = append(matches, SearchMatch{
				Row:    row,
				Col:    runeCol,
				Length: queryLen,
			})
			col += idx + 1
		}
	}
	return matches
}
