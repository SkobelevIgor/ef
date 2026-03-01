package editor

import "unicode"

// VisualColumn calculates the visual column position accounting for tab expansion.
// This is the single source of truth for visual column calculation used by both
// buffer operations and screen rendering.
func VisualColumn(line []rune, charCol int, tabStop int) int {
	if tabStop <= 0 {
		tabStop = DefaultTabStop
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

// VisualLineWidth calculates the visual width of an entire line accounting for tabs.
func VisualLineWidth(line []rune, tabStop int) int {
	return VisualColumn(line, len(line), tabStop)
}

// NormalizeRange ensures start position is before end position.
// Returns (startRow, startCol, endRow, endCol) with start <= end.
func NormalizeRange(startRow, startCol, endRow, endCol int) (int, int, int, int) {
	if startRow > endRow || (startRow == endRow && startCol > endCol) {
		startRow, endRow = endRow, startRow
		startCol, endCol = endCol, startCol
	}
	return startRow, startCol, endRow, endCol
}

// ClampPosition clamps row and col to valid buffer bounds.
// Returns clamped (row, col). Handles empty buffers.
func ClampPosition(lines [][]rune, row, col int) (int, int) {
	if len(lines) == 0 {
		return 0, 0
	}
	if row < 0 {
		row = 0
	}
	if row >= len(lines) {
		row = len(lines) - 1
	}
	lineLen := len(lines[row])
	if col > lineLen {
		col = lineLen
	}
	if col < 0 {
		col = 0
	}
	return row, col
}

// isWhitespace returns true for space and tab characters.
func isWhitespace(r rune) bool {
	return r == ' ' || r == '\t'
}

// isWordChar returns true for alphanumeric and underscore characters (Vim word chars).
func isWordChar(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r) || r == '_'
}
