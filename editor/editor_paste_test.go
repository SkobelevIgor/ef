package editor

import (
	"testing"
)

// TestPaste_WholeLineAfter verifies that pasting a whole-line clipboard (clipboardLine=true)
// after the cursor inserts the line below and moves the cursor down.
func TestPaste_WholeLineAfter(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line", "third line")
	ed := env.Editor
	pane := ed.activePane()
	buf := ed.activeBuffer()

	ed.clipboard = [][]rune{[]rune("pasted line")}
	ed.clipboardLine = true

	// Cursor starts at row 0
	if pane.CursorRow != 0 {
		t.Fatalf("expected cursor at row 0, got %d", pane.CursorRow)
	}

	firstRow, lastRow := ed.pasteAfter()

	// Should have 4 lines now
	if len(buf.Lines) != 4 {
		t.Errorf("expected 4 lines after paste, got %d", len(buf.Lines))
	}

	// Pasted line should appear at index 1 (below cursor row 0)
	got := string(buf.Lines[1])
	if got != "pasted line" {
		t.Errorf("expected line[1]='pasted line', got '%s'", got)
	}

	// Original lines should be preserved
	if string(buf.Lines[0]) != "hello world" {
		t.Errorf("expected line[0]='hello world', got '%s'", string(buf.Lines[0]))
	}
	if string(buf.Lines[2]) != "second line" {
		t.Errorf("expected line[2]='second line', got '%s'", string(buf.Lines[2]))
	}

	// Cursor should have moved down to the pasted line
	if pane.CursorRow != 1 {
		t.Errorf("expected cursor at row 1, got %d", pane.CursorRow)
	}
	if pane.CursorCol != 0 {
		t.Errorf("expected cursor col 0, got %d", pane.CursorCol)
	}

	// Return values: firstRow=1, lastRow=1 (single line pasted)
	if firstRow != 1 {
		t.Errorf("expected firstRow=1, got %d", firstRow)
	}
	if lastRow != 1 {
		t.Errorf("expected lastRow=1, got %d", lastRow)
	}
}

// TestPaste_WholeLineBefore verifies that pasting a whole-line clipboard before the cursor
// inserts the line above the current row without moving the cursor row down.
func TestPaste_WholeLineBefore(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line", "third line")
	ed := env.Editor
	pane := ed.activePane()
	buf := ed.activeBuffer()

	// Move cursor to row 1
	pane.CursorRow = 1

	ed.clipboard = [][]rune{[]rune("pasted line")}
	ed.clipboardLine = true

	firstRow, lastRow := ed.pasteBefore()

	// Should have 4 lines now
	if len(buf.Lines) != 4 {
		t.Errorf("expected 4 lines after paste, got %d", len(buf.Lines))
	}

	// Pasted line should appear at index 1 (before the original row 1)
	got := string(buf.Lines[1])
	if got != "pasted line" {
		t.Errorf("expected line[1]='pasted line', got '%s'", got)
	}

	// Original row 1 should now be at row 2
	if string(buf.Lines[2]) != "second line" {
		t.Errorf("expected line[2]='second line', got '%s'", string(buf.Lines[2]))
	}

	// Cursor should be at the pasted line (firstRow)
	if pane.CursorRow != 1 {
		t.Errorf("expected cursor at row 1, got %d", pane.CursorRow)
	}
	if pane.CursorCol != 0 {
		t.Errorf("expected cursor col 0, got %d", pane.CursorCol)
	}

	// Return values: firstRow=1, lastRow=1
	if firstRow != 1 {
		t.Errorf("expected firstRow=1, got %d", firstRow)
	}
	if lastRow != 1 {
		t.Errorf("expected lastRow=1, got %d", lastRow)
	}
}

// TestPaste_SingleLineInline verifies that a single-line inline clipboard (clipboardLine=false)
// is inserted after the cursor position within the current line.
func TestPaste_SingleLineInline(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	pane := ed.activePane()
	buf := ed.activeBuffer()

	// clipboard has inline text (not a whole line)
	ed.clipboard = [][]rune{[]rune("XYZ")}
	ed.clipboardLine = false

	// Cursor at col 4 (between 'o' and ' '), paste after = insertPos 5
	pane.CursorCol = 4

	firstRow, lastRow := ed.pasteAfter()

	// Line count should remain at 1
	if len(buf.Lines) != 1 {
		t.Errorf("expected 1 line, got %d", len(buf.Lines))
	}

	// "XYZ" inserted after col 4 (insertPos=5): "hello" + "XYZ" + " world"
	got := string(buf.Lines[0])
	if got != "helloXYZ world" {
		t.Errorf("expected 'helloXYZ world', got '%s'", got)
	}

	// Cursor should be at end of pasted text (insertPos + len - 1 = 5 + 3 - 1 = 7)
	if pane.CursorCol != 7 {
		t.Errorf("expected cursor col 7, got %d", pane.CursorCol)
	}

	// Single-line inline paste returns (-1, -1)
	if firstRow != -1 || lastRow != -1 {
		t.Errorf("expected (-1,-1) for single-line inline paste, got (%d,%d)", firstRow, lastRow)
	}
}

// TestPaste_MultiLineInline verifies that a multi-line clipboard (clipboardLine=false)
// splits the current line and inserts additional lines correctly.
func TestPaste_MultiLineInline(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	pane := ed.activePane()
	buf := ed.activeBuffer()

	// Multi-line inline clipboard
	ed.clipboard = [][]rune{
		[]rune("AAA"),
		[]rune("BBB"),
		[]rune("CCC"),
	}
	ed.clipboardLine = false

	// Cursor at col 5 ("hello|world"), paste after = insertPos 6
	pane.CursorCol = 5

	firstRow, lastRow := ed.pasteAfter()

	// Original 2 lines + 2 extra clipboard lines = 4 lines total
	// Line 0: "hello" + "AAA"  = "helloAAA" (insertPos=6 within "hello world", so "hello " + "AAA")
	// Line 1: "BBB"
	// Line 2: "CCC" + " world" = "CCC world"  (rest of original line 0 after insertPos)
	// Line 3: "second line"
	if len(buf.Lines) != 4 {
		t.Errorf("expected 4 lines, got %d", len(buf.Lines))
	}

	// insertPos = CursorCol+1 = 6
	// firstPart = line[:6] + clipboard[0] = "hello " + "AAA" = "hello AAA"
	got0 := string(buf.Lines[0])
	if got0 != "hello AAA" {
		t.Errorf("expected line[0]='hello AAA', got '%s'", got0)
	}

	got1 := string(buf.Lines[1])
	if got1 != "BBB" {
		t.Errorf("expected line[1]='BBB', got '%s'", got1)
	}

	// lastPart = clipboard[last] + line[insertPos:] = "CCC" + "world" = "CCCworld"
	got2 := string(buf.Lines[2])
	if got2 != "CCCworld" {
		t.Errorf("expected line[2]='CCCworld', got '%s'", got2)
	}

	got3 := string(buf.Lines[3])
	if got3 != "second line" {
		t.Errorf("expected line[3]='second line', got '%s'", got3)
	}

	// Return values: firstRow = cursorRow+1 = 1, lastRow = cursorRow+len(clipboard)-1 = 2
	if firstRow != 1 {
		t.Errorf("expected firstRow=1, got %d", firstRow)
	}
	if lastRow != 2 {
		t.Errorf("expected lastRow=2, got %d", lastRow)
	}

	// Cursor should be at the start of the last clipboard line
	if pane.CursorRow != 2 {
		t.Errorf("expected cursor row 2, got %d", pane.CursorRow)
	}
	// CursorCol = len(lastClip) - 1 = len("CCC") - 1 = 2
	if pane.CursorCol != 2 {
		t.Errorf("expected cursor col 2, got %d", pane.CursorCol)
	}
}

// TestPaste_EmptyClipboard verifies that performPaste returns (-1, -1) when the clipboard is empty.
func TestPaste_EmptyClipboard(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	buf := ed.activeBuffer()

	// Ensure clipboard is empty
	ed.clipboard = nil
	ed.clipboardLine = false

	firstRow, lastRow := ed.performPaste(false)

	// Should return -1, -1 and not change the buffer
	if firstRow != -1 || lastRow != -1 {
		t.Errorf("expected (-1,-1) for empty clipboard, got (%d,%d)", firstRow, lastRow)
	}

	// Buffer should be unchanged
	if len(buf.Lines) != 1 {
		t.Errorf("expected 1 line unchanged, got %d", len(buf.Lines))
	}
	if string(buf.Lines[0]) != "hello world" {
		t.Errorf("expected line unchanged 'hello world', got '%s'", string(buf.Lines[0]))
	}
}

// TestPaste_AfterYankLine verifies the full yy then p flow via key events.
func TestPaste_AfterYankLine(t *testing.T) {
	env := newTestEditor(t, "first line", "second line", "third line")
	ed := env.Editor
	buf := ed.activeBuffer()

	// yy to yank current line (row 0)
	pressRune(ed, 'y')
	pressRune(ed, 'y')

	if len(ed.clipboard) != 1 {
		t.Fatalf("expected 1 clipboard line after yy, got %d", len(ed.clipboard))
	}
	if string(ed.clipboard[0]) != "first line" {
		t.Errorf("expected 'first line' in clipboard, got '%s'", string(ed.clipboard[0]))
	}
	if !ed.clipboardLine {
		t.Error("clipboardLine should be true after yy")
	}

	// p to paste after
	pressRune(ed, 'p')

	// Should have 4 lines now
	if len(buf.Lines) != 4 {
		t.Errorf("expected 4 lines after yy+p, got %d", len(buf.Lines))
	}

	// "first line" should be duplicated at row 1
	if string(buf.Lines[0]) != "first line" {
		t.Errorf("expected line[0]='first line', got '%s'", string(buf.Lines[0]))
	}
	if string(buf.Lines[1]) != "first line" {
		t.Errorf("expected line[1]='first line' (pasted), got '%s'", string(buf.Lines[1]))
	}
	if string(buf.Lines[2]) != "second line" {
		t.Errorf("expected line[2]='second line', got '%s'", string(buf.Lines[2]))
	}

	// Cursor should be at the pasted line
	if ed.activePane().CursorRow != 1 {
		t.Errorf("expected cursor at row 1, got %d", ed.activePane().CursorRow)
	}
}

// TestPaste_AfterDeleteLine verifies the full dd then p flow via key events.
// Deleting a line and pasting it should restore the line below the current cursor.
func TestPaste_AfterDeleteLine(t *testing.T) {
	env := newTestEditor(t, "first line", "second line", "third line")
	ed := env.Editor
	buf := ed.activeBuffer()

	// dd to delete the first line
	pressRune(ed, 'd')
	pressRune(ed, 'd')

	// Should have 2 lines now
	if len(buf.Lines) != 2 {
		t.Fatalf("expected 2 lines after dd, got %d", len(buf.Lines))
	}
	if string(buf.Lines[0]) != "second line" {
		t.Errorf("expected line[0]='second line' after dd, got '%s'", string(buf.Lines[0]))
	}

	// Clipboard should contain the deleted line
	if len(ed.clipboard) != 1 {
		t.Fatalf("expected 1 clipboard line after dd, got %d", len(ed.clipboard))
	}
	if string(ed.clipboard[0]) != "first line" {
		t.Errorf("expected clipboard='first line', got '%s'", string(ed.clipboard[0]))
	}
	if !ed.clipboardLine {
		t.Error("clipboardLine should be true after dd")
	}

	// p to paste the deleted line below current cursor (row 0 = "second line")
	pressRune(ed, 'p')

	// Should have 3 lines again
	if len(buf.Lines) != 3 {
		t.Errorf("expected 3 lines after dd+p, got %d", len(buf.Lines))
	}

	// "first line" should now be at row 1 (below "second line")
	if string(buf.Lines[0]) != "second line" {
		t.Errorf("expected line[0]='second line', got '%s'", string(buf.Lines[0]))
	}
	if string(buf.Lines[1]) != "first line" {
		t.Errorf("expected line[1]='first line' (restored), got '%s'", string(buf.Lines[1]))
	}
	if string(buf.Lines[2]) != "third line" {
		t.Errorf("expected line[2]='third line', got '%s'", string(buf.Lines[2]))
	}

	// Cursor should be at the pasted line
	if ed.activePane().CursorRow != 1 {
		t.Errorf("expected cursor at row 1, got %d", ed.activePane().CursorRow)
	}
}
