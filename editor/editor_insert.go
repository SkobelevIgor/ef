package editor

import (
	"time"

	"github.com/gdamore/tcell/v2"
)

func (e *Editor) reindentPastedRange(buf *Buffer, firstRow, lastRow int) {
	if firstRow < 0 || lastRow < 0 || firstRow > lastRow {
		return
	}
	if buf.FileType == "" || !buf.Config.AutoIndentation {
		return
	}
	contextIndent := buf.GetPrevNonEmptyLineIndent(firstRow)
	buf.ReindentLines(firstRow, lastRow, contextIndent)
}

// startInsertSession records the cursor position when entering insert mode
func (e *Editor) startInsertSession() {
	pane := e.activePane()
	e.insertStartRow = pane.CursorRow
	e.insertStartCol = pane.CursorCol
}

// enterInsertMode performs the common sequence for entering insert mode:
// SyncToBuffer, start history session, set mode, record session start.
func (e *Editor) enterInsertMode() {
	pane := e.activePane()
	buf := pane.Buffer
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
	e.mode = ModeInsert
	e.startInsertSession()
}

// deleteCharUnderCursorCmd executes the 'x' command: delete char at cursor as a single undo unit.
func (e *Editor) deleteCharUnderCursorCmd() {
	pane := e.activePane()
	buf := pane.Buffer
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
	buf.DeleteCharAt(pane.CursorRow, pane.CursorCol)
	pane.clampCursorCol()
	e.history.CommitSession(buf.Lines)
	e.scheduleAutoSave()
}

// endInsertSession reindents lines if multiple lines were created during the session.
// Single-line sessions are skipped — they already have correct indent from o/O/Enter.
func (e *Editor) endInsertSession() {
	pane := e.activePane()
	buf := pane.Buffer
	endRow := pane.CursorRow
	startRow := e.insertStartRow

	// Only reindent multi-line sessions
	if endRow <= startRow || startRow < 0 {
		return
	}

	if buf.FileType == "" || !buf.Config.AutoIndentation {
		return
	}

	contextIndent := buf.GetPrevNonEmptyLineIndent(startRow)
	buf.ReindentLines(startRow, endRow, contextIndent)

	// Clamp cursor column to the current line length after reindent
	lineLen := len(buf.Lines[pane.CursorRow])
	if pane.CursorCol > lineLen {
		pane.CursorCol = lineLen
	}
}

// ReindentEvent is a custom tcell event for debounced reindentation
type ReindentEvent struct {
	when time.Time
}

// When returns the time when the event was created
func (e *ReindentEvent) When() time.Time {
	return e.when
}

// reindentInsertSession reindents pasted text without leaving insert mode.
// Only activates for multi-line insert sessions (i.e. pastes). Single-line
// edits already have correct indentation from o/O/Enter.
func (e *Editor) reindentInsertSession() {
	pane := e.activePane()
	buf := pane.Buffer
	cursorRow := pane.CursorRow
	startRow := e.insertStartRow

	// Only reindent multi-line sessions (paste). Single-line typing
	// already has correct indent from o/O/InsertNewlineWithIndent.
	if cursorRow <= startRow || startRow < 0 {
		return
	}

	if buf.FileType == "" || !buf.Config.AutoIndentation {
		return
	}

	endRow := cursorRow
	// If cursor row is whitespace-only, exclude it — the user hasn't
	// typed real content there yet (e.g. paste ended with a newline).
	if !hasContent(buf.Lines[cursorRow]) {
		endRow = cursorRow - 1
	}

	if startRow > endRow {
		return
	}

	contextIndent := buf.GetPrevNonEmptyLineIndent(startRow)

	if endRow == cursorRow {
		// Cursor row included — adjust cursor col for indent change
		oldLen := len(buf.Lines[cursorRow])
		buf.ReindentLines(startRow, endRow, contextIndent)
		newLen := len(buf.Lines[cursorRow])
		pane.CursorCol += newLen - oldLen
		if pane.CursorCol < 0 {
			pane.CursorCol = 0
		}
		if pane.CursorCol > newLen {
			pane.CursorCol = newLen
		}
	} else {
		buf.ReindentLines(startRow, endRow, contextIndent)
	}

}

// handleInsertMode handles key events in insert mode (text editing)
func (e *Editor) handleInsertMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	buf := pane.Buffer
	ac := e.inputState.Autocomplete

	// Handle autocomplete keys when dropdown is visible
	if ac != nil && ac.Active {
		switch ev.Key() {
		case tcell.KeyTab:
			e.acceptAutocomplete()
			return false
		case tcell.KeyEnter:
			// If the suggestion matches what's already typed, dismiss autocomplete
			// and let Enter create a newline
			sel := ac.Selected()
			if sel != nil && sel.Word != ac.Prefix {
				e.acceptAutocomplete()
				return false
			}
			// Suggestion matches prefix — dismiss and fall through to Enter handler
			e.inputState.Autocomplete = nil
		case tcell.KeyCtrlN, tcell.KeyDown:
			ac.Next()
			return false
		case tcell.KeyCtrlP, tcell.KeyUp:
			ac.Prev()
			return false
		}
		// Note: Escape falls through to main handler to also exit insert mode
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.endInsertSession()
		e.inputState.Autocomplete = nil
		e.mode = ModeNormal
		e.history.CommitSession(buf.Lines)
		e.inputState.Reset()
		// Move cursor back one position like vim
		if pane.CursorCol > 0 {
			pane.CursorCol--
		}

	case tcell.KeyUp:
		// Only move cursor if autocomplete is not active (it handles Up itself)
		if ac == nil || !ac.Active {
			pane.MoveUp()
		}

	case tcell.KeyDown:
		// Only move cursor if autocomplete is not active (it handles Down itself)
		if ac == nil || !ac.Active {
			pane.MoveDown()
		}

	case tcell.KeyLeft:
		e.inputState.Autocomplete = nil
		pane.MoveLeft()

	case tcell.KeyRight:
		e.inputState.Autocomplete = nil
		pane.MoveRight()

	case tcell.KeyBackspace, tcell.KeyBackspace2:
		pane.CursorRow, pane.CursorCol = buf.DeleteChar(pane.CursorRow, pane.CursorCol)
		e.scheduleAutoSave()
		e.triggerAutocomplete()

	case tcell.KeyDelete:
		e.inputState.Autocomplete = nil
		buf.DeleteCharForward(pane.CursorRow, pane.CursorCol)
		e.scheduleAutoSave()

	case tcell.KeyEnter:
		e.inputState.Autocomplete = nil
		pane.CursorRow, pane.CursorCol = buf.InsertNewlineWithIndent(pane.CursorRow, pane.CursorCol)
		e.scheduleAutoSave()

	case tcell.KeyCtrlD:
		e.inputState.Autocomplete = nil
		_, height := e.screen.Size()
		pane.PageDown(height)

	case tcell.KeyCtrlU:
		e.inputState.Autocomplete = nil
		_, height := e.screen.Size()
		pane.PageUp(height)

	case tcell.KeyRune:
		pane.CursorCol = buf.InsertChar(pane.CursorRow, pane.CursorCol, ev.Rune())
		e.scheduleAutoSave()
		e.triggerAutocomplete()

	case tcell.KeyTab:
		pane.CursorCol = buf.InsertTab(pane.CursorRow, pane.CursorCol)
		e.scheduleAutoSave()
		e.triggerAutocomplete()
	}

	return false
}
