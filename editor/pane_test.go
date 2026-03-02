package editor

import "testing"

// ---------------------------------------------------------------------------
// TestNewPane
// ---------------------------------------------------------------------------

func TestNewPane(t *testing.T) {
	p := newTestPane("hello", "world")

	if p.CursorRow != 0 {
		t.Errorf("CursorRow = %d, want 0", p.CursorRow)
	}
	if p.CursorCol != 0 {
		t.Errorf("CursorCol = %d, want 0", p.CursorCol)
	}
	if p.ScrollOffset != 0 {
		t.Errorf("ScrollOffset = %d, want 0", p.ScrollOffset)
	}
	if p.SelectionActive {
		t.Error("SelectionActive should be false")
	}
	if p.Buffer == nil {
		t.Fatal("Buffer should not be nil")
	}
	if len(p.Buffer.Lines) != 2 {
		t.Errorf("Buffer.Lines length = %d, want 2", len(p.Buffer.Lines))
	}
}

// ---------------------------------------------------------------------------
// TestNewPaneAtLine
// ---------------------------------------------------------------------------

func TestNewPaneAtLine(t *testing.T) {
	tests := []struct {
		name    string
		lines   []string
		line    int
		wantRow int
		wantCol int
	}{
		{"line 1 (first)", []string{"aaa", "bbb", "ccc"}, 1, 0, 0},
		{"line 2 (middle)", []string{"aaa", "bbb", "ccc"}, 2, 1, 0},
		{"line 3 (last)", []string{"aaa", "bbb", "ccc"}, 3, 2, 0},
		{"line 0 stays at 0", []string{"aaa", "bbb"}, 0, 0, 0},
		{"negative line stays at 0", []string{"aaa", "bbb"}, -5, 0, 0},
		{"line beyond end clamped", []string{"aaa", "bbb"}, 100, 1, 0},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			buf := newTestBuffer(tc.lines...)
			p := NewPaneAtLine(buf, tc.line)
			if p.CursorRow != tc.wantRow {
				t.Errorf("CursorRow = %d, want %d", p.CursorRow, tc.wantRow)
			}
			if p.CursorCol != tc.wantCol {
				t.Errorf("CursorCol = %d, want %d", p.CursorCol, tc.wantCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneGotoLine
// ---------------------------------------------------------------------------

func TestPaneGotoLine(t *testing.T) {
	tests := []struct {
		name    string
		lines   []string
		goto_   int
		wantRow int
	}{
		{"goto first line", []string{"a", "b", "c"}, 1, 0},
		{"goto middle", []string{"a", "b", "c"}, 2, 1},
		{"goto last", []string{"a", "b", "c"}, 3, 2},
		{"goto 0 clamps to 1", []string{"a", "b", "c"}, 0, 0},
		{"goto negative clamps to 1", []string{"a", "b", "c"}, -10, 0},
		{"goto beyond end clamps", []string{"a", "b", "c"}, 99, 2},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane(tc.lines...)
			// Start with cursor somewhere else
			p.CursorRow = 1
			p.CursorCol = 2
			p.GotoLine(tc.goto_)
			if p.CursorRow != tc.wantRow {
				t.Errorf("CursorRow = %d, want %d", p.CursorRow, tc.wantRow)
			}
			if p.CursorCol != 0 {
				t.Errorf("CursorCol = %d, want 0 after GotoLine", p.CursorCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneCursorClamp
// ---------------------------------------------------------------------------

func TestPaneCursorClamp(t *testing.T) {
	t.Run("clamp row above buffer", func(t *testing.T) {
		p := newTestPane("hello", "world")
		p.CursorRow = -5
		p.clampCursor()
		if p.CursorRow != 0 {
			t.Errorf("CursorRow = %d, want 0", p.CursorRow)
		}
	})

	t.Run("clamp row below buffer", func(t *testing.T) {
		p := newTestPane("hello", "world")
		p.CursorRow = 100
		p.clampCursor()
		if p.CursorRow != 1 {
			t.Errorf("CursorRow = %d, want 1", p.CursorRow)
		}
	})

	t.Run("clamp col beyond line length", func(t *testing.T) {
		p := newTestPane("hi")
		p.CursorCol = 50
		p.clampCursor()
		if p.CursorCol != 2 {
			t.Errorf("CursorCol = %d, want 2", p.CursorCol)
		}
	})

	t.Run("clamp col negative", func(t *testing.T) {
		p := newTestPane("hi")
		p.CursorCol = -3
		p.clampCursor()
		if p.CursorCol != 0 {
			t.Errorf("CursorCol = %d, want 0", p.CursorCol)
		}
	})

	t.Run("empty buffer", func(t *testing.T) {
		p := newTestPane() // creates buffer with one empty line
		p.CursorRow = 5
		p.CursorCol = 5
		p.clampCursor()
		if p.CursorRow != 0 {
			t.Errorf("CursorRow = %d, want 0", p.CursorRow)
		}
		if p.CursorCol != 0 {
			t.Errorf("CursorCol = %d, want 0", p.CursorCol)
		}
	})
}

// ---------------------------------------------------------------------------
// TestPaneAdjustCursorForEdit
// ---------------------------------------------------------------------------

func TestPaneAdjustCursorForEdit(t *testing.T) {
	t.Run("insert lines below cursor - no change", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 1
		// Insert 2 lines at row 3 (after cursor)
		p.Buffer.Lines = append(p.Buffer.Lines, []rune("f"), []rune("g"))
		p.AdjustCursorForEdit(3, 2)
		if p.CursorRow != 1 {
			t.Errorf("CursorRow = %d, want 1", p.CursorRow)
		}
	})

	t.Run("insert lines at cursor row", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 2
		p.Buffer.Lines = append(p.Buffer.Lines, []rune("f"), []rune("g"))
		p.AdjustCursorForEdit(2, 2)
		if p.CursorRow != 4 {
			t.Errorf("CursorRow = %d, want 4", p.CursorRow)
		}
	})

	t.Run("insert lines above cursor", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 3
		p.Buffer.Lines = append(p.Buffer.Lines, []rune("f"))
		p.AdjustCursorForEdit(1, 1)
		if p.CursorRow != 4 {
			t.Errorf("CursorRow = %d, want 4", p.CursorRow)
		}
	})

	t.Run("delete lines after cursor", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 1
		// Simulate deleting row 3 (1 line)
		p.Buffer.Lines = append(p.Buffer.Lines[:3], p.Buffer.Lines[4:]...)
		p.AdjustCursorForEdit(3, -1)
		if p.CursorRow != 1 {
			t.Errorf("CursorRow = %d, want 1", p.CursorRow)
		}
	})

	t.Run("delete lines above cursor", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 4
		// Delete rows 1-2 (2 lines)
		p.Buffer.Lines = append(p.Buffer.Lines[:1], p.Buffer.Lines[3:]...)
		p.AdjustCursorForEdit(1, -2)
		if p.CursorRow != 2 {
			t.Errorf("CursorRow = %d, want 2", p.CursorRow)
		}
	})

	t.Run("cursor in deleted range", func(t *testing.T) {
		p := newTestPane("a", "b", "c", "d", "e")
		p.CursorRow = 2
		// Delete rows 1-3 (3 lines)
		p.Buffer.Lines = append(p.Buffer.Lines[:1], p.Buffer.Lines[4:]...)
		p.AdjustCursorForEdit(1, -3)
		if p.CursorRow != 1 {
			t.Errorf("CursorRow = %d, want 1 (deletion point)", p.CursorRow)
		}
	})

	t.Run("zero delta is no-op", func(t *testing.T) {
		p := newTestPane("a", "b", "c")
		p.CursorRow = 1
		p.CursorCol = 1
		p.AdjustCursorForEdit(0, 0)
		if p.CursorRow != 1 || p.CursorCol != 1 {
			t.Errorf("got (%d,%d), want (1,1)", p.CursorRow, p.CursorCol)
		}
	})
}

// ---------------------------------------------------------------------------
// TestPaneStartAndClearSelection
// ---------------------------------------------------------------------------

func TestPaneStartAndClearSelection(t *testing.T) {
	p := newTestPane("hello world", "second line")
	p.CursorRow = 1
	p.CursorCol = 3

	p.StartSelection()
	if !p.SelectionActive {
		t.Error("SelectionActive should be true after StartSelection")
	}
	if p.SelectionStartRow != 1 {
		t.Errorf("SelectionStartRow = %d, want 1", p.SelectionStartRow)
	}
	if p.SelectionStartCol != 3 {
		t.Errorf("SelectionStartCol = %d, want 3", p.SelectionStartCol)
	}

	p.ClearSelection()
	if p.SelectionActive {
		t.Error("SelectionActive should be false after ClearSelection")
	}
}

// ---------------------------------------------------------------------------
// TestPaneGetSelection
// ---------------------------------------------------------------------------

func TestPaneGetSelection(t *testing.T) {
	tests := []struct {
		name                         string
		selStartRow, selStartCol     int
		cursorRow, cursorCol         int
		wantSR, wantSC, wantER, wantEC int
	}{
		{
			"forward selection",
			0, 2, 1, 5,
			0, 2, 1, 5,
		},
		{
			"backward selection normalizes",
			1, 5, 0, 2,
			0, 2, 1, 5,
		},
		{
			"same row forward",
			0, 1, 0, 4,
			0, 1, 0, 4,
		},
		{
			"same row backward normalizes",
			0, 4, 0, 1,
			0, 1, 0, 4,
		},
		{
			"same position",
			2, 3, 2, 3,
			2, 3, 2, 3,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane("hello", "world", "foo bar")
			p.SelectionStartRow = tc.selStartRow
			p.SelectionStartCol = tc.selStartCol
			p.CursorRow = tc.cursorRow
			p.CursorCol = tc.cursorCol

			sr, sc, er, ec := p.GetSelection()
			if sr != tc.wantSR || sc != tc.wantSC || er != tc.wantER || ec != tc.wantEC {
				t.Errorf("GetSelection() = (%d,%d,%d,%d), want (%d,%d,%d,%d)",
					sr, sc, er, ec, tc.wantSR, tc.wantSC, tc.wantER, tc.wantEC)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneIsInSelection
// ---------------------------------------------------------------------------

func TestPaneIsInSelection(t *testing.T) {
	p := newTestPane("hello", "world", "foo bar")
	p.SelectionActive = true
	p.SelectionStartRow = 0
	p.SelectionStartCol = 2
	p.CursorRow = 1
	p.CursorCol = 3

	tests := []struct {
		name string
		row  int
		col  int
		want bool
	}{
		{"before start row", 0, 0, false},
		{"at start exact", 0, 2, true},
		{"just after start", 0, 3, true},
		{"end of first line", 0, 4, true},
		{"middle of selection", 1, 0, true},
		{"at end exact", 1, 3, true},
		{"just after end", 1, 4, false},
		{"row after selection", 2, 0, false},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := p.IsInSelection(tc.row, tc.col)
			if got != tc.want {
				t.Errorf("IsInSelection(%d,%d) = %v, want %v", tc.row, tc.col, got, tc.want)
			}
		})
	}

	t.Run("inactive selection always false", func(t *testing.T) {
		p2 := newTestPane("hello")
		p2.SelectionActive = false
		if p2.IsInSelection(0, 0) {
			t.Error("IsInSelection should return false when selection inactive")
		}
	})
}

// ---------------------------------------------------------------------------
// TestPaneMoveUpDown
// ---------------------------------------------------------------------------

func TestPaneMoveUpDown(t *testing.T) {
	p := newTestPane("aaa", "bbbbb", "cc")
	// Start at row 0
	if p.CursorRow != 0 {
		t.Fatalf("initial CursorRow = %d, want 0", p.CursorRow)
	}

	// MoveDown
	p.MoveDown()
	if p.CursorRow != 1 {
		t.Errorf("after MoveDown: CursorRow = %d, want 1", p.CursorRow)
	}

	p.MoveDown()
	if p.CursorRow != 2 {
		t.Errorf("after 2nd MoveDown: CursorRow = %d, want 2", p.CursorRow)
	}

	// MoveDown at last line - no change
	p.MoveDown()
	if p.CursorRow != 2 {
		t.Errorf("MoveDown at last line: CursorRow = %d, want 2", p.CursorRow)
	}

	// MoveUp
	p.MoveUp()
	if p.CursorRow != 1 {
		t.Errorf("after MoveUp: CursorRow = %d, want 1", p.CursorRow)
	}

	p.MoveUp()
	if p.CursorRow != 0 {
		t.Errorf("after 2nd MoveUp: CursorRow = %d, want 0", p.CursorRow)
	}

	// MoveUp at first line - no change
	p.MoveUp()
	if p.CursorRow != 0 {
		t.Errorf("MoveUp at first line: CursorRow = %d, want 0", p.CursorRow)
	}

	// Column clamping when moving to shorter line
	p.CursorRow = 1
	p.CursorCol = 4 // "bbbbb" has len 5, col 4 is valid
	p.MoveDown()     // Move to "cc" (len 2)
	if p.CursorCol != 2 {
		t.Errorf("col clamp on MoveDown: CursorCol = %d, want 2", p.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// TestPaneMoveLeftRight
// ---------------------------------------------------------------------------

func TestPaneMoveLeftRight(t *testing.T) {
	p := newTestPane("ab", "cd")

	// Basic right movement
	p.MoveRight()
	if p.CursorCol != 1 {
		t.Errorf("CursorCol = %d, want 1", p.CursorCol)
	}
	p.MoveRight()
	if p.CursorCol != 2 {
		t.Errorf("CursorCol = %d, want 2", p.CursorCol)
	}

	// Right at end of line wraps to next line
	p.MoveRight()
	if p.CursorRow != 1 || p.CursorCol != 0 {
		t.Errorf("wrap right: got (%d,%d), want (1,0)", p.CursorRow, p.CursorCol)
	}

	// Right at end of last line - no change
	p.CursorCol = 2
	p.MoveRight()
	if p.CursorRow != 1 || p.CursorCol != 2 {
		t.Errorf("right at end of last line: got (%d,%d), want (1,2)", p.CursorRow, p.CursorCol)
	}

	// Left at start of line wraps to end of prev line
	p.CursorRow = 1
	p.CursorCol = 0
	p.MoveLeft()
	if p.CursorRow != 0 || p.CursorCol != 2 {
		t.Errorf("wrap left: got (%d,%d), want (0,2)", p.CursorRow, p.CursorCol)
	}

	// Left at start of first line - no change
	p.CursorRow = 0
	p.CursorCol = 0
	p.MoveLeft()
	if p.CursorRow != 0 || p.CursorCol != 0 {
		t.Errorf("left at start: got (%d,%d), want (0,0)", p.CursorRow, p.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// TestPaneMoveUpDownN
// ---------------------------------------------------------------------------

func TestPaneMoveUpDownN(t *testing.T) {
	p := newTestPane("a", "b", "c", "d", "e", "f", "g", "h", "i", "j")

	p.MoveDownN(3)
	if p.CursorRow != 3 {
		t.Errorf("MoveDownN(3): CursorRow = %d, want 3", p.CursorRow)
	}

	p.MoveDownN(100) // Clamp to last line
	if p.CursorRow != 9 {
		t.Errorf("MoveDownN(100): CursorRow = %d, want 9", p.CursorRow)
	}

	p.MoveUpN(4)
	if p.CursorRow != 5 {
		t.Errorf("MoveUpN(4): CursorRow = %d, want 5", p.CursorRow)
	}

	p.MoveUpN(100) // Clamp to first line
	if p.CursorRow != 0 {
		t.Errorf("MoveUpN(100): CursorRow = %d, want 0", p.CursorRow)
	}

	// Column clamping: start on long line, move to short line
	p2 := newTestPane("hello world", "hi")
	p2.CursorCol = 10
	p2.MoveDownN(1)
	if p2.CursorCol != 2 {
		t.Errorf("col clamp MoveDownN: CursorCol = %d, want 2", p2.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// TestPaneMoveToLineStartEnd
// ---------------------------------------------------------------------------

func TestPaneMoveToLineStartEnd(t *testing.T) {
	p := newTestPane("hello world")
	p.CursorCol = 5

	p.MoveToLineStart()
	if p.CursorCol != 0 {
		t.Errorf("MoveToLineStart: CursorCol = %d, want 0", p.CursorCol)
	}

	p.MoveToLineEnd()
	if p.CursorCol != 11 { // len("hello world") = 11
		t.Errorf("MoveToLineEnd: CursorCol = %d, want 11", p.CursorCol)
	}

	// Empty line
	p2 := newTestPane("")
	p2.MoveToLineEnd()
	if p2.CursorCol != 0 {
		t.Errorf("MoveToLineEnd on empty line: CursorCol = %d, want 0", p2.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// TestPanePageDownUp
// ---------------------------------------------------------------------------

func TestPanePageDownUp(t *testing.T) {
	lines := make([]string, 100)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)

	screenHeight := 20

	// PageDown moves by height/2 = 10
	p.PageDown(screenHeight)
	if p.CursorRow != 10 {
		t.Errorf("PageDown: CursorRow = %d, want 10", p.CursorRow)
	}

	p.PageDown(screenHeight)
	if p.CursorRow != 20 {
		t.Errorf("2nd PageDown: CursorRow = %d, want 20", p.CursorRow)
	}

	// PageDown near end clamps
	p.CursorRow = 96
	p.PageDown(screenHeight)
	if p.CursorRow != 99 {
		t.Errorf("PageDown near end: CursorRow = %d, want 99", p.CursorRow)
	}

	// PageUp
	p.CursorRow = 30
	p.PageUp(screenHeight)
	if p.CursorRow != 20 {
		t.Errorf("PageUp: CursorRow = %d, want 20", p.CursorRow)
	}

	// PageUp near start clamps to 0
	p.CursorRow = 3
	p.PageUp(screenHeight)
	if p.CursorRow != 0 {
		t.Errorf("PageUp near start: CursorRow = %d, want 0", p.CursorRow)
	}
}

// ---------------------------------------------------------------------------
// TestPaneMoveToNextWord
// ---------------------------------------------------------------------------

func TestPaneMoveToNextWord(t *testing.T) {
	tests := []struct {
		name    string
		lines   []string
		row     int
		col     int
		wantRow int
		wantCol int
	}{
		{
			"skip word chars to next word",
			[]string{"hello world"},
			0, 0,
			0, 6,
		},
		{
			"skip whitespace already between words",
			[]string{"hello   world"},
			0, 5, // at space after "hello"
			0, 8,
		},
		{
			"punctuation group",
			[]string{"foo::bar"},
			0, 3, // at first ':'
			0, 5,
		},
		{
			"word to punctuation",
			[]string{"abc++def"},
			0, 0,
			0, 3,
		},
		{
			"end of line wraps to next",
			[]string{"abc", "def"},
			0, 3, // at end of "abc"
			1, 0,
		},
		{
			"end of line wraps skipping leading whitespace",
			[]string{"abc", "  def"},
			0, 3,
			1, 2,
		},
		{
			"last line end stays",
			[]string{"abc"},
			0, 3,
			0, 3,
		},
		{
			"middle of word",
			[]string{"hello world"},
			0, 2,
			0, 6,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane(tc.lines...)
			p.CursorRow = tc.row
			p.CursorCol = tc.col
			p.MoveToNextWord()
			if p.CursorRow != tc.wantRow || p.CursorCol != tc.wantCol {
				t.Errorf("got (%d,%d), want (%d,%d)", p.CursorRow, p.CursorCol, tc.wantRow, tc.wantCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneMoveToPrevWord
// ---------------------------------------------------------------------------

func TestPaneMoveToPrevWord(t *testing.T) {
	tests := []struct {
		name    string
		lines   []string
		row     int
		col     int
		wantRow int
		wantCol int
	}{
		{
			"from middle of second word",
			[]string{"hello world"},
			0, 8,
			0, 6,
		},
		{
			"from start of second word",
			[]string{"hello world"},
			0, 6,
			0, 0,
		},
		{
			"from start of line wraps to prev",
			[]string{"abc", "def"},
			1, 0,
			0, 0,
		},
		{
			"at start of first line stays",
			[]string{"hello"},
			0, 0,
			0, 0,
		},
		{
			"backward over punctuation",
			[]string{"foo::bar"},
			0, 5,
			0, 3,
		},
		{
			"skip whitespace backward",
			[]string{"hello   world"},
			0, 8,
			0, 0,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane(tc.lines...)
			p.CursorRow = tc.row
			p.CursorCol = tc.col
			p.MoveToPrevWord()
			if p.CursorRow != tc.wantRow || p.CursorCol != tc.wantCol {
				t.Errorf("got (%d,%d), want (%d,%d)", p.CursorRow, p.CursorCol, tc.wantRow, tc.wantCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneFindCharForward
// ---------------------------------------------------------------------------

func TestPaneFindCharForward(t *testing.T) {
	tests := []struct {
		name    string
		line    string
		col     int
		ch      rune
		wantCol int
	}{
		{"find next occurrence", "abcdefg", 0, 'd', 3},
		{"find from middle", "abcdecf", 2, 'c', 5},
		{"no match stays put", "abcdefg", 0, 'z', 0},
		{"does not match current pos", "abcabc", 0, 'a', 3},
		{"find at end", "abcdef", 0, 'f', 5},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane(tc.line)
			p.CursorCol = tc.col
			p.FindCharForward(tc.ch)
			if p.CursorCol != tc.wantCol {
				t.Errorf("CursorCol = %d, want %d", p.CursorCol, tc.wantCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestPaneFindCharBackward
// ---------------------------------------------------------------------------

func TestPaneFindCharBackward(t *testing.T) {
	tests := []struct {
		name    string
		line    string
		col     int
		ch      rune
		wantCol int
	}{
		{"find prev occurrence", "abcdefg", 6, 'c', 2},
		{"find from middle", "abcabc", 5, 'a', 3},
		{"no match stays put", "abcdefg", 6, 'z', 6},
		{"does not match current pos", "abcabc", 3, 'a', 0},
		{"find at start", "abcdef", 5, 'a', 0},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := newTestPane(tc.line)
			p.CursorCol = tc.col
			p.FindCharBackward(tc.ch)
			if p.CursorCol != tc.wantCol {
				t.Errorf("CursorCol = %d, want %d", p.CursorCol, tc.wantCol)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// TestAdjustScroll
// ---------------------------------------------------------------------------

func TestAdjustScroll_CursorBelowViewport(t *testing.T) {
	lines := make([]string, 50)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)
	p.CursorRow = 30
	p.ScrollOffset = 0

	p.AdjustScroll(80, 10)

	if p.ScrollOffset == 0 {
		t.Error("ScrollOffset should have increased to keep cursor visible")
	}
	if p.CursorRow < p.ScrollOffset {
		t.Errorf("CursorRow %d should be >= ScrollOffset %d", p.CursorRow, p.ScrollOffset)
	}
}

func TestAdjustScroll_CursorAboveViewport(t *testing.T) {
	lines := make([]string, 50)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)
	p.CursorRow = 5
	p.ScrollOffset = 20

	p.AdjustScroll(80, 10)

	if p.ScrollOffset > p.CursorRow {
		t.Errorf("ScrollOffset %d should be <= CursorRow %d", p.ScrollOffset, p.CursorRow)
	}
}

func TestAdjustScroll_CursorAlreadyVisible(t *testing.T) {
	lines := make([]string, 50)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)
	// Place cursor well within view (past scroll margin from both edges)
	p.CursorRow = 15
	p.ScrollOffset = 5

	oldOffset := p.ScrollOffset
	p.AdjustScroll(80, 20)

	// Cursor at row 15, scroll at 5, height 20: position 10 in viewport, well within margins
	if p.ScrollOffset != oldOffset {
		t.Errorf("ScrollOffset changed from %d to %d, should stay same", oldOffset, p.ScrollOffset)
	}
}

func TestAdjustScroll_SmallScreen(t *testing.T) {
	lines := make([]string, 10)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)
	p.CursorRow = 5
	p.ScrollOffset = 0

	p.AdjustScroll(80, 3)

	// Cursor should be visible within the 3-line screen
	if p.CursorRow < p.ScrollOffset {
		t.Errorf("CursorRow %d below ScrollOffset %d", p.CursorRow, p.ScrollOffset)
	}
}

func TestAdjustScroll_MinTextWidth(t *testing.T) {
	p := newTestPane("hello world")
	p.CursorRow = 0
	p.CursorCol = 5

	// textWidth 0 should be clamped to 1; should not panic
	p.AdjustScroll(0, 10)
}

func TestAdjustScroll_AtFirstLine(t *testing.T) {
	p := newTestPane("hello", "world")
	p.CursorRow = 0
	p.ScrollOffset = 0

	p.AdjustScroll(80, 10)

	if p.ScrollOffset != 0 {
		t.Errorf("ScrollOffset = %d, want 0", p.ScrollOffset)
	}
}

func TestAdjustScroll_CursorRowBelowScrollOffset(t *testing.T) {
	// Tests the guard: if p.CursorRow < p.ScrollOffset
	lines := make([]string, 20)
	for i := range lines {
		lines[i] = "line"
	}
	p := newTestPane(lines...)
	p.CursorRow = 2
	p.ScrollOffset = 10

	p.AdjustScroll(80, 10)

	if p.CursorRow < p.ScrollOffset {
		t.Errorf("CursorRow %d should be >= ScrollOffset %d after adjust", p.CursorRow, p.ScrollOffset)
	}
}

// ---------------------------------------------------------------------------
// TestPane_HasActiveWidget
// ---------------------------------------------------------------------------

func TestPane_HasActiveWidget_Nil(t *testing.T) {
	p := newTestPane("hello")
	if p.HasActiveWidget() {
		t.Error("should return false when Widget is nil")
	}
}

func TestPane_HasActiveWidget_Active(t *testing.T) {
	p := newTestPane("hello")
	p.Widget = NewWidgetState(WidgetSearch, 0, 0)
	if !p.HasActiveWidget() {
		t.Error("should return true when Widget is active")
	}
}

func TestPane_HasActiveWidget_Inactive(t *testing.T) {
	p := newTestPane("hello")
	p.Widget = &WidgetState{Active: false}
	if p.HasActiveWidget() {
		t.Error("should return false when Widget.Active is false")
	}
}
