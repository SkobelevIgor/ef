package editor

// insertRunes inserts text into line at the given position, returning a new slice.
func insertRunes(line []rune, pos int, text []rune) []rune {
	result := make([]rune, len(line)+len(text))
	copy(result[:pos], line[:pos])
	copy(result[pos:], text)
	copy(result[pos+len(text):], line[pos:])
	return result
}

// removeRunes removes the range [start, end) from line, returning a new slice.
func removeRunes(line []rune, start, end int) []rune {
	result := make([]rune, len(line)-(end-start))
	copy(result[:start], line[:start])
	copy(result[start:], line[end:])
	return result
}

// spliceLines removes deleteCount lines starting at start, inserts the given lines,
// and returns the new slice.
func spliceLines(lines [][]rune, start, deleteCount int, insert [][]rune) [][]rune {
	result := make([][]rune, len(lines)-deleteCount+len(insert))
	copy(result[:start], lines[:start])
	copy(result[start:start+len(insert)], insert)
	copy(result[start+len(insert):], lines[start+deleteCount:])
	return result
}
