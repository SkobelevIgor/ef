package editor

// performPaste pastes clipboard content relative to cursor.
// When before is false, paste after cursor (p command).
// When before is true, paste at/before cursor (P command).
// Returns the range of inserted lines (firstRow, lastRow) for reindentation.
func (e *Editor) performPaste(before bool) (int, int) {
	if len(e.clipboard) == 0 {
		return -1, -1
	}

	if e.clipboardLine {
		return e.pasteWholeLines(before)
	}

	// Compute insert position: at cursor for before, after cursor for after
	pane := e.activePane()
	buf := pane.Buffer
	insertPos := pane.CursorCol
	if !before {
		insertPos = pane.CursorCol + 1
		line := buf.Lines[pane.CursorRow]
		if insertPos > len(line) {
			insertPos = len(line)
		}
	}

	if len(e.clipboard) == 1 {
		e.pasteSingleLine(insertPos)
		return -1, -1
	}

	return e.pasteMultiLine(insertPos)
}

// pasteWholeLines pastes clipboard content as whole lines before or after cursor line.
func (e *Editor) pasteWholeLines(before bool) (int, int) {
	pane := e.activePane()
	buf := pane.Buffer

	var firstRow int
	if before {
		firstRow = pane.CursorRow
		for i := len(e.clipboard) - 1; i >= 0; i-- {
			lineCopy := make([]rune, len(e.clipboard[i]))
			copy(lineCopy, e.clipboard[i])
			buf.InsertLineBefore(pane.CursorRow, lineCopy)
		}
	} else {
		firstRow = pane.CursorRow + 1
		for i := len(e.clipboard) - 1; i >= 0; i-- {
			lineCopy := make([]rune, len(e.clipboard[i]))
			copy(lineCopy, e.clipboard[i])
			buf.InsertLineAfter(pane.CursorRow, lineCopy)
		}
		pane.CursorRow++
	}
	lastRow := firstRow + len(e.clipboard) - 1
	pane.CursorCol = 0
	buf.Modified = true
	e.scheduleAutoSave()
	return firstRow, lastRow
}

// pasteSingleLine pastes a single clipboard line inline at insertPos.
func (e *Editor) pasteSingleLine(insertPos int) {
	pane := e.activePane()
	buf := pane.Buffer

	line := buf.Lines[pane.CursorRow]
	newLine := make([]rune, len(line)+len(e.clipboard[0]))
	copy(newLine[:insertPos], line[:insertPos])
	copy(newLine[insertPos:], e.clipboard[0])
	copy(newLine[insertPos+len(e.clipboard[0]):], line[insertPos:])
	buf.Lines[pane.CursorRow] = newLine
	pane.CursorCol = insertPos + len(e.clipboard[0]) - 1
	if pane.CursorCol < 0 {
		pane.CursorCol = 0
	}
	buf.Modified = true
	e.scheduleAutoSave()
}

// pasteMultiLine pastes multi-line clipboard content, splitting the current line.
func (e *Editor) pasteMultiLine(insertPos int) (int, int) {
	pane := e.activePane()
	buf := pane.Buffer

	line := buf.Lines[pane.CursorRow]
	firstRow := pane.CursorRow + 1

	// First part of current line + first clipboard line
	firstPart := make([]rune, insertPos+len(e.clipboard[0]))
	copy(firstPart[:insertPos], line[:insertPos])
	copy(firstPart[insertPos:], e.clipboard[0])

	// Last clipboard line + rest of current line
	lastClip := e.clipboard[len(e.clipboard)-1]
	lastPart := make([]rune, len(lastClip)+len(line)-insertPos)
	copy(lastPart, lastClip)
	copy(lastPart[len(lastClip):], line[insertPos:])

	// Build new lines
	newLines := make([][]rune, len(buf.Lines)+len(e.clipboard)-1)
	copy(newLines[:pane.CursorRow], buf.Lines[:pane.CursorRow])
	newLines[pane.CursorRow] = firstPart

	for i := 1; i < len(e.clipboard)-1; i++ {
		lineCopy := make([]rune, len(e.clipboard[i]))
		copy(lineCopy, e.clipboard[i])
		newLines[pane.CursorRow+i] = lineCopy
	}

	lastRow := pane.CursorRow + len(e.clipboard) - 1
	newLines[lastRow] = lastPart
	copy(newLines[pane.CursorRow+len(e.clipboard):], buf.Lines[pane.CursorRow+1:])

	buf.Lines = newLines
	pane.CursorRow += len(e.clipboard) - 1
	pane.CursorCol = len(lastClip) - 1
	if pane.CursorCol < 0 {
		pane.CursorCol = 0
	}

	buf.Modified = true
	e.scheduleAutoSave()
	return firstRow, lastRow
}

// pasteAfter is a convenience wrapper for performPaste(false)
func (e *Editor) pasteAfter() (int, int) { return e.performPaste(false) }

// pasteBefore is a convenience wrapper for performPaste(true)
func (e *Editor) pasteBefore() (int, int) { return e.performPaste(true) }
