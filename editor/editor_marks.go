package editor

import "github.com/gdamore/tcell/v2"

// handleMarkInput handles input when waiting for a mark identifier
func (e *Editor) handleMarkInput(ev *tcell.EventKey) bool {
	// Cancel on Escape
	if ev.Key() == tcell.KeyEscape {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	// Only accept rune keys
	if ev.Key() != tcell.KeyRune {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	r := ev.Rune()

	// Validate identifier
	if !isValidMarkIdentifier(r) {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	if e.inputState.PendingMark {
		e.setMark(r)
		e.inputState.PendingMark = false
	} else if e.inputState.PendingJumpToMark {
		e.jumpToMark(r)
		e.inputState.PendingJumpToMark = false
	}

	return false
}

// setMark saves the current cursor position as a global mark with buffer reference
func (e *Editor) setMark(id rune) {
	pane := e.activePane()
	e.globalMarks[id] = GlobalMark{
		Buffer: pane.Buffer,
		Row:    pane.CursorRow,
		Col:    pane.CursorCol,
	}
}

// jumpToMark moves cursor to a previously set mark, with cross-file pane switching
func (e *Editor) jumpToMark(id rune) {
	mark, exists := e.globalMarks[id]
	if !exists {
		return // Mark doesn't exist, do nothing
	}

	// Find a pane viewing the mark's buffer
	paneIdx := -1
	for i, p := range e.panes {
		if p.Buffer == mark.Buffer {
			paneIdx = i
			break
		}
	}

	// Buffer no longer open - mark is invalid
	if paneIdx == -1 {
		return
	}

	// Switch pane if mark is in different pane
	if paneIdx != e.activePaneIdx {
		e.activePaneIdx = paneIdx
	}

	// Position cursor with clamping on the target pane
	pane := e.panes[paneIdx]
	buf := pane.Buffer

	// Clamp row to valid range
	row := mark.Row
	if row >= len(buf.Lines) {
		row = len(buf.Lines) - 1
	}
	if row < 0 {
		row = 0
	}

	// Clamp column to line length
	col := mark.Col
	if col > len(buf.Lines[row]) {
		col = len(buf.Lines[row])
	}
	if col < 0 {
		col = 0
	}

	pane.CursorRow = row
	pane.CursorCol = col
}
