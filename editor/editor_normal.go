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
		e.inputState.Reset()

	case tcell.KeyDown:
		pane.MoveDownN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyLeft:
		pane.MoveLeftN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyRight:
		pane.MoveRightN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyCtrlD:
		pane.PageDown(height)
		e.inputState.Reset()

	case tcell.KeyCtrlU:
		pane.PageUp(height)
		e.inputState.Reset()

	case tcell.KeyCtrlR:
		e.redo()
		e.inputState.Reset()

	case tcell.KeyRune:
		return e.handleNormalModeRune(ev.Rune())
	}

	return false
}

// handleNormalModeRune handles rune keys in normal mode
func (e *Editor) handleNormalModeRune(r rune) bool {
	pane := e.activePane()
	buf := pane.Buffer
	count := e.inputState.GetCount()

	// Handle pending operator
	if e.inputState.PendingOperator != 0 {
		return e.handlePendingOperator(r)
	}

	switch r {
	// Numeric prefix
	case '1', '2', '3', '4', '5', '6', '7', '8', '9':
		e.inputState.AddDigit(int(r - '0'))
		return false
	case '0':
		if e.inputState.HasCount {
			e.inputState.AddDigit(0)
		} else {
			pane.MoveToLineStart()
		}
		return false

	// Navigation
	case 'h':
		pane.MoveLeftN(count)
		e.inputState.Reset()
	case 'j':
		pane.MoveDownN(count)
		e.inputState.Reset()
	case 'k':
		pane.MoveUpN(count)
		e.inputState.Reset()
	case 'l':
		pane.MoveRightN(count)
		e.inputState.Reset()
	case '$':
		pane.MoveToLineEnd()
		e.inputState.Reset()

	// Mode switching
	case 'i':
		e.enterInsertMode()
		e.inputState.Reset()
	case 'a':
		if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol++
		}
		e.enterInsertMode()
		e.inputState.Reset()
	case 'A':
		pane.MoveToLineEnd()
		e.enterInsertMode()
		e.inputState.Reset()
	case 'I':
		pane.MoveToLineStart()
		e.enterInsertMode()
		e.inputState.Reset()
	case 'o':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineBelow(pane.CursorRow)
		e.inputState.Reset()
		e.scheduleAutoSave()
	case 'O':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineAbove(pane.CursorRow)
		e.inputState.Reset()
		e.scheduleAutoSave()
	case 'v':
		e.mode = ModeVisual
		pane.StartSelection()
		e.inputState.Reset()

	// Find character
	case 'f':
		e.inputState.PendingFindForward = true
	case 'F':
		e.inputState.PendingFindBackward = true
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
		e.inputState.Reset()
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
		e.inputState.Reset()

	// Goto line
	case ':':
		e.inputState.PendingGotoLine = true
		e.inputState.GotoLineBuffer = ""

	// Page navigation (like G and gg)
	case 'G':
		if e.inputState.HasCount {
			pane.GotoLine(count)
		} else {
			pane.GotoLine(len(buf.Lines)) // Go to last line
		}
		e.inputState.Reset()
	case 'g':
		// For gg, we'd need another pending state, but for simplicity
		// just go to first line on single 'g'
		pane.GotoLine(1)
		e.inputState.Reset()

	// Word navigation
	case 'w':
		for i := 0; i < count; i++ {
			pane.MoveToNextWord()
		}
		e.inputState.Reset()
	case 'b':
		for i := 0; i < count; i++ {
			pane.MoveToPrevWord()
		}
		e.inputState.Reset()

	// Operators (start pending state)
	case 'd':
		e.inputState.PendingOperator = 'd'
		return false
	case 'y':
		e.inputState.PendingOperator = 'y'
		return false

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
		e.inputState.Reset()

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
		e.inputState.Reset()

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
		e.inputState.Reset()

	// Undo
	case 'u':
		e.undo()
		e.inputState.Reset()

	// Mark operations
	case 'm':
		e.inputState.PendingMark = true
		return false
	case '`':
		e.inputState.PendingJumpToMark = true
		return false

	default:
		e.inputState.Reset()
	}

	return false
}

// handlePendingOperator handles the second key after an operator (d, y)
func (e *Editor) handlePendingOperator(r rune) bool {
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
		// Record for undo - treat as deletion of full lines
		e.history.RecordDeleteLines(buf, startRow, deleted)
		// Clamp cursor after deletion
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
		// Restore cursor and delete range
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

	default:
		// Unknown operator combination, just reset
	}

	e.inputState.Reset()
	return false
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
		// Execute goto line
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
