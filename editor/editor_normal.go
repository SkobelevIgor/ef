package editor

import "github.com/gdamore/tcell/v2"

// handleNormalMode handles key events in normal mode (navigation/commands)
func (e *Editor) handleNormalMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	_, height := e.screen.Size()

	// Handle pending goto line mode
	if e.inputState.PendingGotoLine {
		return e.handleGotoLineInput(ev)
	}

	// Handle pending find character
	if e.inputState.PendingFindForward || e.inputState.PendingFindBackward {
		return e.handleFindCharInput(ev)
	}

	// Handle pending mark operations
	if e.inputState.PendingMark || e.inputState.PendingJumpToMark {
		return e.handleMarkInput(ev)
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.inputState.Reset()
		return false

	case tcell.KeyUp:
		pane.MoveUpN(e.inputState.GetCount())

	case tcell.KeyDown:
		pane.MoveDownN(e.inputState.GetCount())

	case tcell.KeyLeft:
		pane.MoveLeftN(e.inputState.GetCount())

	case tcell.KeyRight:
		pane.MoveRightN(e.inputState.GetCount())

	case tcell.KeyCtrlD:
		pane.PageDown(height)

	case tcell.KeyCtrlU:
		pane.PageUp(height)

	case tcell.KeyCtrlR:
		e.redo()

	case tcell.KeyRune:
		skipReset := e.handleNormalModeRune(ev.Rune())
		if !skipReset {
			e.inputState.Reset()
		}
		return false
	}

	e.inputState.Reset()
	return false
}

// handleNormalModeRune handles rune keys in normal mode.
// Returns true if the command accumulates state (skip auto-reset),
// false if auto-reset should happen after this command.
func (e *Editor) handleNormalModeRune(r rune) bool {
	pane := e.activePane()
	buf := pane.Buffer
	count := e.inputState.GetCount()

	// Handle pending operator
	if e.inputState.PendingOperator != 0 {
		e.handlePendingOperator(r)
		return false
	}

	switch r {
	// Numeric prefix — accumulates state, skip reset
	case '1', '2', '3', '4', '5', '6', '7', '8', '9':
		e.inputState.AddDigit(int(r - '0'))
		return true
	case '0':
		if e.inputState.HasCount {
			e.inputState.AddDigit(0)
			return true
		}
		pane.MoveToLineStart()
		return false

	// Navigation
	case 'h':
		pane.MoveLeftN(count)
	case 'j':
		pane.MoveDownN(count)
	case 'k':
		pane.MoveUpN(count)
	case 'l':
		pane.MoveRightN(count)
	case '$':
		pane.MoveToLineEnd()

	// Mode switching
	case 'i':
		e.enterInsertMode()
	case 'a':
		if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol++
		}
		e.enterInsertMode()
	case 'A':
		pane.MoveToLineEnd()
		e.enterInsertMode()
	case 'I':
		pane.MoveToLineStart()
		e.enterInsertMode()
	case 'o':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineBelow(pane.CursorRow)
		e.scheduleAutoSave()
	case 'O':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineAbove(pane.CursorRow)
		e.scheduleAutoSave()
	case 'v':
		e.mode = ModeVisual
		pane.StartSelection()

	// Find character — accumulates state, skip reset
	case 'f':
		e.inputState.PendingFindForward = true
		return true
	case 'F':
		e.inputState.PendingFindBackward = true
		return true
	case ';':
		// Repeat last find
		if e.inputState.HasLastFind {
			for i := 0; i < count; i++ {
				if e.inputState.LastFindForward {
					pane.FindCharForward(e.inputState.LastFindChar)
				} else {
					pane.FindCharBackward(e.inputState.LastFindChar)
				}
			}
		}
	case ',':
		// Repeat last find in reverse
		if e.inputState.HasLastFind {
			for i := 0; i < count; i++ {
				if e.inputState.LastFindForward {
					pane.FindCharBackward(e.inputState.LastFindChar)
				} else {
					pane.FindCharForward(e.inputState.LastFindChar)
				}
			}
		}

	// Goto line — accumulates state, skip reset
	case ':':
		e.inputState.PendingGotoLine = true
		e.inputState.GotoLineBuffer = ""
		return true

	// Page navigation
	case 'G':
		if e.inputState.HasCount {
			pane.GotoLine(count)
		} else {
			pane.GotoLine(len(buf.Lines))
		}
	case 'g':
		pane.GotoLine(1)

	// Word navigation
	case 'w':
		for i := 0; i < count; i++ {
			pane.MoveToNextWord()
		}
	case 'b':
		for i := 0; i < count; i++ {
			pane.MoveToPrevWord()
		}

	// Operators — accumulates state, skip reset
	case 'd':
		e.inputState.PendingOperator = 'd'
		return true
	case 'y':
		e.inputState.PendingOperator = 'y'
		return true

	// Delete character under cursor
	case 'x':
		startCol := pane.CursorCol
		var deletedChars []rune
		for i := 0; i < count; i++ {
			if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
				deleted := buf.DeleteCharAt(pane.CursorRow, pane.CursorCol)
				if deleted != 0 {
					deletedChars = append(deletedChars, deleted)
				}
			}
		}
		if len(deletedChars) > 0 {
			e.clipboard = [][]rune{deletedChars}
			e.clipboardLine = false
			e.history.RecordDelete(buf, pane.CursorRow, startCol, [][]rune{deletedChars})
		}
		e.scheduleAutoSave()

	// Paste after cursor
	case 'p':
		if len(e.clipboard) > 0 {
			oldLines := copyLines(buf.Lines)
			cursorRow, cursorCol := pane.CursorRow, pane.CursorCol
			firstRow, lastRow := e.pasteAfter()
			e.reindentPastedRange(buf, firstRow, lastRow)
			e.history.Push(&Change{
				Type:    ChangeReplace,
				Buffer:  buf,
				Row:     cursorRow,
				Col:     cursorCol,
				Text:    copyLines(buf.Lines),
				OldText: oldLines,
			})
		}

	// Paste before cursor
	case 'P':
		if len(e.clipboard) > 0 {
			oldLines := copyLines(buf.Lines)
			cursorRow, cursorCol := pane.CursorRow, pane.CursorCol
			firstRow, lastRow := e.pasteBefore()
			e.reindentPastedRange(buf, firstRow, lastRow)
			e.history.Push(&Change{
				Type:    ChangeReplace,
				Buffer:  buf,
				Row:     cursorRow,
				Col:     cursorCol,
				Text:    copyLines(buf.Lines),
				OldText: oldLines,
			})
		}

	// Undo
	case 'u':
		e.undo()

	// Mark operations — accumulates state, skip reset
	case 'm':
		e.inputState.PendingMark = true
		return true
	case '`':
		e.inputState.PendingJumpToMark = true
		return true
	}

	return false
}

// handlePendingOperator handles the second key after an operator (d, y)
func (e *Editor) handlePendingOperator(r rune) {
	pane := e.activePane()
	buf := pane.Buffer
	op := e.inputState.PendingOperator
	count := e.inputState.GetCount()

	switch {
	// dd - delete line(s)
	case op == 'd' && r == 'd':
		startRow := pane.CursorRow
		var deleted [][]rune
		for i := 0; i < count && pane.CursorRow < len(buf.Lines); i++ {
			line := buf.DeleteLine(pane.CursorRow)
			deleted = append(deleted, line)
		}
		e.clipboard = deleted
		e.clipboardLine = true
		e.history.RecordDeleteLines(buf, startRow, deleted)
		pane.clampCursor()
		e.scheduleAutoSave()

	// yy - yank line(s)
	case op == 'y' && r == 'y':
		var yanked [][]rune
		for i := 0; i < count && pane.CursorRow+i < len(buf.Lines); i++ {
			yanked = append(yanked, buf.CopyLine(pane.CursorRow+i))
		}
		e.clipboard = yanked
		e.clipboardLine = true

	// dw - delete word
	case op == 'd' && r == 'w':
		startRow, startCol := pane.CursorRow, pane.CursorCol
		for i := 0; i < count; i++ {
			pane.MoveToNextWord()
		}
		endRow, endCol := pane.CursorRow, pane.CursorCol
		pane.CursorRow, pane.CursorCol = startRow, startCol
		deleted := buf.DeleteRange(startRow, startCol, endRow, endCol)
		e.clipboard = deleted
		e.clipboardLine = false
		e.history.RecordDelete(buf, startRow, startCol, deleted)
		e.scheduleAutoSave()

	// db - delete word backward
	case op == 'd' && r == 'b':
		endRow, endCol := pane.CursorRow, pane.CursorCol
		for i := 0; i < count; i++ {
			pane.MoveToPrevWord()
		}
		startRow, startCol := pane.CursorRow, pane.CursorCol
		deleted := buf.DeleteRange(startRow, startCol, endRow, endCol)
		e.clipboard = deleted
		e.clipboardLine = false
		e.history.RecordDelete(buf, startRow, startCol, deleted)
		e.scheduleAutoSave()

	// d$ - delete to end of line
	case op == 'd' && r == '$':
		line := buf.Lines[pane.CursorRow]
		if pane.CursorCol < len(line) {
			deleted := make([]rune, len(line)-pane.CursorCol)
			copy(deleted, line[pane.CursorCol:])
			buf.Lines[pane.CursorRow] = line[:pane.CursorCol]
			e.clipboard = [][]rune{deleted}
			e.clipboardLine = false
			e.history.RecordDelete(buf, pane.CursorRow, pane.CursorCol, [][]rune{deleted})
			buf.Modified = true
			e.scheduleAutoSave()
		}

	// d0 - delete to beginning of line
	case op == 'd' && r == '0':
		line := buf.Lines[pane.CursorRow]
		if pane.CursorCol > 0 {
			deleted := make([]rune, pane.CursorCol)
			copy(deleted, line[:pane.CursorCol])
			buf.Lines[pane.CursorRow] = line[pane.CursorCol:]
			e.history.RecordDelete(buf, pane.CursorRow, 0, [][]rune{deleted})
			pane.CursorCol = 0
			e.clipboard = [][]rune{deleted}
			e.clipboardLine = false
			buf.Modified = true
			e.scheduleAutoSave()
		}
	}
}

// handleFindCharInput handles character input after f or F
func (e *Editor) handleFindCharInput(ev *tcell.EventKey) bool {
	pane := e.activePane()
	count := e.inputState.GetCount()

	if ev.Key() == tcell.KeyEscape {
		e.inputState.Reset()
		return false
	}

	if ev.Key() == tcell.KeyRune {
		ch := ev.Rune()
		forward := e.inputState.PendingFindForward

		for i := 0; i < count; i++ {
			if forward {
				pane.FindCharForward(ch)
			} else {
				pane.FindCharBackward(ch)
			}
		}

		e.inputState.SaveLastFind(ch, forward)
		e.inputState.Reset()
	}

	return false
}

// handleGotoLineInput handles input in goto line mode (after :)
func (e *Editor) handleGotoLineInput(ev *tcell.EventKey) bool {
	pane := e.activePane()

	switch ev.Key() {
	case tcell.KeyEscape:
		e.inputState.Reset()
	case tcell.KeyEnter:
		if e.inputState.GotoLineBuffer != "" {
			lineNum := 0
			for _, ch := range e.inputState.GotoLineBuffer {
				if ch >= '0' && ch <= '9' {
					lineNum = lineNum*10 + int(ch-'0')
				}
			}
			if lineNum > 0 {
				pane.GotoLine(lineNum)
			}
		}
		e.inputState.Reset()
	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(e.inputState.GotoLineBuffer) > 0 {
			e.inputState.GotoLineBuffer = e.inputState.GotoLineBuffer[:len(e.inputState.GotoLineBuffer)-1]
		}
	case tcell.KeyRune:
		ch := ev.Rune()
		if ch >= '0' && ch <= '9' {
			e.inputState.GotoLineBuffer += string(ch)
		}
	}

	return false
}
