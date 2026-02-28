package editor

import "github.com/gdamore/tcell/v2"

// handleVisualMode handles key events in visual mode (selection)
func (e *Editor) handleVisualMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	buf := pane.Buffer
	_, height := e.screen.Size()

	// Handle pending find character
	if e.inputState.PendingFindForward || e.inputState.PendingFindBackward {
		if ev.Key() == tcell.KeyEscape {
			e.inputState.Reset()
			return false
		}
		if ev.Key() == tcell.KeyRune {
			ch := ev.Rune()
			forward := e.inputState.PendingFindForward
			if forward {
				pane.FindCharForward(ch)
			} else {
				pane.FindCharBackward(ch)
			}
			e.inputState.SaveLastFind(ch, forward)
			e.inputState.Reset()
		}
		return false
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.mode = ModeNormal
		pane.ClearSelection()
		e.inputState.Reset()

	case tcell.KeyUp:
		pane.MoveUp()

	case tcell.KeyDown:
		pane.MoveDown()

	case tcell.KeyLeft:
		pane.MoveLeft()

	case tcell.KeyRight:
		pane.MoveRight()

	case tcell.KeyCtrlD:
		pane.PageDown(height)

	case tcell.KeyCtrlU:
		pane.PageUp(height)

	case tcell.KeyRune:
		switch ev.Rune() {
		case 'h':
			pane.MoveLeft()
		case 'j':
			pane.MoveDown()
		case 'k':
			pane.MoveUp()
		case 'l':
			pane.MoveRight()
		case '0':
			pane.MoveToLineStart()
		case '$':
			pane.MoveToLineEnd()
		case 'v':
			// Exit visual mode
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
		case 'G':
			pane.GotoLine(len(buf.Lines))
		case 'g':
			pane.GotoLine(1)
		case 'w':
			pane.MoveToNextWord()
		case 'b':
			pane.MoveToPrevWord()

		// Find character in line
		case 'f':
			e.inputState.PendingFindForward = true
		case 'F':
			e.inputState.PendingFindBackward = true
		case ';':
			// Repeat last find
			if e.inputState.HasLastFind {
				if e.inputState.LastFindForward {
					pane.FindCharForward(e.inputState.LastFindChar)
				} else {
					pane.FindCharBackward(e.inputState.LastFindChar)
				}
			}
		case ',':
			// Repeat last find in reverse
			if e.inputState.HasLastFind {
				if e.inputState.LastFindForward {
					pane.FindCharBackward(e.inputState.LastFindChar)
				} else {
					pane.FindCharForward(e.inputState.LastFindChar)
				}
			}

		// Copy selection
		case 'y':
			startRow, startCol, endRow, endCol := pane.GetSelection()
			e.clipboard = buf.GetRange(startRow, startCol, endRow, endCol)
			e.clipboardLine = false
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()

		// Delete selection (d and x both delete selection in visual mode)
		case 'd', 'x':
			startRow, startCol, endRow, endCol := pane.GetSelection()
			e.clipboard = buf.DeleteRange(startRow, startCol, endRow, endCol+1)
			e.clipboardLine = false
			e.history.RecordDelete(buf, startRow, startCol, e.clipboard)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Indent selection
		case '>':
			startRow, _, endRow, _ := pane.GetSelection()
			buf.IndentRange(startRow, endRow)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Unindent selection
		case '<':
			startRow, _, endRow, _ := pane.GetSelection()
			buf.UnindentRange(startRow, endRow)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Paste (replace selection)
		case 'p', 'P':
			if len(e.clipboard) > 0 {
				// Take snapshot for undo
				oldLines := copyLines(buf.Lines)
				startRow, startCol, endRow, endCol := pane.GetSelection()

				buf.DeleteRange(startRow, startCol, endRow, endCol+1)
				pane.ClearSelection()
				e.mode = ModeNormal
				// Now paste at cursor
				var fr, lr int
				if ev.Rune() == 'p' {
					fr, lr = e.pasteAfter()
				} else {
					fr, lr = e.pasteBefore()
				}
				e.reindentPastedRange(buf, fr, lr)

				// Record the combined delete+paste operation for undo
				e.history.Push(&Change{
					Type:    ChangeReplace,
					Buffer:  buf,
					Row:     startRow,
					Col:     startCol,
					Text:    copyLines(buf.Lines),
					OldText: oldLines,
				})
				e.scheduleAutoSave()
			}
			e.inputState.Reset()
		}
	}

	return false
}
