package editor

import "github.com/gdamore/tcell/v2"

// openSearchWidget handles F4: open Search widget or cycle focus
func (e *Editor) openSearchWidget() {
	w := e.inputState.Widget
	if w == nil {
		e.createWidget(WidgetSearch)
		return
	}
	if w.Kind == WidgetSearch {
		e.cycleFocusSearch()
		return
	}
	// Currently FindReplace → switch to Search
	e.switchToWidget(WidgetSearch)
}

// openFindReplaceWidget handles F3: open FR widget or switch to it
func (e *Editor) openFindReplaceWidget() {
	w := e.inputState.Widget
	if w == nil {
		e.createWidget(WidgetFindReplace)
		return
	}
	if w.Kind == WidgetFindReplace {
		// Already active — Tab now handles cycling
		return
	}
	// Currently Search → switch to FindReplace
	e.switchToWidget(WidgetFindReplace)
}

// createWidget creates a brand-new WidgetState for the given kind
func (e *Editor) createWidget(kind WidgetKind) {
	pane := e.activePane()
	w := NewWidgetState(kind, pane.CursorRow, pane.CursorCol)
	e.inputState.Widget = w
}

// cycleFocusSearch toggles: FindBar → Editor → FindBar
func (e *Editor) cycleFocusSearch() {
	w := e.inputState.Widget
	switch w.Focus {
	case FocusFindBar:
		w.Focus = FocusEditor
	default:
		w.Focus = FocusFindBar
	}
}

// cycleFocusFindReplace cycles: FindBar → ReplaceBar → Editor → FindBar
func (e *Editor) cycleFocusFindReplace() {
	w := e.inputState.Widget
	switch w.Focus {
	case FocusFindBar:
		w.Focus = FocusReplaceBar
	case FocusReplaceBar:
		w.Focus = FocusEditor
	default:
		w.Focus = FocusFindBar
	}
}

// switchToWidget switches the active kind, preserving both sessions
func (e *Editor) switchToWidget(kind WidgetKind) {
	w := e.inputState.Widget
	pane := e.activePane()

	// Save current session's cursor
	if cur := w.CurrentSession(); cur != nil {
		cur.CursorRow = pane.CursorRow
		cur.CursorCol = pane.CursorCol
	}

	w.Kind = kind
	w.Focus = FocusFindBar

	// Lazy-create the target session if needed
	e.ensureSession(w)

	// Refresh matches from current buffer content
	e.refreshSessionMatches(w.CurrentSession())

	// Restore target session's cursor
	session := w.CurrentSession()
	if session != nil {
		pane.CursorRow = session.CursorRow
		pane.CursorCol = session.CursorCol
	}
}

// ensureSession lazy-creates the session for the current kind
func (e *Editor) ensureSession(w *WidgetState) {
	pane := e.activePane()
	switch w.Kind {
	case WidgetSearch:
		if w.SearchSession == nil {
			w.SearchSession = NewWidgetSession(pane.CursorRow, pane.CursorCol)
		}
	case WidgetFindReplace:
		if w.FindReplaceSession == nil {
			w.FindReplaceSession = NewWidgetSession(
				pane.CursorRow, pane.CursorCol,
			)
		}
	}
}

// refreshSessionMatches recalculates matches for a session
func (e *Editor) refreshSessionMatches(s *WidgetSession) {
	if s == nil || s.Query == "" {
		return
	}
	buf := e.activeBuffer()
	s.Matches = buf.FindAllMatches(s.Query)
	s.NoMatches = len(s.Matches) == 0
	if len(s.Matches) == 0 {
		s.CurrentIndex = -1
		return
	}
	// Clamp CurrentIndex
	if s.CurrentIndex >= len(s.Matches) {
		s.CurrentIndex = FindFirstMatchAfterCursor(
			s.Matches, s.CursorRow, s.CursorCol,
		)
	}
}

// handleWidgetMode dispatches key events when widget is active
func (e *Editor) handleWidgetMode(ev *tcell.EventKey) bool {
	w := e.inputState.Widget
	if w == nil {
		return false
	}

	// Global widget keys
	switch ev.Key() {
	case tcell.KeyEscape:
		e.closeWidget()
		return false
	case tcell.KeyF3:
		e.openFindReplaceWidget()
		return false
	case tcell.KeyF4:
		e.openSearchWidget()
		return false
	case tcell.KeyTab:
		if w.Kind == WidgetFindReplace {
			e.cycleFocusFindReplace()
		}
		return false
	}

	// Dispatch by focus
	switch w.Focus {
	case FocusFindBar:
		return e.handleWidgetFindBarKey(ev)
	case FocusReplaceBar:
		return e.handleWidgetReplaceBarKey(ev)
	case FocusEditor:
		return e.handleWidgetEditorKey(ev)
	}
	return false
}

// handleWidgetFindBarKey handles input in the Find bar
func (e *Editor) handleWidgetFindBarKey(ev *tcell.EventKey) bool {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil {
		return false
	}

	switch ev.Key() {
	case tcell.KeyEnter:
		// Enter confirms search in Search mode; no-op in FR mode
		if w.Kind == WidgetSearch {
			w.Focus = FocusEditor
		}
		return false
	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(session.Query) > 0 {
			runes := []rune(session.Query)
			session.Query = string(runes[:len(runes)-1])
			e.updateWidgetMatches()
		}
		return false
	case tcell.KeyRune:
		session.Query += string(ev.Rune())
		e.updateWidgetMatches()
		return false
	}
	return false
}

// handleWidgetReplaceBarKey handles input in the Replace bar
func (e *Editor) handleWidgetReplaceBarKey(ev *tcell.EventKey) bool {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil {
		return false
	}

	switch ev.Key() {
	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(session.ReplaceText) > 0 {
			runes := []rune(session.ReplaceText)
			session.ReplaceText = string(runes[:len(runes)-1])
		}
		return false
	case tcell.KeyRune:
		session.ReplaceText += string(ev.Rune())
		return false
	}
	return false
}

// handleWidgetEditorKey handles keys when focus is on editor
func (e *Editor) handleWidgetEditorKey(ev *tcell.EventKey) bool {
	w := e.inputState.Widget
	switch ev.Key() {
	case tcell.KeyEnter:
		if w.Kind == WidgetFindReplace {
			e.widgetReplaceCurrentMatch()
		}
		return false
	case tcell.KeyRune:
		switch ev.Rune() {
		case 'n':
			e.widgetNextMatch()
			return false
		case 'N':
			e.widgetPrevMatch()
			return false
		}
	}
	// Pass through to normal mode handling
	return false
}

// updateWidgetMatches recalculates matches for the current session
func (e *Editor) updateWidgetMatches() {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil {
		return
	}

	buf := e.activeBuffer()
	if session.Query == "" {
		session.Matches = nil
		session.CurrentIndex = -1
		session.NoMatches = false
		return
	}

	session.Matches = buf.FindAllMatches(session.Query)
	session.NoMatches = len(session.Matches) == 0

	if len(session.Matches) == 0 {
		session.CurrentIndex = -1
		return
	}

	// Auto-navigate at 2+ chars
	qLen := len([]rune(session.Query))
	if qLen >= 2 {
		e.widgetNavigateToMatch(session.CursorRow, session.CursorCol)
	}
}

// widgetNavigateToMatch finds and moves to the first match after position
func (e *Editor) widgetNavigateToMatch(row, col int) {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil || len(session.Matches) == 0 {
		return
	}
	idx := FindFirstMatchAfterCursor(session.Matches, row, col)
	session.CurrentIndex = idx
	if idx >= 0 {
		pane := e.activePane()
		match := session.Matches[idx]
		pane.CursorRow = match.Row
		pane.CursorCol = match.Col
	}
}

// widgetNavigateToCurrentMatch moves cursor to the current match
func (e *Editor) widgetNavigateToCurrentMatch() {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil {
		return
	}
	if session.CurrentIndex < 0 || session.CurrentIndex >= len(session.Matches) {
		return
	}
	pane := e.activePane()
	match := session.Matches[session.CurrentIndex]
	pane.CursorRow = match.Row
	pane.CursorCol = match.Col
}

// widgetNextMatch moves to the next match (wrapping)
func (e *Editor) widgetNextMatch() {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil || len(session.Matches) == 0 {
		return
	}
	session.CurrentIndex = (session.CurrentIndex + 1) % len(session.Matches)
	e.widgetNavigateToCurrentMatch()
}

// widgetPrevMatch moves to the previous match (wrapping)
func (e *Editor) widgetPrevMatch() {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil || len(session.Matches) == 0 {
		return
	}
	session.CurrentIndex--
	if session.CurrentIndex < 0 {
		session.CurrentIndex = len(session.Matches) - 1
	}
	e.widgetNavigateToCurrentMatch()
}

// widgetReplaceCurrentMatch replaces the current match and advances
func (e *Editor) widgetReplaceCurrentMatch() {
	w := e.inputState.Widget
	session := w.CurrentSession()
	if session == nil {
		return
	}
	if session.CurrentIndex < 0 || session.CurrentIndex >= len(session.Matches) {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	match := session.Matches[session.CurrentIndex]

	// Snapshot for undo
	oldLines := copyLines(buf.Lines)

	// Replace matched text
	line := buf.Lines[match.Row]
	endCol := match.Col + match.Length
	if endCol > len(line) {
		endCol = len(line)
	}
	replRunes := []rune(session.ReplaceText)
	newLine := make([]rune, 0, len(line)-(endCol-match.Col)+len(replRunes))
	newLine = append(newLine, line[:match.Col]...)
	newLine = append(newLine, replRunes...)
	newLine = append(newLine, line[endCol:]...)
	buf.Lines[match.Row] = newLine
	buf.Modified = true

	// Record undo
	e.history.Push(&Change{
		Type:    ChangeReplace,
		Buffer:  buf,
		Row:     match.Row,
		Col:     match.Col,
		Text:    copyLines(buf.Lines),
		OldText: oldLines,
	})
	e.scheduleAutoSave()

	// Save cursor position before recalculating
	cursorRow := pane.CursorRow
	cursorCol := pane.CursorCol

	// Recalculate matches
	session.Matches = buf.FindAllMatches(session.Query)
	session.NoMatches = len(session.Matches) == 0

	if len(session.Matches) > 0 {
		e.widgetNavigateToMatch(cursorRow, cursorCol)
	} else {
		session.CurrentIndex = -1
	}
}

// closeWidget restores anchor cursor and clears widget
func (e *Editor) closeWidget() {
	w := e.inputState.Widget
	if w == nil {
		return
	}
	pane := e.activePane()
	pane.CursorRow = w.AnchorRow
	pane.CursorCol = w.AnchorCol
	e.inputState.Widget = nil
}
