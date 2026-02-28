package editor

import "github.com/gdamore/tcell/v2"

// enterSearchMode initializes search mode with current cursor position as CursorZero
// If a previous confirmed search exists, restore the query for immediate navigation
// If already in search mode with a confirmed search, F4 returns focus to the search bar
func (e *Editor) enterSearchMode() {
	// If search mode is already active and confirmed, return focus to query editing (FR-026)
	if e.inputState.Search != nil && e.inputState.Search.Active && e.inputState.Search.Confirmed {
		e.inputState.Search.Confirmed = false
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	search := NewSearchState(pane.CursorRow, pane.CursorCol)

	// Restore previous query if available (FR-024)
	if e.lastSearchQuery != "" {
		search.Query = e.lastSearchQuery
		search.Confirmed = true // Enable immediate n/N navigation (FR-025)

		// Calculate matches for restored query (T047)
		search.Matches = buf.FindAllMatches(search.Query)
		search.NoMatches = len(search.Matches) == 0

		// Find first match after current cursor (T048)
		if len(search.Matches) > 0 {
			search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, pane.CursorRow, pane.CursorCol)
			if search.CurrentIndex >= 0 {
				match := search.Matches[search.CurrentIndex]
				pane.CursorRow = match.Row
				pane.CursorCol = match.Col
			}
		}
	}

	e.inputState.Search = search
}

// handleSearchMode processes key events while in search mode
func (e *Editor) handleSearchMode(ev *tcell.EventKey) bool {
	search := e.inputState.Search
	if search == nil {
		return false
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.exitSearchMode()
		return false

	case tcell.KeyEnter:
		if !search.Confirmed {
			e.confirmSearch()
		} else if search.IsReplaceMode {
			e.replaceCurrentMatch()
		}
		return false

	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(search.Query) > 0 {
			search.Query = search.Query[:len(search.Query)-1]
			e.updateSearchMatches()
		}
		return false

	case tcell.KeyRune:
		if search.Confirmed {
			// Handle n/N navigation after search confirmed
			switch ev.Rune() {
			case 'n':
				e.nextMatch()
			case 'N':
				e.prevMatch()
			}
		} else {
			// Accumulate query characters
			search.Query += string(ev.Rune())
			e.updateSearchMatches()
		}
		return false
	}

	return false
}

// updateSearchMatches recalculates matches based on current query
func (e *Editor) updateSearchMatches() {
	search := e.inputState.Search
	if search == nil {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	if search.Query == "" {
		search.Matches = nil
		search.CurrentIndex = -1
		search.NoMatches = false
		return
	}

	search.Matches = buf.FindAllMatches(search.Query)
	search.NoMatches = len(search.Matches) == 0

	// Find first match after CursorZero for incremental search highlighting
	if len(search.Matches) > 0 {
		search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, search.CursorZeroRow, search.CursorZeroCol)
		// Move cursor to show incremental match
		if search.CurrentIndex >= 0 {
			match := search.Matches[search.CurrentIndex]
			pane.CursorRow = match.Row
			pane.CursorCol = match.Col
		}
	} else {
		search.CurrentIndex = -1
	}
}

// confirmSearch confirms the search query and enables n/N navigation
func (e *Editor) confirmSearch() {
	search := e.inputState.Search
	if search == nil || search.Query == "" {
		return
	}

	// Parse for replace syntax
	query, replacement, isReplace := ParseQuery(search.Query)
	if isReplace {
		search.Query = query
		search.IsReplaceMode = true
		search.ReplaceText = replacement
		// Recalculate matches with the actual search query
		e.updateSearchMatches()
	}

	search.Confirmed = true

	// Navigate to first match after CursorZero
	if len(search.Matches) > 0 && search.CurrentIndex >= 0 {
		e.navigateToCurrentMatch()
	}
}

// navigateToCurrentMatch moves cursor to the current match position
func (e *Editor) navigateToCurrentMatch() {
	search := e.inputState.Search
	if search == nil || search.CurrentIndex < 0 || search.CurrentIndex >= len(search.Matches) {
		return
	}

	pane := e.activePane()
	match := search.Matches[search.CurrentIndex]
	pane.CursorRow = match.Row
	pane.CursorCol = match.Col
}

// nextMatch moves to the next match (wrapping at end)
func (e *Editor) nextMatch() {
	search := e.inputState.Search
	if search == nil || len(search.Matches) == 0 {
		return
	}

	search.CurrentIndex = (search.CurrentIndex + 1) % len(search.Matches)
	e.navigateToCurrentMatch()
}

// prevMatch moves to the previous match (wrapping at start)
func (e *Editor) prevMatch() {
	search := e.inputState.Search
	if search == nil || len(search.Matches) == 0 {
		return
	}

	search.CurrentIndex--
	if search.CurrentIndex < 0 {
		search.CurrentIndex = len(search.Matches) - 1
	}
	e.navigateToCurrentMatch()
}

// exitSearchMode exits search mode and positions cursor appropriately
func (e *Editor) exitSearchMode() {
	search := e.inputState.Search
	if search == nil {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer

	// Save query for restoration on F4 re-entry (only if confirmed)
	if search.Confirmed && search.Query != "" {
		e.lastSearchQuery = search.Query
	}

	// If search was confirmed and we have a match, position cursor at end of match
	if search.Confirmed && search.CurrentIndex >= 0 && search.CurrentIndex < len(search.Matches) {
		match := search.Matches[search.CurrentIndex]
		pane.CursorRow = match.Row
		pane.CursorCol = match.Col + match.Length
		// Clamp to line bounds
		if pane.CursorCol > len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol = len(buf.Lines[pane.CursorRow])
		}
	} else {
		// Return to CursorZero (cancelled or no match)
		pane.CursorRow = search.CursorZeroRow
		pane.CursorCol = search.CursorZeroCol
	}

	// Clear search state
	e.inputState.Search = nil
}

// replaceCurrentMatch replaces the current match with replacement text
func (e *Editor) replaceCurrentMatch() {
	search := e.inputState.Search
	if search == nil || !search.IsReplaceMode {
		return
	}
	if search.CurrentIndex < 0 || search.CurrentIndex >= len(search.Matches) {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	match := search.Matches[search.CurrentIndex]

	// Snapshot for undo - get the text being replaced
	oldLines := copyLines(buf.Lines)

	// Position cursor at match start
	pane.CursorRow = match.Row
	pane.CursorCol = match.Col

	// Delete the matched text
	line := buf.Lines[match.Row]
	endCol := match.Col + match.Length
	if endCol > len(line) {
		endCol = len(line)
	}
	newLine := make([]rune, len(line)-(endCol-match.Col)+len([]rune(search.ReplaceText)))
	copy(newLine[:match.Col], line[:match.Col])
	copy(newLine[match.Col:], []rune(search.ReplaceText))
	copy(newLine[match.Col+len([]rune(search.ReplaceText)):], line[endCol:])
	buf.Lines[match.Row] = newLine
	buf.Modified = true

	// Record in undo history as individual change
	e.history.Push(&Change{
		Type:    ChangeReplace,
		Buffer:  buf,
		Row:     match.Row,
		Col:     match.Col,
		Text:    copyLines(buf.Lines),
		OldText: oldLines,
	})

	e.scheduleAutoSave()

	// Save current cursor position before recalculating matches
	cursorRow := pane.CursorRow
	cursorCol := pane.CursorCol

	// Recalculate matches after replacement
	search.Matches = buf.FindAllMatches(search.Query)
	search.NoMatches = len(search.Matches) == 0

	// Find next match after current cursor position (not CursorZero)
	if len(search.Matches) > 0 {
		search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, cursorRow, cursorCol)
		if search.CurrentIndex >= 0 {
			e.navigateToCurrentMatch()
		}
	} else {
		search.CurrentIndex = -1
	}
}
