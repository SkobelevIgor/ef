package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

func TestNewEditorWithDeps(t *testing.T) {
	// Verify Editor creates successfully with all mocks
	env := newTestEditor(t, "hello", "world")
	if env.Editor == nil {
		t.Fatal("Editor should not be nil")
	}
	if env.Editor.mode != ModeNormal {
		t.Errorf("expected ModeNormal, got %v", env.Editor.mode)
	}
	if len(env.Editor.panes) != 1 {
		t.Errorf("expected 1 pane, got %d", len(env.Editor.panes))
	}
	if env.Editor.activePane().CursorRow != 0 || env.Editor.activePane().CursorCol != 0 {
		t.Error("cursor should start at 0,0")
	}
}

func TestEditorHandleKey_F10Quits(t *testing.T) {
	env := newTestEditor(t, "hello")
	ev := tcell.NewEventKey(tcell.KeyF10, 0, tcell.ModNone)
	quit := env.Editor.handleKey(ev)
	if !quit {
		t.Error("F10 should return true (quit)")
	}
}

func TestEditorModeTransitions(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor

	// Start in normal mode
	if ed.mode != ModeNormal {
		t.Fatalf("should start in ModeNormal")
	}

	// 'i' → insert mode
	ev := tcell.NewEventKey(tcell.KeyRune, 'i', tcell.ModNone)
	ed.handleKey(ev)
	if ed.mode != ModeInsert {
		t.Errorf("expected ModeInsert after 'i', got %v", ed.mode)
	}

	// Esc → normal mode
	ev = tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone)
	ed.handleKey(ev)
	if ed.mode != ModeNormal {
		t.Errorf("expected ModeNormal after Esc, got %v", ed.mode)
	}

	// 'v' → visual mode
	ev = tcell.NewEventKey(tcell.KeyRune, 'v', tcell.ModNone)
	ed.handleKey(ev)
	if ed.mode != ModeVisual {
		t.Errorf("expected ModeVisual after 'v', got %v", ed.mode)
	}

	// Esc → normal mode
	ev = tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone)
	ed.handleKey(ev)
	if ed.mode != ModeNormal {
		t.Errorf("expected ModeNormal after Esc from visual, got %v", ed.mode)
	}
}
