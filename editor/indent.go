package editor

// smartIndentForNewLine computes indentation for a new line opened after row.
// It starts from the current line's indent and adjusts based on content:
// indent after '{', '(', ':'; dedent if line ends with ')' with unmatched parens.
func (b *Buffer) smartIndentForNewLine(row int) []rune {
	line := b.Lines[row]
	stripped := stripLeadingWhitespace(line)
	level := b.getIndentLevel(line)

	if len(stripped) > 0 {
		last := stripped[len(stripped)-1]
		openParens := countRune(stripped, '(')
		closeParens := countRune(stripped, ')')
		openBraces := countRune(stripped, '{')
		closeBraces := countRune(stripped, '}')

		// Line ends with opener or colon — indent
		if last == ':' || last == '{' {
			level++
		} else if last == '(' && openParens > closeParens {
			level++
		}

		// Line ends with closer and has net closing — dedent
		if last == ')' && closeParens > openParens {
			level--
		}
		if last == '}' && closeBraces > openBraces {
			level--
		}
	}

	if level < 0 {
		level = 0
	}
	return b.makeIndent(level)
}

// getIndentLevel returns the indent level of a line.
// Tabs count as 1 level, shiftWidth spaces count as 1 level.
func (b *Buffer) getIndentLevel(line []rune) int {
	sw := b.GetShiftWidth()
	level := 0
	spaces := 0
	for _, ch := range line {
		if ch == '\t' {
			level++
			spaces = 0
		} else if ch == ' ' {
			spaces++
			if spaces >= sw {
				level++
				spaces = 0
			}
		} else {
			break
		}
	}
	return level
}

// makeIndent creates indentation whitespace for the given level
func (b *Buffer) makeIndent(level int) []rune {
	if level <= 0 {
		return nil
	}
	if b.Config.ExpandTab {
		sw := b.GetShiftWidth()
		indent := make([]rune, level*sw)
		for i := range indent {
			indent[i] = ' '
		}
		return indent
	}
	indent := make([]rune, level)
	for i := range indent {
		indent[i] = '\t'
	}
	return indent
}

// GetPrevNonEmptyLineIndent returns the indent level of the first non-empty
// line above row. If the line ends with { or :, adds +1 for brace-open context.
func (b *Buffer) GetPrevNonEmptyLineIndent(row int) int {
	for r := row - 1; r >= 0; r-- {
		line := b.Lines[r]
		if !hasContent(line) {
			continue
		}
		level := b.getIndentLevel(line)
		// Check if line ends with opening brace/colon (brace-aware indent)
		if ch := lastNonWhitespace(line); ch == '{' || ch == ':' {
			level++
		}
		return level
	}
	return 0
}

// countRune counts occurrences of a rune in a slice
func countRune(line []rune, ch rune) int {
	n := 0
	for _, r := range line {
		if r == ch {
			n++
		}
	}
	return n
}

// ReindentLines reindents a range of lines using block-structure-aware
// indentation. It computes the proper indent for each line based on
// brace/colon patterns rather than preserving original relative indentation.
func (b *Buffer) ReindentLines(startRow, endRow, contextIndent int) {
	if startRow > endRow {
		return
	}
	if startRow < 0 {
		startRow = 0
	}
	if endRow >= len(b.Lines) {
		endRow = len(b.Lines) - 1
	}

	currentIndent := contextIndent
	braceDepth := 0
	prevLineContinuation := false

	for row := startRow; row <= endRow; row++ {
		line := b.Lines[row]
		stripped := stripLeadingWhitespace(line)

		// Empty/whitespace-only lines
		if len(stripped) == 0 {
			if len(line) > 0 {
				b.Lines[row] = nil
			}
			// Reset indent at paragraph boundaries when outside braces
			if braceDepth <= 0 {
				currentIndent = contextIndent
			}
			continue
		}

		openBraces := countRune(stripped, '{')
		closeBraces := countRune(stripped, '}')
		openParens := countRune(stripped, '(')
		closeParens := countRune(stripped, ')')

		first := stripped[0]
		last := stripped[len(stripped)-1]

		// If line starts with a closer, decrease indent for this line
		closerAtStart := first == '}' || first == ')'
		lineIndent := currentIndent
		if closerAtStart {
			lineIndent--
		}
		if lineIndent < 0 {
			lineIndent = 0
		}

		// Apply indent
		indent := b.makeIndent(lineIndent)
		newLine := make([]rune, len(indent)+len(stripped))
		copy(newLine, indent)
		copy(newLine[len(indent):], stripped)
		b.Lines[row] = newLine

		// Track brace depth for paragraph reset logic
		braceDepth += openBraces - closeBraces

		// Update currentIndent for next line based on net braces
		netBraces := openBraces - closeBraces
		if closerAtStart && first == '}' {
			netBraces++
		}
		currentIndent = lineIndent + netBraces

		// Unmatched opening paren at end of line (multi-line call)
		if last == '(' && openParens > closeParens {
			currentIndent++
		}
		// Unmatched closing paren at end of line
		if last == ')' && closeParens > openParens && first != ')' {
			currentIndent--
		}

		// ':' at end of line (Python block opener)
		if last == ':' {
			currentIndent++
		}

		// '\' at end of line (Python line continuation) — indent next line
		if last == '\\' {
			if !prevLineContinuation {
				currentIndent++
			}
			prevLineContinuation = true
		} else {
			if prevLineContinuation {
				currentIndent--
			}
			prevLineContinuation = false
		}

		if currentIndent < 0 {
			currentIndent = 0
		}
	}
	b.Modified = true
	b.ModCount++
}

// reindentPastedRange reindents pasted lines using context-aware indentation.
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
