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
