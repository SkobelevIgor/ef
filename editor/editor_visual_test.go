package editor

import (
	"testing"
	"github.com/gdamore/tcell/v2"
)

func TestVisualMode_SelectionExpand(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// Enter visual mode
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'v', tcell.ModNone))
	if ed.mode != ModeVisual {
		t.Fatal("should be in visual mode")
	}
	if !ed.activePane().SelectionActive {
		t.Fatal("selection should be active")
	}

	// Move right to expand selection
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	if ed.activePane().CursorCol != 2 {
		t.Errorf("expected col 2, got %d", ed.activePane().CursorCol)
	}
}

func TestVisualMode_YankSelection(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// Enter visual mode and select "hel"
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'v', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))

	// Yank
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'y', tcell.ModNone))
	if ed.mode != ModeNormal {
		t.Error("should return to normal mode after yank")
	}
	if len(ed.clipboard) == 0 {
		t.Fatal("clipboard should not be empty")
	}
}

func TestVisualMode_DeleteSelection(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// Enter visual mode and select "hel"
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'v', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))

	// Delete
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'd', tcell.ModNone))
	if ed.mode != ModeNormal {
		t.Error("should return to normal mode after delete")
	}
	got := string(ed.activeBuffer().Lines[0])
	if got != "lo world" {
		t.Errorf("expected 'lo world', got '%s'", got)
	}
}

func TestVisualMode_EscCancels(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'v', tcell.ModNone))
	if ed.mode != ModeVisual {
		t.Fatal("should be in visual mode")
	}

	ed.handleKey(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))
	if ed.mode != ModeNormal {
		t.Errorf("Esc should return to normal mode, got %v", ed.mode)
	}
	if ed.activePane().SelectionActive {
		t.Error("selection should be cleared")
	}
}
