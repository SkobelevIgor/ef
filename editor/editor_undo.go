package editor

// applyRemoveText and applyInsertText are mirror functions:
//   - applyRemoveText removes text (used by undo-insert and redo-delete)
//   - applyInsertText inserts text (used by undo-delete and redo-insert)
// Both handle three cases identically but inverted: whole-line operations
// (LineDeletion flag), single-line inline edits, and multi-line edits
// that split/merge lines at the change point.

// applyRemoveText removes previously inserted text from the buffer (used by undo insert / redo delete)
func applyRemoveText(buf *Buffer, change *Change) {
	if change.LineDeletion {
		endRow := change.Row + len(change.Text)
		if endRow <= len(buf.Lines) {
			buf.Lines = spliceLines(buf.Lines, change.Row, endRow-change.Row, nil)
		}
		if len(buf.Lines) == 0 {
			buf.Lines = [][]rune{{}}
		}
		return
	}
	if len(change.Text) == 1 {
		line := buf.Lines[change.Row]
		textLen := len(change.Text[0])
		if change.Col+textLen <= len(line) {
			buf.Lines[change.Row] = removeRunes(line, change.Col, change.Col+textLen)
		}
	} else {
		endRow := change.Row + len(change.Text) - 1
		if endRow < len(buf.Lines) {
			lastLine := buf.Lines[endRow]
			afterText := lastLine[len(change.Text[len(change.Text)-1]):]
			firstPart := buf.Lines[change.Row][:change.Col]
			merged := make([]rune, len(firstPart)+len(afterText))
			copy(merged, firstPart)
			copy(merged[len(firstPart):], afterText)
			buf.Lines = spliceLines(buf.Lines, change.Row, endRow-change.Row+1, [][]rune{merged})
		}
	}
}

// applyInsertText inserts text into the buffer (used by undo delete / redo insert)
func applyInsertText(buf *Buffer, change *Change) {
	if change.LineDeletion {
		insertLines := copyLines(change.Text)
		buf.Lines = spliceLines(buf.Lines, change.Row, 0, insertLines)
		return
	}
	if len(change.Text) == 1 {
		buf.Lines[change.Row] = insertRunes(buf.Lines[change.Row], change.Col, change.Text[0])
	} else {
		line := buf.Lines[change.Row]
		firstPart := line[:change.Col]
		restPart := line[change.Col:]

		newFirstLine := append(append([]rune(nil), firstPart...), change.Text[0]...)

		lastText := change.Text[len(change.Text)-1]
		newLastLine := append(append([]rune(nil), lastText...), restPart...)

		insertBlock := make([][]rune, len(change.Text))
		insertBlock[0] = newFirstLine
		for i := 1; i < len(change.Text)-1; i++ {
			lineCopy := make([]rune, len(change.Text[i]))
			copy(lineCopy, change.Text[i])
			insertBlock[i] = lineCopy
		}
		insertBlock[len(change.Text)-1] = newLastLine

		buf.Lines = spliceLines(buf.Lines, change.Row, 1, insertBlock)
	}
}

// applyReplaceLines replaces all buffer lines with the given lines and updates pane cursor
func applyReplaceLines(buf *Buffer, pane *Pane, change *Change, lines [][]rune) {
	buf.Lines = copyLines(lines)
	pane.CursorRow, pane.CursorCol = ClampPosition(buf.Lines, change.Row, change.Col)
	buf.Modified = true
}

// resolveChangeBuffer finds the buffer for a change and switches pane if needed
func (e *Editor) resolveChangeBuffer(change *Change) *Buffer {
	buf := change.Buffer
	if buf == nil {
		buf = e.activeBuffer()
	}
	if e.activePane().Buffer != buf {
		for i, p := range e.panes {
			if p.Buffer == buf {
				e.activePaneIdx = i
				break
			}
		}
	}
	return buf
}

// undo reverses the last change
func (e *Editor) undo() {
	change := e.history.Undo()
	if change == nil {
		return
	}

	buf := e.resolveChangeBuffer(change)
	pane := e.activePane()

	switch change.Type {
	case ChangeInsert:
		applyRemoveText(buf, change)
		pane.CursorRow = change.Row
		pane.CursorCol = change.Col
		buf.Modified = true
	case ChangeDelete:
		applyInsertText(buf, change)
		pane.CursorRow = change.Row
		pane.CursorCol = change.Col
		buf.Modified = true
	case ChangeReplace:
		applyReplaceLines(buf, pane, change, change.OldText)
	}

	pane.clampCursorCol()
	e.scheduleAutoSave()
}

// redo reapplies the last undone change
func (e *Editor) redo() {
	change := e.history.Redo()
	if change == nil {
		return
	}

	buf := e.resolveChangeBuffer(change)
	pane := e.activePane()

	switch change.Type {
	case ChangeInsert:
		applyInsertText(buf, change)
		if len(change.Text) == 1 {
			pane.CursorCol = change.Col + len(change.Text[0])
		} else {
			pane.CursorRow = change.Row + len(change.Text) - 1
			pane.CursorCol = len(change.Text[len(change.Text)-1])
		}
		buf.Modified = true
	case ChangeDelete:
		applyRemoveText(buf, change)
		pane.CursorRow = change.Row
		pane.CursorCol = change.Col
		if pane.CursorRow >= len(buf.Lines) {
			pane.CursorRow = len(buf.Lines) - 1
		}
		buf.Modified = true
	case ChangeReplace:
		applyReplaceLines(buf, pane, change, change.Text)
	}

	pane.clampCursorCol()
	e.scheduleAutoSave()
}
