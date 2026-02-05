package editor

import "unicode"

// Mark represents a saved cursor position in a buffer
type Mark struct {
	Row int // Line number (0-indexed)
	Col int // Column position (0-indexed)
}

// isValidMarkIdentifier returns true if the rune is a valid mark identifier (0-9, a-z, A-Z)
func isValidMarkIdentifier(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r)
}
