package editor

// FormatResult contains the result of a formatting operation
type FormatResult struct {
	Lines   [][]rune // Formatted lines
	Changed bool     // Whether any changes were made
	Error   error    // Any error that occurred
}

// Formatter interface for code formatting
type Formatter interface {
	// Format formats all lines
	Format(lines [][]rune) FormatResult
	// FormatRange formats a range of lines
	FormatRange(lines [][]rune, startLine, endLine int) FormatResult
	// FormatOnSave returns whether to format on save
	FormatOnSave() bool
	// GetIndent returns the indentation for a new line based on the previous line
	GetIndent(prevLine []rune, cursorCol int) []rune
}

// BaseFormatter provides a no-op formatter implementation
type BaseFormatter struct {
	tabSize   int
	useSpaces bool
}

// NewBaseFormatter creates a new base formatter
func NewBaseFormatter(tabSize int, useSpaces bool) *BaseFormatter {
	if tabSize <= 0 {
		tabSize = 4
	}
	return &BaseFormatter{
		tabSize:   tabSize,
		useSpaces: useSpaces,
	}
}

// Format returns the lines unchanged (no-op)
func (f *BaseFormatter) Format(lines [][]rune) FormatResult {
	return FormatResult{
		Lines:   lines,
		Changed: false,
	}
}

// FormatRange returns the lines unchanged (no-op)
func (f *BaseFormatter) FormatRange(lines [][]rune, startLine, endLine int) FormatResult {
	return FormatResult{
		Lines:   lines,
		Changed: false,
	}
}

// FormatOnSave returns false (no auto-format)
func (f *BaseFormatter) FormatOnSave() bool {
	return false
}

// GetIndent returns the leading whitespace from the previous line
// This provides basic auto-indentation that matches the previous line
func (f *BaseFormatter) GetIndent(prevLine []rune, cursorCol int) []rune {
	if len(prevLine) == 0 {
		return nil
	}

	// Count leading whitespace
	var indent []rune
	for _, ch := range prevLine {
		if ch == ' ' || ch == '\t' {
			indent = append(indent, ch)
		} else {
			break
		}
	}

	return indent
}

// TabSize returns the tab size setting
func (f *BaseFormatter) TabSize() int {
	return f.tabSize
}

// UseSpaces returns whether to use spaces instead of tabs
func (f *BaseFormatter) UseSpaces() bool {
	return f.useSpaces
}
