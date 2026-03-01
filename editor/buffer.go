package editor

import (
	"bufio"
	"os"
	"strings"
	"time"
)

// Buffer represents the text content being edited.
// Buffer is a pure text container — cursor, scroll, and selection state live in Pane.
type Buffer struct {
	Lines       [][]rune
	Filename    string
	Modified    bool
	LastModTime time.Time // Last known modification time of the file

	// Syntax highlighting support
	FileType       string
	HighlightCache *HighlightCache

	// Filetype configuration
	Config FileTypeConfig

	// ModCount is incremented on every buffer mutation, used for cache invalidation
	ModCount uint64
}

// NewBuffer creates a new buffer and loads the file if it exists
func NewBuffer(filename string) (*Buffer, error) {
	b := &Buffer{
		Filename: filename,
		Lines:    [][]rune{{}},
		FileType: "", // Will be detected by registry
		Config: FileTypeConfig{
			TabStop:         DefaultTabStop,
			ShiftWidth:      DefaultTabStop,
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

// InsertChar inserts a character at the given position, returns new col
func (b *Buffer) InsertChar(row, col int, ch rune) int {
	b.Lines[row] = insertRunes(b.Lines[row], col, []rune{ch})
	b.Modified = true
	b.ModCount++
	return col + 1
}

// DeleteChar deletes the character before the given position (backspace), returns new row, col
func (b *Buffer) DeleteChar(row, col int) (int, int) {
	if col > 0 {
		b.Lines[row] = removeRunes(b.Lines[row], col-1, col)
		b.Modified = true
		b.ModCount++
		return row, col - 1
	} else if row > 0 {
		// Join with previous line
		prevLine := b.Lines[row-1]
		currLine := b.Lines[row]
		newCol := len(prevLine)
		b.Lines[row-1] = append(prevLine, currLine...)
		b.Lines = append(b.Lines[:row], b.Lines[row+1:]...)
		b.Modified = true
		b.ModCount++
		return row - 1, newCol
	}
	return row, col
}

// DeleteCharForward deletes the character at the given position (delete key)
func (b *Buffer) DeleteCharForward(row, col int) {
	line := b.Lines[row]
	if col < len(line) {
		b.Lines[row] = removeRunes(line, col, col+1)
		b.Modified = true
		b.ModCount++
	} else if row < len(b.Lines)-1 {
		// Join with next line
		nextLine := b.Lines[row+1]
		b.Lines[row] = append(line, nextLine...)
		b.Lines = append(b.Lines[:row+1], b.Lines[row+2:]...)
		b.Modified = true
		b.ModCount++
	}
}

// InsertNewline splits the line at the given position, returns new row, col
func (b *Buffer) InsertNewline(row, col int) (int, int) {
	line := b.Lines[row]
	left := make([]rune, col)
	copy(left, line[:col])
	right := make([]rune, len(line)-col)
	copy(right, line[col:])

	b.Lines[row] = left
	b.Lines = spliceLines(b.Lines, row+1, 0, [][]rune{right})

	b.Modified = true
	b.ModCount++
	return row + 1, 0
}

// InsertNewlineWithIndent splits the line and applies auto-indentation, returns new row, col
func (b *Buffer) InsertNewlineWithIndent(row, col int) (int, int) {
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[row])
	}

	newRow, newCol := b.InsertNewline(row, col)

	if len(indent) > 0 {
		line := b.Lines[newRow]
		newLine := make([]rune, len(indent)+len(line))
		copy(newLine, indent)
		copy(newLine[len(indent):], line)
		b.Lines[newRow] = newLine
		newCol = len(indent)
	}
	return newRow, newCol
}

// OpenLineBelow opens a new line below the given row with auto-indentation, returns new row, col
func (b *Buffer) OpenLineBelow(row int) (int, int) {
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[row])
	}

	newLine := make([]rune, len(indent))
	copy(newLine, indent)

	b.InsertLineAfter(row, newLine)

	return row + 1, len(indent)
}

// OpenLineAbove opens a new line above the given row with auto-indentation, returns new row, col
func (b *Buffer) OpenLineAbove(row int) (int, int) {
	var indent []rune
	if b.Config.AutoIndentation {
		indent = b.getLeadingWhitespace(b.Lines[row])
	}

	newLine := make([]rune, len(indent))
	copy(newLine, indent)

	b.InsertLineBefore(row, newLine)

	// Row stays the same (the new line is at the old row position)
	return row, len(indent)
}

// InsertTab inserts a tab character or spaces, returns new col
func (b *Buffer) InsertTab(row, col int) int {
	if b.Config.ExpandTab {
		tabStop := b.Config.TabStop
		if tabStop <= 0 {
			tabStop = DefaultTabStop
		}
		visualCol := b.GetVisualColumn(b.Lines[row], col)
		spacesNeeded := tabStop - (visualCol % tabStop)
		for i := 0; i < spacesNeeded; i++ {
			col = b.InsertChar(row, col, ' ')
		}
	} else {
		col = b.InsertChar(row, col, '\t')
	}
	return col
}

// getLeadingWhitespace returns the leading whitespace from a line
func (b *Buffer) getLeadingWhitespace(line []rune) []rune {
	var ws []rune
	for _, ch := range line {
		if isWhitespace(ch) {
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
	return DefaultTabStop
}

// GetVisualColumn calculates the visual column position accounting for tab expansion
func (b *Buffer) GetVisualColumn(line []rune, charCol int) int {
	return VisualColumn(line, charCol, b.Config.TabStop)
}

// GetVisualLineWidth calculates the visual width of a line accounting for tabs
func (b *Buffer) GetVisualLineWidth(line []rune) int {
	return VisualLineWidth(line, b.Config.TabStop)
}

// DeleteLine deletes the specified row and returns its content
func (b *Buffer) DeleteLine(row int) []rune {
	if row < 0 || row >= len(b.Lines) {
		return nil
	}

	deleted := make([]rune, len(b.Lines[row]))
	copy(deleted, b.Lines[row])

	if len(b.Lines) == 1 {
		b.Lines[0] = []rune{}
	} else {
		b.Lines = append(b.Lines[:row], b.Lines[row+1:]...)
	}

	b.Modified = true
	b.ModCount++

	return deleted
}

// DeleteCharAt deletes the character at (row, col), returns the deleted rune
func (b *Buffer) DeleteCharAt(row, col int) rune {
	line := b.Lines[row]
	if col >= len(line) {
		return 0
	}

	deleted := line[col]
	b.Lines[row] = removeRunes(line, col, col+1)
	b.Modified = true
	b.ModCount++

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
	b.Lines = spliceLines(b.Lines, row+1, 0, [][]rune{line})
	b.Modified = true
	b.ModCount++
}

// InsertLineBefore inserts a line before the specified row
func (b *Buffer) InsertLineBefore(row int, line []rune) {
	if row < 0 {
		row = 0
	}
	if row > len(b.Lines) {
		row = len(b.Lines)
	}
	b.Lines = spliceLines(b.Lines, row, 0, [][]rune{line})
	b.Modified = true
	b.ModCount++
}

// DeleteRange deletes text from (startRow, startCol) to (endRow, endCol) exclusive
// Returns the deleted text as a slice of lines
func (b *Buffer) DeleteRange(startRow, startCol, endRow, endCol int) [][]rune {
	startRow, startCol, endRow, endCol = NormalizeRange(startRow, startCol, endRow, endCol)

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
		b.Lines[startRow] = removeRunes(line, startCol, endCol)

		b.Modified = true
		b.ModCount++

		return [][]rune{deleted}
	}

	// Multi-line deletion
	var deleted [][]rune

	firstLine := b.Lines[startRow]
	deleted = append(deleted, firstLine[startCol:])

	for row := startRow + 1; row < endRow; row++ {
		lineCopy := make([]rune, len(b.Lines[row]))
		copy(lineCopy, b.Lines[row])
		deleted = append(deleted, lineCopy)
	}

	lastLine := b.Lines[endRow]
	deleted = append(deleted, lastLine[:endCol])

	newLine := make([]rune, startCol+len(lastLine)-endCol)
	copy(newLine[:startCol], firstLine[:startCol])
	copy(newLine[startCol:], lastLine[endCol:])

	b.Lines = spliceLines(b.Lines, startRow, endRow-startRow+1, [][]rune{newLine})

	b.Modified = true
	b.ModCount++

	return deleted
}

// GetRange returns text from (startRow, startCol) to (endRow, endCol) inclusive
func (b *Buffer) GetRange(startRow, startCol, endRow, endCol int) [][]rune {
	startRow, startCol, endRow, endCol = NormalizeRange(startRow, startCol, endRow, endCol)

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
	b.ModCount++
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
	b.ModCount++
}

// hasContent returns true if a line contains non-whitespace characters
func hasContent(line []rune) bool {
	for _, ch := range line {
		if !isWhitespace(ch) {
			return true
		}
	}
	return false
}

// stripLeadingWhitespace returns a line with leading whitespace removed
func stripLeadingWhitespace(line []rune) []rune {
	for i, ch := range line {
		if !isWhitespace(ch) {
			return line[i:]
		}
	}
	return nil
}

// lastNonWhitespace returns the last non-whitespace character in a line, or 0
func lastNonWhitespace(line []rune) rune {
	for i := len(line) - 1; i >= 0; i-- {
		if !isWhitespace(line[i]) {
			return line[i]
		}
	}
	return 0
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
