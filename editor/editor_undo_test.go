package editor

import (
	"testing"
)

// ---------------------------------------------------------------------------
// Helper: linesToStrings converts [][]rune to []string for easy comparison.
// ---------------------------------------------------------------------------
func linesToStrings(lines [][]rune) []string {
	result := make([]string, len(lines))
	for i, l := range lines {
		result[i] = string(l)
	}
	return result
}

// ---------------------------------------------------------------------------
// applyRemoveText tests
// ---------------------------------------------------------------------------

func TestApplyRemoveText_SingleLine(t *testing.T) {
	// Buffer: "hello world"
	// Change: ChangeInsert at row=0, col=5, Text=[" world"] (6 chars)
	// After applyRemoveText the line should become "hello".
	buf := newTestBuffer("hello world")
	change := &Change{
		Type: ChangeInsert,
		Row:  0,
		Col:  5,
		Text: [][]rune{[]rune(" world")},
	}

	applyRemoveText(buf, change)

	got := string(buf.Lines[0])
	if got != "hello" {
		t.Errorf("applyRemoveText single-line: got %q, want %q", got, "hello")
	}
}

func TestApplyRemoveText_MultiLine(t *testing.T) {
	// Buffer represents the state after a multi-line insert:
	//   line 0: "helloXXX"
	//   line 1: "YYY"
	//   line 2: " world"
	// The insert recorded in Change started at row=0, col=5 and spans 3 text pieces:
	//   Text = ["XXX", "YYY", ""]
	// After removal the buffer should be:
	//   line 0: "hello world"
	//   line 1: " world"      <- whatever was on the original last row after the text
	//
	// Let's use a simpler, direct scenario matching the actual logic:
	//   Buffer: ["helloXXX", "YYYworld"]
	//   Change.Row=0, Change.Col=5, Change.Text=["XXX","YYY"]
	//   After remove: the two rows collapse back to one: "helloworld"
	buf := newTestBuffer("helloXXX", "YYYworld")
	change := &Change{
		Type: ChangeInsert,
		Row:  0,
		Col:  5,
		Text: [][]rune{[]rune("XXX"), []rune("YYY")},
	}

	applyRemoveText(buf, change)

	if len(buf.Lines) != 1 {
		t.Fatalf("applyRemoveText multi-line: expected 1 line, got %d: %v", len(buf.Lines), linesToStrings(buf.Lines))
	}
	got := string(buf.Lines[0])
	if got != "helloworld" {
		t.Errorf("applyRemoveText multi-line: got %q, want %q", got, "helloworld")
	}
}

// ---------------------------------------------------------------------------
// applyInsertText tests
// ---------------------------------------------------------------------------

func TestApplyInsertText_SingleLine(t *testing.T) {
	// Buffer: "helloworld"
	// Insert at col=5: " " (space), making "hello world"
	buf := newTestBuffer("helloworld")
	change := &Change{
		Type: ChangeInsert,
		Row:  0,
		Col:  5,
		Text: [][]rune{[]rune(" ")},
	}

	applyInsertText(buf, change)

	got := string(buf.Lines[0])
	if got != "hello world" {
		t.Errorf("applyInsertText single-line: got %q, want %q", got, "hello world")
	}
}

func TestApplyInsertText_MultiLine(t *testing.T) {
	// Buffer: ["helloworld"]
	// Insert at row=0, col=5 with Text=["XXX","YYY"]
	// Result should split the line: ["helloXXX", "YYYworld"]
	buf := newTestBuffer("helloworld")
	change := &Change{
		Type: ChangeInsert,
		Row:  0,
		Col:  5,
		Text: [][]rune{[]rune("XXX"), []rune("YYY")},
	}

	applyInsertText(buf, change)

	if len(buf.Lines) != 2 {
		t.Fatalf("applyInsertText multi-line: expected 2 lines, got %d: %v", len(buf.Lines), linesToStrings(buf.Lines))
	}
	if got := string(buf.Lines[0]); got != "helloXXX" {
		t.Errorf("applyInsertText multi-line line 0: got %q, want %q", got, "helloXXX")
	}
	if got := string(buf.Lines[1]); got != "YYYworld" {
		t.Errorf("applyInsertText multi-line line 1: got %q, want %q", got, "YYYworld")
	}
}

func TestApplyInsertText_LineDeletion(t *testing.T) {
	// Buffer: ["first", "third"]
	// LineDeletion insert at row=1 with Text=["second"] inserts whole lines
	// Result: ["first", "second", "third"]
	buf := newTestBuffer("first", "third")
	change := &Change{
		Type:         ChangeDelete,
		Row:          1,
		Col:          0,
		Text:         [][]rune{[]rune("second")},
		LineDeletion: true,
	}

	applyInsertText(buf, change)

	if len(buf.Lines) != 3 {
		t.Fatalf("applyInsertText line-deletion: expected 3 lines, got %d: %v", len(buf.Lines), linesToStrings(buf.Lines))
	}
	if got := string(buf.Lines[0]); got != "first" {
		t.Errorf("line 0: got %q, want %q", got, "first")
	}
	if got := string(buf.Lines[1]); got != "second" {
		t.Errorf("line 1: got %q, want %q", got, "second")
	}
	if got := string(buf.Lines[2]); got != "third" {
		t.Errorf("line 2: got %q, want %q", got, "third")
	}
}

// ---------------------------------------------------------------------------
// undo / redo at Editor level
// ---------------------------------------------------------------------------

func TestUndo_InsertChange(t *testing.T) {
	// Simulate: text " world" was inserted at row=0, col=5.
	// Buffer currently reads "hello world"; undo should give back "hello".
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	buf := ed.activeBuffer()

	ed.history.Push(&Change{
		Type:   ChangeInsert,
		Buffer: buf,
		Row:    0,
		Col:    5,
		Text:   [][]rune{[]rune(" world")},
	})

	ed.undo()

	got := string(buf.Lines[0])
	if got != "hello" {
		t.Errorf("TestUndo_InsertChange: after undo got %q, want %q", got, "hello")
	}
	if ed.activePane().CursorRow != 0 || ed.activePane().CursorCol != 5 {
		t.Errorf("TestUndo_InsertChange: cursor should be at (0,5), got (%d,%d)",
			ed.activePane().CursorRow, ed.activePane().CursorCol)
	}
}

func TestUndo_DeleteChange(t *testing.T) {
	// Simulate: text " world" was deleted from row=0, col=5.
	// Buffer currently reads "hello"; undo should restore " world".
	env := newTestEditor(t, "hello")
	ed := env.Editor
	buf := ed.activeBuffer()

	ed.history.Push(&Change{
		Type:   ChangeDelete,
		Buffer: buf,
		Row:    0,
		Col:    5,
		Text:   [][]rune{[]rune(" world")},
	})

	ed.undo()

	got := string(buf.Lines[0])
	if got != "hello world" {
		t.Errorf("TestUndo_DeleteChange: after undo got %q, want %q", got, "hello world")
	}
}

func TestUndo_ReplaceChange(t *testing.T) {
	// Simulate: entire buffer was replaced (ChangeReplace).
	// OldText = ["original line"], Text = ["new line"].
	// Buffer currently holds "new line"; undo should restore "original line".
	env := newTestEditor(t, "new line")
	ed := env.Editor
	buf := ed.activeBuffer()

	ed.history.Push(&Change{
		Type:    ChangeReplace,
		Buffer:  buf,
		Row:     0,
		Col:     0,
		Text:    [][]rune{[]rune("new line")},
		OldText: [][]rune{[]rune("original line")},
	})

	ed.undo()

	got := string(buf.Lines[0])
	if got != "original line" {
		t.Errorf("TestUndo_ReplaceChange: after undo got %q, want %q", got, "original line")
	}
}

func TestUndo_Empty(t *testing.T) {
	// Undo with an empty history should be a no-op and not panic.
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	buf := ed.activeBuffer()

	before := linesToStrings(buf.Lines)
	ed.undo() // should not panic
	after := linesToStrings(buf.Lines)

	if len(before) != len(after) {
		t.Fatalf("TestUndo_Empty: line count changed: %d -> %d", len(before), len(after))
	}
	for i := range before {
		if before[i] != after[i] {
			t.Errorf("TestUndo_Empty: line %d changed from %q to %q", i, before[i], after[i])
		}
	}
}

func TestRedo_AfterUndo(t *testing.T) {
	// Push an insert change, undo it (removes text), then redo (re-inserts text).
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	buf := ed.activeBuffer()

	ed.history.Push(&Change{
		Type:   ChangeInsert,
		Buffer: buf,
		Row:    0,
		Col:    5,
		Text:   [][]rune{[]rune(" world")},
	})

	// Undo: line becomes "hello"
	ed.undo()
	if got := string(buf.Lines[0]); got != "hello" {
		t.Fatalf("TestRedo_AfterUndo: after undo expected %q, got %q", "hello", got)
	}

	// Redo: line becomes "hello world" again
	ed.redo()
	if got := string(buf.Lines[0]); got != "hello world" {
		t.Errorf("TestRedo_AfterUndo: after redo expected %q, got %q", "hello world", got)
	}
}

func TestRedo_Empty(t *testing.T) {
	// Redo with an empty redo stack should be a no-op and not panic.
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	buf := ed.activeBuffer()

	before := linesToStrings(buf.Lines)
	ed.redo() // should not panic
	after := linesToStrings(buf.Lines)

	if len(before) != len(after) {
		t.Fatalf("TestRedo_Empty: line count changed: %d -> %d", len(before), len(after))
	}
	for i := range before {
		if before[i] != after[i] {
			t.Errorf("TestRedo_Empty: line %d changed from %q to %q", i, before[i], after[i])
		}
	}
}

func TestUndoRedo_LineLevel(t *testing.T) {
	// Simulate dd (whole-line deletion):
	//   Original buffer: ["first", "second", "third"]
	//   After dd on row 1: ["first", "third"]
	//   Change: ChangeDelete, LineDeletion=true, Row=1, Text=["second"]
	//
	// Start from the already-deleted state and verify undo / redo cycle.
	env := newTestEditor(t, "first", "third")
	ed := env.Editor
	buf := ed.activeBuffer()

	// Record the deletion that was already applied to arrive at ["first","third"].
	ed.history.Push(&Change{
		Type:         ChangeDelete,
		Buffer:       buf,
		Row:          1,
		Col:          0,
		Text:         [][]rune{[]rune("second")},
		LineDeletion: true,
	})

	// --- undo: "second" should be restored ---
	ed.undo()
	if len(buf.Lines) != 3 {
		t.Fatalf("TestUndoRedo_LineLevel: after undo expected 3 lines, got %d: %v",
			len(buf.Lines), linesToStrings(buf.Lines))
	}
	if got := string(buf.Lines[1]); got != "second" {
		t.Errorf("TestUndoRedo_LineLevel: after undo line 1 = %q, want %q", got, "second")
	}

	// --- redo: "second" should be removed again ---
	ed.redo()
	if len(buf.Lines) != 2 {
		t.Fatalf("TestUndoRedo_LineLevel: after redo expected 2 lines, got %d: %v",
			len(buf.Lines), linesToStrings(buf.Lines))
	}
	if got := string(buf.Lines[0]); got != "first" {
		t.Errorf("TestUndoRedo_LineLevel: after redo line 0 = %q, want %q", got, "first")
	}
	if got := string(buf.Lines[1]); got != "third" {
		t.Errorf("TestUndoRedo_LineLevel: after redo line 1 = %q, want %q", got, "third")
	}
}
