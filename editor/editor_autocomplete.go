package editor

// getCurrentWordPrefix returns the word being typed at cursor position
func (e *Editor) getCurrentWordPrefix() (prefix string, startCol int) {
	pane := e.activePane()
	buf := pane.Buffer
	if pane.CursorCol == 0 {
		return "", 0
	}
	line := buf.Lines[pane.CursorRow]
	if len(line) == 0 {
		return "", 0
	}

	// Ensure cursor is within line bounds
	cursorCol := pane.CursorCol
	if cursorCol > len(line) {
		cursorCol = len(line)
	}

	// Scan backwards from cursor to find word start
	startCol = cursorCol
	for startCol > 0 && !isDelimiter(line[startCol-1]) {
		startCol--
	}

	if startCol >= cursorCol {
		return "", startCol
	}

	prefix = string(line[startCol:cursorCol])
	return prefix, startCol
}

// triggerAutocomplete triggers autocomplete suggestions based on current word prefix
func (e *Editor) triggerAutocomplete() {
	prefix, startCol := e.getCurrentWordPrefix()
	if len(prefix) < 2 {
		e.inputState.Autocomplete = nil
		return
	}

	pane := e.activePane()
	buf := pane.Buffer

	// Reuse existing state for word cache, or create a temporary one
	ac := e.inputState.Autocomplete
	if ac == nil {
		ac = &AutocompleteState{}
	}
	words := ac.GetWords(buf.Lines, buf.ModCount, pane.CursorRow, startCol)
	suggestions := FindSuggestions(words, prefix, 10)

	if len(suggestions) == 0 {
		e.inputState.Autocomplete = nil
		return
	}

	ac.Active = true
	ac.Prefix = prefix
	ac.PrefixCol = startCol
	ac.Suggestions = suggestions
	ac.SelectedIdx = 0
	e.inputState.Autocomplete = ac
}

// acceptAutocomplete inserts the selected suggestion
func (e *Editor) acceptAutocomplete() {
	ac := e.inputState.Autocomplete
	if ac == nil || len(ac.Suggestions) == 0 {
		return
	}

	selected := ac.Selected()
	if selected == nil {
		return
	}

	buf := e.activeBuffer()
	pane := e.activePane()
	line := buf.Lines[pane.CursorRow]

	// Replace the typed prefix with the full suggested word
	// Cursor is at the end of the typed prefix
	prefixLen := len([]rune(ac.Prefix))
	prefixStart := pane.CursorCol - prefixLen
	if prefixStart < 0 {
		prefixStart = 0
	}

	fullWord := []rune(selected.Word)

	// Build new line: before prefix + full word + after cursor
	newLine := make([]rune, prefixStart+len(fullWord)+len(line)-pane.CursorCol)
	copy(newLine[:prefixStart], line[:prefixStart])
	copy(newLine[prefixStart:], fullWord)
	copy(newLine[prefixStart+len(fullWord):], line[pane.CursorCol:])

	buf.Lines[pane.CursorRow] = newLine
	pane.CursorCol = prefixStart + len(fullWord)
	buf.Modified = true

	e.inputState.Autocomplete = nil
	e.scheduleAutoSave()
}
