package editor

import (
	"time"

	"github.com/gdamore/tcell/v2"
)

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

// ReindentEvent is a custom tcell event for debounced reindentation
type ReindentEvent struct {
	when time.Time
}

// When returns the time when the event was created
func (e *ReindentEvent) When() time.Time {
	return e.when
}

// EventMappingTimeout is a custom tcell event posted after MappingTimeout elapses.
// The snapshot field captures mapKeyTime at goroutine launch for race-safe comparison.
type EventMappingTimeout struct {
	when     time.Time
	snapshot time.Time // mapKeyTime when the timeout goroutine was launched
}

// When returns the time when the event was created
func (e *EventMappingTimeout) When() time.Time {
	return e.when
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
