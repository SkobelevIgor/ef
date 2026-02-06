package editor

import (
	"fmt"
	"os"
	"syscall"

	"github.com/gdamore/tcell/v2"
)

// Screen handles terminal rendering
type Screen struct {
	screen tcell.Screen
}

// PaneLayout represents the computed layout for a single pane viewport
// This is calculated during rendering based on screen size and split mode
type PaneLayout struct {
	PaneIndex int // Index into Editor.panes slice
	StartX    int // Screen X coordinate of pane's top-left corner
	StartY    int // Screen Y coordinate of pane's top-left corner
	Width     int // Pane width in characters
	Height    int // Pane height in lines
}

// NewScreen creates and initializes a new screen
func NewScreen() (*Screen, error) {
	s, err := tcell.NewScreen()
	if err != nil {
		return nil, err
	}

	if err := s.Init(); err != nil {
		return nil, err
	}

	s.SetStyle(tcell.StyleDefault.Background(tcell.ColorReset).Foreground(tcell.ColorReset))
	s.Clear()

	return &Screen{screen: s}, nil
}

// Close shuts down the screen
func (s *Screen) Close() {
	s.screen.Fini()
}

// Suspend suspends the screen and sends SIGTSTP to move process to background
func (s *Screen) Suspend() error {
	s.screen.Fini()
	// Send SIGTSTP to suspend the process
	return syscall.Kill(os.Getpid(), syscall.SIGTSTP)
}

// Resume reinitializes the screen after being resumed from background
func (s *Screen) Resume() error {
	// Create a fresh screen to avoid state issues after Fini()
	newScreen, err := tcell.NewScreen()
	if err != nil {
		return err
	}
	if err := newScreen.Init(); err != nil {
		return err
	}
	s.screen = newScreen
	s.screen.SetStyle(tcell.StyleDefault.Background(tcell.ColorReset).Foreground(tcell.ColorReset))
	s.screen.Clear()
	s.screen.Sync()
	return nil
}

// Size returns the screen dimensions
func (s *Screen) Size() (width, height int) {
	return s.screen.Size()
}

// PollEvent waits for and returns the next event
func (s *Screen) PollEvent() tcell.Event {
	return s.screen.PollEvent()
}

// PostEvent injects a custom event into the event queue
func (s *Screen) PostEvent(ev tcell.Event) error {
	return s.screen.PostEvent(ev)
}

// calculatePaneLayoutHorizontal computes pane positions for horizontal (stacked) splits
func calculatePaneLayoutHorizontal(numPanes, width, height, contentStartY int) []PaneLayout {
	if numPanes == 0 {
		return nil
	}

	// Calculate available height (excluding separators between panes)
	numSeparators := numPanes - 1
	availableHeight := height - numSeparators

	// Calculate base pane height and distribute remainder
	baseHeight := availableHeight / numPanes
	remainder := availableHeight % numPanes

	// Enforce minimum height
	visiblePanes := numPanes
	for baseHeight < MinPaneHeightHorizontal && visiblePanes > 1 {
		visiblePanes--
		numSeparators = visiblePanes - 1
		availableHeight = height - numSeparators
		baseHeight = availableHeight / visiblePanes
		remainder = availableHeight % visiblePanes
	}

	layouts := make([]PaneLayout, visiblePanes)
	currentY := contentStartY

	for i := 0; i < visiblePanes; i++ {
		paneHeight := baseHeight
		// Distribute remainder to last panes
		if i >= visiblePanes-remainder {
			paneHeight++
		}

		layouts[i] = PaneLayout{
			PaneIndex: i,
			StartX:    0,
			StartY:    currentY,
			Width:     width,
			Height:    paneHeight,
		}

		currentY += paneHeight + 1 // +1 for separator
	}

	return layouts
}

// calculatePaneLayoutVertical computes pane positions for vertical (side-by-side) splits
func calculatePaneLayoutVertical(numPanes, width, height, contentStartY int) []PaneLayout {
	if numPanes == 0 {
		return nil
	}

	// Calculate available width (excluding separators between panes)
	numSeparators := numPanes - 1
	availableWidth := width - numSeparators

	// Calculate base pane width and distribute remainder
	baseWidth := availableWidth / numPanes
	remainder := availableWidth % numPanes

	// Enforce minimum width
	visiblePanes := numPanes
	for baseWidth < MinPaneWidthVertical && visiblePanes > 1 {
		visiblePanes--
		numSeparators = visiblePanes - 1
		availableWidth = width - numSeparators
		baseWidth = availableWidth / visiblePanes
		remainder = availableWidth % visiblePanes
	}

	layouts := make([]PaneLayout, visiblePanes)
	currentX := 0

	for i := 0; i < visiblePanes; i++ {
		paneWidth := baseWidth
		// Distribute remainder to last panes
		if i >= visiblePanes-remainder {
			paneWidth++
		}

		layouts[i] = PaneLayout{
			PaneIndex: i,
			StartX:    currentX,
			StartY:    contentStartY,
			Width:     paneWidth,
			Height:    height,
		}

		currentX += paneWidth + 1 // +1 for separator
	}

	return layouts
}

// renderHorizontalSeparator draws a horizontal line between panes
func (s *Screen) renderHorizontalSeparator(y, width int) {
	separatorStyle := tcell.StyleDefault.Foreground(tcell.ColorGray)
	for x := 0; x < width; x++ {
		s.screen.SetContent(x, y, '─', nil, separatorStyle)
	}
}

// renderVerticalSeparator draws a vertical line between panes
func (s *Screen) renderVerticalSeparator(x, startY, height int) {
	separatorStyle := tcell.StyleDefault.Foreground(tcell.ColorGray)
	for y := startY; y < startY+height; y++ {
		s.screen.SetContent(x, y, '│', nil, separatorStyle)
	}
}

// Render draws the pane content
func (s *Screen) Render(panes []*Pane, activePaneIdx int, mode Mode, inputState *InputState, splitMode SplitMode) {
	s.screen.Clear()
	width, height := s.screen.Size()

	// Check if search is active
	searchActive := inputState.Search != nil && inputState.Search.Active
	contentStartY := 0

	if searchActive {
		s.renderSearchBar(inputState.Search, width)
		contentStartY = 1
		height-- // Reduce available height for content
	}

	// Calculate pane layout based on split mode
	var layouts []PaneLayout
	if splitMode == SplitVertical {
		layouts = calculatePaneLayoutVertical(len(panes), width, height, contentStartY)
	} else {
		layouts = calculatePaneLayoutHorizontal(len(panes), width, height, contentStartY)
	}

	// Clamp activePaneIdx to visible panes
	if activePaneIdx >= len(layouts) {
		activePaneIdx = len(layouts) - 1
	}
	if activePaneIdx < 0 {
		activePaneIdx = 0
	}

	// Render all panes
	for i, layout := range layouts {
		if layout.PaneIndex >= len(panes) {
			continue
		}
		pane := panes[layout.PaneIndex]

		// Adjust scroll for this pane
		lineNumWidth := getLineNumberWidthFromPane(pane)
		textWidth := layout.Width - lineNumWidth
		if textWidth < 1 {
			textWidth = 1
		}
		pane.AdjustScroll(textWidth, layout.Height)

		// Determine mode for this pane (only active pane shows current mode)
		paneMode := ModeNormal
		if i == activePaneIdx {
			paneMode = mode
		}

		// Render the pane
		s.renderPaneWithSearch(pane, layout.StartX, layout.StartY, layout.Width, layout.Height, paneMode, inputState.Search)

		// Draw separator after this pane (if not the last pane)
		if i < len(layouts)-1 {
			if splitMode == SplitVertical {
				// Vertical separator to the right of this pane
				s.renderVerticalSeparator(layout.StartX+layout.Width, layout.StartY, layout.Height)
			} else {
				// Horizontal separator below this pane
				s.renderHorizontalSeparator(layout.StartY+layout.Height, width)
			}
		}
	}

	// Position cursor
	if searchActive && !inputState.Search.Confirmed {
		// Cursor in search bar at end of query
		prompt := "Search: "
		if inputState.Search.IsReplaceMode {
			prompt = "Replace: "
		}
		cursorX := len(prompt) + len(inputState.Search.Query)
		s.screen.ShowCursor(cursorX, 0)
	} else if activePaneIdx < len(layouts) && activePaneIdx < len(panes) {
		// Cursor in active pane
		activePaneLayout := layouts[activePaneIdx]
		activePane := panes[activePaneIdx]
		lineNumWidth := getLineNumberWidthFromPane(activePane)

		cursorX, cursorY := getCursorScreenPosFromPane(activePane, activePaneLayout.Width, lineNumWidth)
		cursorX += activePaneLayout.StartX
		cursorY += activePaneLayout.StartY
		s.screen.ShowCursor(cursorX, cursorY)

		// Render autocomplete dropdown if active (constrained to pane)
		if inputState.Autocomplete != nil && inputState.Autocomplete.Active {
			// Calculate max Y for autocomplete (bottom of active pane)
			maxY := activePaneLayout.StartY + activePaneLayout.Height
			s.renderAutocomplete(inputState.Autocomplete, cursorX, cursorY, maxY)
		}
	}

	s.screen.Show()
}

// renderSearchBar renders the search input bar at the top of the screen
func (s *Screen) renderSearchBar(search *SearchState, width int) {
	style := tcell.StyleDefault.Background(tcell.ColorDarkBlue).Foreground(tcell.ColorWhite)

	// Clear the row
	for x := 0; x < width; x++ {
		s.screen.SetContent(x, 0, ' ', nil, style)
	}

	// Determine prompt text
	prompt := "Search: "
	if search.IsReplaceMode {
		prompt = "Replace: "
	}

	// Draw prompt
	x := 0
	for _, ch := range prompt {
		if x < width {
			s.screen.SetContent(x, 0, ch, nil, style)
			x++
		}
	}

	// Draw query text
	for _, ch := range search.Query {
		if x < width {
			s.screen.SetContent(x, 0, ch, nil, style)
			x++
		}
	}

	// Show "No matches" feedback if query has no results
	if search.NoMatches && search.Query != "" {
		feedback := " (No matches)"
		feedbackStyle := style.Foreground(tcell.ColorRed)
		for _, ch := range feedback {
			if x < width {
				s.screen.SetContent(x, 0, ch, nil, feedbackStyle)
				x++
			}
		}
	} else if len(search.Matches) > 0 {
		// Show match count
		matchInfo := fmt.Sprintf(" [%d/%d]", search.CurrentIndex+1, len(search.Matches))
		for _, ch := range matchInfo {
			if x < width {
				s.screen.SetContent(x, 0, ch, nil, style)
				x++
			}
		}
	}
}

// lineNumberWidth calculates the width needed for line numbers
func lineNumberWidth(totalLines int) int {
	width := 1
	for totalLines >= 10 {
		totalLines /= 10
		width++
	}
	if width < 3 {
		width = 3 // Minimum width for aesthetics
	}
	return width + 1 // +1 for space after number
}

// renderPane draws a buffer in a specific area of the screen with line wrapping
func (s *Screen) renderPane(buf *Buffer, startX, startY, width, height int, mode Mode) {
	lineNumWidth := lineNumberWidth(len(buf.Lines))
	lineNumStyle := tcell.StyleDefault.Foreground(tcell.ColorGreen)
	wrapStyle := tcell.StyleDefault.Foreground(tcell.ColorDarkGray)
	selectionStyle := tcell.StyleDefault.Reverse(true)

	// Mode-specific style for current line number only
	var currentLineNumStyle tcell.Style
	switch mode {
	case ModeNormal:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorBlue).Foreground(tcell.ColorWhite).Bold(true)
	case ModeInsert:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorGreen).Foreground(tcell.ColorBlack).Bold(true)
	case ModeVisual:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorPurple).Foreground(tcell.ColorWhite).Bold(true)
	}

	textWidth := width - lineNumWidth
	if textWidth < 1 {
		textWidth = 1
	}

	screenRow := 0
	for lineIdx := buf.ScrollOffset; lineIdx < len(buf.Lines) && screenRow < height; lineIdx++ {
		line := buf.Lines[lineIdx]
		isCurrentLine := lineIdx == buf.CursorRow

		// Calculate relative line number
		var lineNum int
		var lineNumStyleToUse tcell.Style
		if isCurrentLine {
			lineNum = lineIdx + 1 // Absolute line number for current line
			lineNumStyleToUse = currentLineNumStyle
		} else {
			lineNum = lineIdx - buf.CursorRow
			if lineNum < 0 {
				lineNum = -lineNum
			}
			lineNumStyleToUse = lineNumStyle
		}

		// Handle empty lines
		if len(line) == 0 {
			// Draw line number
			numStr := fmt.Sprintf("%*d ", lineNumWidth-1, lineNum)
			for i, ch := range numStr {
				if startX+i < startX+width {
					s.screen.SetContent(startX+i, startY+screenRow, ch, nil, lineNumStyleToUse)
				}
			}
			screenRow++
			continue
		}

		// Draw line with wrapping
		charIdx := 0
		isFirstWrap := true
		for charIdx < len(line) && screenRow < height {
			// Draw line number or wrap indicator
			if isFirstWrap {
				numStr := fmt.Sprintf("%*d ", lineNumWidth-1, lineNum)
				for i, ch := range numStr {
					if startX+i < startX+width {
						s.screen.SetContent(startX+i, startY+screenRow, ch, nil, lineNumStyleToUse)
					}
				}
				isFirstWrap = false
			} else {
				// Draw wrap continuation indicator
				wrapIndicator := fmt.Sprintf("%*s ", lineNumWidth-1, "↪")
				wrapStyleToUse := wrapStyle
				if isCurrentLine {
					wrapStyleToUse = currentLineNumStyle
				}
				for i, ch := range wrapIndicator {
					if startX+i < startX+width {
						s.screen.SetContent(startX+i, startY+screenRow, ch, nil, wrapStyleToUse)
					}
				}
			}

			// Get syntax highlighting tokens for this line (if available)
			var tokens []Token
			if buf.HighlightCache != nil {
				tokens = buf.HighlightCache.GetTokens(lineIdx, line, buf.Lines)
			}

			// Get tab stop width from buffer config
			tabStop := buf.Config.TabStop
			if tabStop <= 0 {
				tabStop = 4
			}

			// Draw text for this screen row
			// col = visual column on screen, charIdx = index into line runes
			textStart := startX + lineNumWidth
			col := 0
			for col < textWidth && charIdx < len(line) {
				ch := line[charIdx]
				charStyle := tcell.StyleDefault

				// Apply syntax highlighting
				if len(tokens) > 0 {
					if style, ok := GetStyleAt(tokens, charIdx); ok {
						charStyle = style
					}
				}

				// Selection takes precedence over syntax highlighting
				if buf.IsInSelection(lineIdx, charIdx) {
					charStyle = selectionStyle
				}

				if ch == '\t' {
					// Expand tab to spaces up to next tab stop
					spacesToNextStop := tabStop - (col % tabStop)
					for i := 0; i < spacesToNextStop && col < textWidth; i++ {
						s.screen.SetContent(textStart+col, startY+screenRow, ' ', nil, charStyle)
						col++
					}
				} else {
					s.screen.SetContent(textStart+col, startY+screenRow, ch, nil, charStyle)
					col++
				}
				charIdx++
			}
			screenRow++
		}
	}
}

// renderPaneWithSearch draws a pane with optional search match highlighting
func (s *Screen) renderPaneWithSearch(pane *Pane, startX, startY, width, height int, mode Mode, search *SearchState) {
	buf := pane.Buffer
	lineNumWidth := lineNumberWidth(len(buf.Lines))
	lineNumStyle := tcell.StyleDefault.Foreground(tcell.ColorGreen)
	wrapStyle := tcell.StyleDefault.Foreground(tcell.ColorDarkGray)
	selectionStyle := tcell.StyleDefault.Reverse(true)
	searchMatchStyle := tcell.StyleDefault.Background(tcell.ColorYellow).Foreground(tcell.ColorBlack)
	currentMatchStyle := tcell.StyleDefault.Background(tcell.ColorOrange).Foreground(tcell.ColorBlack).Bold(true)

	// Mode-specific style for current line number only
	var currentLineNumStyle tcell.Style
	switch mode {
	case ModeNormal:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorBlue).Foreground(tcell.ColorWhite).Bold(true)
	case ModeInsert:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorGreen).Foreground(tcell.ColorBlack).Bold(true)
	case ModeVisual:
		currentLineNumStyle = tcell.StyleDefault.Background(tcell.ColorPurple).Foreground(tcell.ColorWhite).Bold(true)
	}

	textWidth := width - lineNumWidth
	if textWidth < 1 {
		textWidth = 1
	}

	screenRow := 0
	for lineIdx := pane.ScrollOffset; lineIdx < len(buf.Lines) && screenRow < height; lineIdx++ {
		line := buf.Lines[lineIdx]
		isCurrentLine := lineIdx == pane.CursorRow

		// Calculate relative line number
		var lineNum int
		var lineNumStyleToUse tcell.Style
		if isCurrentLine {
			lineNum = lineIdx + 1 // Absolute line number for current line
			lineNumStyleToUse = currentLineNumStyle
		} else {
			lineNum = lineIdx - pane.CursorRow
			if lineNum < 0 {
				lineNum = -lineNum
			}
			lineNumStyleToUse = lineNumStyle
		}

		// Handle empty lines
		if len(line) == 0 {
			// Draw line number
			numStr := fmt.Sprintf("%*d ", lineNumWidth-1, lineNum)
			for i, ch := range numStr {
				if startX+i < startX+width {
					s.screen.SetContent(startX+i, startY+screenRow, ch, nil, lineNumStyleToUse)
				}
			}
			screenRow++
			continue
		}

		// Draw line with wrapping
		charIdx := 0
		isFirstWrap := true
		for charIdx < len(line) && screenRow < height {
			// Draw line number or wrap indicator
			if isFirstWrap {
				numStr := fmt.Sprintf("%*d ", lineNumWidth-1, lineNum)
				for i, ch := range numStr {
					if startX+i < startX+width {
						s.screen.SetContent(startX+i, startY+screenRow, ch, nil, lineNumStyleToUse)
					}
				}
				isFirstWrap = false
			} else {
				// Draw wrap continuation indicator
				wrapIndicator := fmt.Sprintf("%*s ", lineNumWidth-1, "↪")
				wrapStyleToUse := wrapStyle
				if isCurrentLine {
					wrapStyleToUse = currentLineNumStyle
				}
				for i, ch := range wrapIndicator {
					if startX+i < startX+width {
						s.screen.SetContent(startX+i, startY+screenRow, ch, nil, wrapStyleToUse)
					}
				}
			}

			// Get syntax highlighting tokens for this line (if available)
			var tokens []Token
			if buf.HighlightCache != nil {
				tokens = buf.HighlightCache.GetTokens(lineIdx, line, buf.Lines)
			}

			// Get tab stop width from buffer config
			tabStop := buf.Config.TabStop
			if tabStop <= 0 {
				tabStop = 4
			}

			// Draw text for this screen row
			// col = visual column on screen, charIdx = index into line runes
			textStart := startX + lineNumWidth
			col := 0
			for col < textWidth && charIdx < len(line) {
				ch := line[charIdx]
				charStyle := tcell.StyleDefault

				// Apply syntax highlighting
				if len(tokens) > 0 {
					if style, ok := GetStyleAt(tokens, charIdx); ok {
						charStyle = style
					}
				}

				// Selection takes precedence over syntax highlighting (use pane's selection)
				if pane.IsInSelection(lineIdx, charIdx) {
					charStyle = selectionStyle
				}

				// Search match highlighting takes highest precedence
				if search != nil && search.Active && len(search.Matches) > 0 {
					matchIdx, inMatch := isInSearchMatch(search, lineIdx, charIdx)
					if inMatch {
						if matchIdx == search.CurrentIndex {
							charStyle = currentMatchStyle
						} else {
							charStyle = searchMatchStyle
						}
					}
				}

				if ch == '\t' {
					// Expand tab to spaces up to next tab stop
					spacesToNextStop := tabStop - (col % tabStop)
					for i := 0; i < spacesToNextStop && col < textWidth; i++ {
						s.screen.SetContent(textStart+col, startY+screenRow, ' ', nil, charStyle)
						col++
					}
				} else {
					s.screen.SetContent(textStart+col, startY+screenRow, ch, nil, charStyle)
					col++
				}
				charIdx++
			}
			screenRow++
		}
	}
}

// isInSearchMatch checks if a position is within any search match
// Returns the match index and whether position is in a match
func isInSearchMatch(search *SearchState, row, col int) (int, bool) {
	for i, match := range search.Matches {
		if match.Row == row && col >= match.Col && col < match.Col+match.Length {
			return i, true
		}
	}
	return -1, false
}

// getLineNumberWidth returns the line number gutter width for a buffer
func getLineNumberWidth(buf *Buffer) int {
	return lineNumberWidth(len(buf.Lines))
}

// getLineNumberWidthFromPane returns the line number gutter width for a pane
func getLineNumberWidthFromPane(pane *Pane) int {
	return lineNumberWidth(len(pane.Buffer.Lines))
}

// getVisualColumn calculates the visual column position accounting for tab expansion
func getVisualColumn(line []rune, charCol int, tabStop int) int {
	if tabStop <= 0 {
		tabStop = 4
	}
	visualCol := 0
	for i := 0; i < charCol && i < len(line); i++ {
		if line[i] == '\t' {
			// Tab expands to next tab stop
			visualCol += tabStop - (visualCol % tabStop)
		} else {
			visualCol++
		}
	}
	return visualCol
}

// getVisualLineWidth calculates the visual width of a line accounting for tabs
func getVisualLineWidth(line []rune, tabStop int) int {
	return getVisualColumn(line, len(line), tabStop)
}

// getCursorScreenPos calculates the screen position of the cursor with line wrapping
func getCursorScreenPos(buf *Buffer, paneWidth, lineNumWidth int) (screenX, screenY int) {
	textWidth := paneWidth - lineNumWidth
	if textWidth < 1 {
		textWidth = 1
	}

	tabStop := buf.Config.TabStop
	if tabStop <= 0 {
		tabStop = 4
	}

	screenY = 0

	// Count screen rows used by lines from ScrollOffset to cursor
	for lineIdx := buf.ScrollOffset; lineIdx < buf.CursorRow && lineIdx < len(buf.Lines); lineIdx++ {
		line := buf.Lines[lineIdx]
		if len(line) == 0 {
			screenY++
		} else {
			visualWidth := getVisualLineWidth(line, tabStop)
			screenY += (visualWidth + textWidth - 1) / textWidth // Ceiling division
		}
	}

	// Calculate position within the cursor's line
	cursorLine := buf.Lines[buf.CursorRow]
	if len(cursorLine) == 0 || buf.CursorCol == 0 {
		screenX = lineNumWidth
	} else {
		// Calculate visual column accounting for tabs
		visualCol := getVisualColumn(cursorLine, buf.CursorCol, tabStop)
		// Which wrapped row is the cursor on?
		wrapRow := visualCol / textWidth
		screenY += wrapRow
		screenX = lineNumWidth + (visualCol % textWidth)
	}

	return screenX, screenY
}

// getCursorScreenPosFromPane calculates the screen position of the cursor for a pane
func getCursorScreenPosFromPane(pane *Pane, paneWidth, lineNumWidth int) (screenX, screenY int) {
	buf := pane.Buffer
	textWidth := paneWidth - lineNumWidth
	if textWidth < 1 {
		textWidth = 1
	}

	tabStop := buf.Config.TabStop
	if tabStop <= 0 {
		tabStop = 4
	}

	screenY = 0

	// Count screen rows used by lines from ScrollOffset to cursor
	for lineIdx := pane.ScrollOffset; lineIdx < pane.CursorRow && lineIdx < len(buf.Lines); lineIdx++ {
		line := buf.Lines[lineIdx]
		if len(line) == 0 {
			screenY++
		} else {
			visualWidth := getVisualLineWidth(line, tabStop)
			screenY += (visualWidth + textWidth - 1) / textWidth // Ceiling division
		}
	}

	// Calculate position within the cursor's line
	cursorLine := buf.Lines[pane.CursorRow]
	if len(cursorLine) == 0 || pane.CursorCol == 0 {
		screenX = lineNumWidth
	} else {
		// Calculate visual column accounting for tabs
		visualCol := getVisualColumn(cursorLine, pane.CursorCol, tabStop)
		// Which wrapped row is the cursor on?
		wrapRow := visualCol / textWidth
		screenY += wrapRow
		screenX = lineNumWidth + (visualCol % textWidth)
	}

	return screenX, screenY
}

// renderAutocomplete draws the autocomplete dropdown overlay
func (s *Screen) renderAutocomplete(ac *AutocompleteState, cursorX, cursorY, screenHeight int) {
	if ac == nil || len(ac.Suggestions) == 0 {
		return
	}

	// Calculate dropdown dimensions with extra horizontal padding
	maxWidth := 0
	for _, sug := range ac.Suggestions {
		if len(sug.Word) > maxWidth {
			maxWidth = len(sug.Word)
		}
	}
	horizontalPadding := 2 // spaces on each side
	dropdownWidth := maxWidth + (horizontalPadding * 2)
	dropdownHeight := len(ac.Suggestions)

	// Determine position (below or above cursor)
	spaceBelow := screenHeight - cursorY - 1
	var startY int
	showAbove := false

	if spaceBelow >= dropdownHeight {
		startY = cursorY + 1
	} else {
		// Try to show above
		startY = cursorY - dropdownHeight
		showAbove = true
		if startY < 0 {
			// Not enough space above either, truncate
			if spaceBelow > cursorY {
				// More space below, show truncated below
				startY = cursorY + 1
				dropdownHeight = spaceBelow
				showAbove = false
			} else {
				// More space above, show truncated above
				dropdownHeight = cursorY
				startY = 0
			}
		}
	}

	if dropdownHeight <= 0 {
		return
	}

	// Update state for rendering position
	ac.ShowAbove = showAbove

	// Styles: white on black for selected, black on purple for unselected
	unselectedStyle := tcell.StyleDefault.Background(tcell.ColorPurple).Foreground(tcell.ColorBlack)
	selectedStyle := tcell.StyleDefault.Background(tcell.ColorBlack).Foreground(tcell.ColorWhite).Bold(true)

	// Draw dropdown
	displayCount := dropdownHeight
	if displayCount > len(ac.Suggestions) {
		displayCount = len(ac.Suggestions)
	}

	for i := 0; i < displayCount; i++ {
		sug := ac.Suggestions[i]
		style := unselectedStyle
		if i == ac.SelectedIdx {
			style = selectedStyle
		}

		y := startY + i
		x := cursorX

		// Draw left padding
		for p := 0; p < horizontalPadding; p++ {
			s.screen.SetContent(x, y, ' ', nil, style)
			x++
		}

		// Draw suggestion text
		for _, r := range sug.Word {
			s.screen.SetContent(x, y, r, nil, style)
			x++
		}

		// Pad remaining width (text + right padding)
		for x < cursorX+dropdownWidth {
			s.screen.SetContent(x, y, ' ', nil, style)
			x++
		}
	}
}
