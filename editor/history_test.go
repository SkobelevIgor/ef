package editor

import (
	"testing"
)

func TestNewHistory(t *testing.T) {
	t.Run("positive maxSize", func(t *testing.T) {
		h := NewHistory(50)
		if h.maxSize != 50 {
			t.Errorf("maxSize = %d, want 50", h.maxSize)
		}
	})

	t.Run("zero maxSize defaults to 100", func(t *testing.T) {
		h := NewHistory(0)
		if h.maxSize != 100 {
			t.Errorf("maxSize = %d, want 100", h.maxSize)
		}
	})

	t.Run("negative maxSize defaults to 100", func(t *testing.T) {
		h := NewHistory(-5)
		if h.maxSize != 100 {
			t.Errorf("maxSize = %d, want 100", h.maxSize)
		}
	})
}

func TestPush(t *testing.T) {
	t.Run("nil change is ignored", func(t *testing.T) {
		h := NewHistory(10)
		h.Push(nil)
		if h.CanUndo() {
			t.Error("should not have undo after pushing nil")
		}
	})

	t.Run("push clears redo stack", func(t *testing.T) {
		h := NewHistory(10)
		h.Push(&Change{Type: ChangeInsert})
		h.Undo()
		if !h.CanRedo() {
			t.Fatal("should have redo")
		}
		h.Push(&Change{Type: ChangeInsert})
		if h.CanRedo() {
			t.Error("redo should be cleared after new push")
		}
	})

	t.Run("respects maxSize", func(t *testing.T) {
		h := NewHistory(3)
		for i := 0; i < 5; i++ {
			h.Push(&Change{Type: ChangeInsert, Row: i})
		}
		if len(h.undoStack) != 3 {
			t.Errorf("undoStack len = %d, want 3", len(h.undoStack))
		}
		// Should keep the last 3 (rows 2, 3, 4)
		if h.undoStack[0].Row != 2 {
			t.Errorf("first item Row = %d, want 2", h.undoStack[0].Row)
		}
	})
}

func TestUndoRedo(t *testing.T) {
	h := NewHistory(10)

	t.Run("undo empty returns nil", func(t *testing.T) {
		if h.Undo() != nil {
			t.Error("expected nil from empty undo")
		}
	})

	t.Run("redo empty returns nil", func(t *testing.T) {
		if h.Redo() != nil {
			t.Error("expected nil from empty redo")
		}
	})

	t.Run("undo returns last pushed", func(t *testing.T) {
		h.Push(&Change{Type: ChangeInsert, Row: 1})
		h.Push(&Change{Type: ChangeDelete, Row: 2})
		c := h.Undo()
		if c.Row != 2 || c.Type != ChangeDelete {
			t.Errorf("got row=%d type=%d, want row=2 type=ChangeDelete", c.Row, c.Type)
		}
	})

	t.Run("redo returns last undone", func(t *testing.T) {
		c := h.Redo()
		if c.Row != 2 {
			t.Errorf("got row=%d, want 2", c.Row)
		}
	})
}

func TestCopyLines(t *testing.T) {
	t.Run("nil input", func(t *testing.T) {
		if copyLines(nil) != nil {
			t.Error("expected nil for nil input")
		}
	})

	t.Run("deep copy", func(t *testing.T) {
		original := [][]rune{[]rune("hello"), []rune("world")}
		copied := copyLines(original)

		// Modify original
		original[0][0] = 'X'

		if copied[0][0] == 'X' {
			t.Error("copy was not deep — modifying original affected copy")
		}
		if string(copied[0]) != "hello" {
			t.Errorf("got %q, want %q", string(copied[0]), "hello")
		}
	})

	t.Run("empty slice", func(t *testing.T) {
		got := copyLines([][]rune{})
		if got == nil || len(got) != 0 {
			t.Errorf("expected empty non-nil slice, got %v", got)
		}
	})
}

func TestHistorySession(t *testing.T) {
	t.Run("CommitSession with changes pushes one entry", func(t *testing.T) {
		h := NewHistory(10)
		buf := newTestBuffer("hello", "world")
		lines := buf.Lines

		h.StartSession(buf, 0, 0, lines)
		if !h.inSession {
			t.Fatal("expected inSession to be true after StartSession")
		}

		// Simulate modification: change first line
		modified := [][]rune{[]rune("hello modified"), []rune("world")}
		h.CommitSession(modified)

		if h.inSession {
			t.Error("expected inSession to be false after CommitSession")
		}
		if !h.CanUndo() {
			t.Fatal("expected an undo entry after CommitSession with changes")
		}
		if len(h.undoStack) != 1 {
			t.Errorf("undoStack len = %d, want 1", len(h.undoStack))
		}
	})

	t.Run("CommitSession with no changes does not push", func(t *testing.T) {
		h := NewHistory(10)
		buf := newTestBuffer("hello", "world")
		lines := buf.Lines

		h.StartSession(buf, 0, 0, lines)
		// Commit with identical content
		h.CommitSession(lines)

		if h.CanUndo() {
			t.Error("expected no undo entry after CommitSession with no changes")
		}
	})
}

func TestHistorySessionWithChanges(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("foo", "bar")
	originalLines := buf.Lines

	h.StartSession(buf, 1, 2, originalLines)

	// Simulate user typing: "bar" becomes "baz"
	modifiedLines := [][]rune{[]rune("foo"), []rune("baz")}
	h.CommitSession(modifiedLines)

	if !h.CanUndo() {
		t.Fatal("expected undo to be available")
	}

	c := h.Undo()
	if c == nil {
		t.Fatal("Undo returned nil")
	}
	if c.Type != ChangeReplace {
		t.Errorf("change type = %v, want ChangeReplace", c.Type)
	}
	if c.Row != 1 {
		t.Errorf("change Row = %d, want 1", c.Row)
	}
	if c.Col != 2 {
		t.Errorf("change Col = %d, want 2", c.Col)
	}
	if string(c.OldText[1]) != "bar" {
		t.Errorf("OldText[1] = %q, want %q", string(c.OldText[1]), "bar")
	}
	if string(c.Text[1]) != "baz" {
		t.Errorf("Text[1] = %q, want %q", string(c.Text[1]), "baz")
	}
	if c.Buffer != buf {
		t.Error("change Buffer does not point to the session buffer")
	}
}

func TestHistoryPushDuringSession(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("line1")

	h.StartSession(buf, 0, 0, buf.Lines)

	// The first push during a session goes to undoStack because pending is nil.
	first := &Change{Type: ChangeInsert, Row: 0, Col: 0}
	h.Push(first)

	if len(h.undoStack) != 1 {
		t.Errorf("undoStack len = %d, want 1 (first push during session goes to undoStack)", len(h.undoStack))
	}

	// Manually set pending to simulate subsequent session push behaviour.
	h.pending = first

	second := &Change{Type: ChangeDelete, Row: 1, Col: 0}
	h.Push(second)

	// With pending already set, a second push replaces pending and does NOT append to undoStack.
	if h.pending != second {
		t.Error("expected pending to be replaced by the second change pushed")
	}
	if len(h.undoStack) != 1 {
		t.Errorf("undoStack len = %d, want 1 after second push during session (pending replaces, not appends)", len(h.undoStack))
	}
}

func TestHistoryRecordInsert(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("hello")
	text := [][]rune{[]rune("inserted")}

	h.RecordInsert(buf, 3, 5, text)

	if !h.CanUndo() {
		t.Fatal("expected undo after RecordInsert")
	}
	c := h.Undo()
	if c == nil {
		t.Fatal("Undo returned nil")
	}
	if c.Type != ChangeInsert {
		t.Errorf("change type = %v, want ChangeInsert", c.Type)
	}
	if c.Row != 3 {
		t.Errorf("Row = %d, want 3", c.Row)
	}
	if c.Col != 5 {
		t.Errorf("Col = %d, want 5", c.Col)
	}
	if string(c.Text[0]) != "inserted" {
		t.Errorf("Text[0] = %q, want %q", string(c.Text[0]), "inserted")
	}
	if c.Buffer != buf {
		t.Error("Buffer pointer mismatch")
	}
}

func TestHistoryRecordDelete(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("hello world")
	text := [][]rune{[]rune("world")}

	h.RecordDelete(buf, 0, 6, text)

	if !h.CanUndo() {
		t.Fatal("expected undo after RecordDelete")
	}
	c := h.Undo()
	if c == nil {
		t.Fatal("Undo returned nil")
	}
	if c.Type != ChangeDelete {
		t.Errorf("change type = %v, want ChangeDelete", c.Type)
	}
	if c.Row != 0 {
		t.Errorf("Row = %d, want 0", c.Row)
	}
	if c.Col != 6 {
		t.Errorf("Col = %d, want 6", c.Col)
	}
	if string(c.Text[0]) != "world" {
		t.Errorf("Text[0] = %q, want %q", string(c.Text[0]), "world")
	}
	if c.LineDeletion {
		t.Error("LineDeletion should be false for RecordDelete")
	}
}

func TestHistoryRecordDeleteLines(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("line one", "line two", "line three")
	deleted := [][]rune{[]rune("line two")}

	h.RecordDeleteLines(buf, 1, deleted)

	if !h.CanUndo() {
		t.Fatal("expected undo after RecordDeleteLines")
	}
	c := h.Undo()
	if c == nil {
		t.Fatal("Undo returned nil")
	}
	if c.Type != ChangeDelete {
		t.Errorf("change type = %v, want ChangeDelete", c.Type)
	}
	if !c.LineDeletion {
		t.Error("LineDeletion should be true for RecordDeleteLines")
	}
	if c.Row != 1 {
		t.Errorf("Row = %d, want 1", c.Row)
	}
	if c.Col != 0 {
		t.Errorf("Col = %d, want 0", c.Col)
	}
	if string(c.Text[0]) != "line two" {
		t.Errorf("Text[0] = %q, want %q", string(c.Text[0]), "line two")
	}
}

func TestHistoryCanUndoRedo(t *testing.T) {
	h := NewHistory(10)

	// Initially both false
	if h.CanUndo() {
		t.Error("CanUndo should be false on fresh history")
	}
	if h.CanRedo() {
		t.Error("CanRedo should be false on fresh history")
	}

	// After push: CanUndo true, CanRedo still false
	h.Push(&Change{Type: ChangeInsert, Row: 0})
	if !h.CanUndo() {
		t.Error("CanUndo should be true after push")
	}
	if h.CanRedo() {
		t.Error("CanRedo should be false after push with no prior undo")
	}

	// After undo: CanUndo false, CanRedo true
	h.Undo()
	if h.CanUndo() {
		t.Error("CanUndo should be false after undoing the only change")
	}
	if !h.CanRedo() {
		t.Error("CanRedo should be true after undo")
	}

	// After redo: back to CanUndo true, CanRedo false
	h.Redo()
	if !h.CanUndo() {
		t.Error("CanUndo should be true after redo")
	}
	if h.CanRedo() {
		t.Error("CanRedo should be false after redo")
	}

	// After new push: redo stack is cleared
	h.Undo()
	h.Push(&Change{Type: ChangeDelete, Row: 1})
	if h.CanRedo() {
		t.Error("CanRedo should be false after new push clears redo stack")
	}
}

func TestHistoryCommitSessionNoChanges(t *testing.T) {
	h := NewHistory(10)
	buf := newTestBuffer("unchanged line")
	snapshot := buf.Lines

	h.StartSession(buf, 0, 0, snapshot)
	// Pass back the exact same content
	h.CommitSession(snapshot)

	if h.CanUndo() {
		t.Error("no undo entry expected when buffer content did not change")
	}
	if len(h.undoStack) != 0 {
		t.Errorf("undoStack len = %d, want 0", len(h.undoStack))
	}
}
