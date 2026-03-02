package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

func enterInsert(ed *Editor) {
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'i', tcell.ModNone))
}

func TestInsertMode_TypeCharacter(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	enterInsert(ed)
	// Type 'X' at position 0
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'X', tcell.ModNone))
	got := string(ed.activeBuffer().Lines[0])
	if got != "Xhello" {
		t.Errorf("expected 'Xhello', got '%s'", got)
	}
}

func TestInsertMode_Backspace(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	// Move to col 1 then enter insert mode
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	enterInsert(ed)
	ed.handleKey(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	got := string(ed.activeBuffer().Lines[0])
	if got != "ello" {
		t.Errorf("expected 'ello', got '%s'", got)
	}
}

func TestInsertMode_Enter(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	// Move to col 3 then insert mode, then Enter
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	ed.handleKey(tcell.NewEventKey(tcell.KeyRune, 'l', tcell.ModNone))
	enterInsert(ed)
	ed.handleKey(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))
	if len(ed.activeBuffer().Lines) != 2 {
		t.Errorf("expected 2 lines after Enter, got %d", len(ed.activeBuffer().Lines))
	}
}

func TestInsertMode_Tab(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	enterInsert(ed)
	ed.handleKey(tcell.NewEventKey(tcell.KeyTab, 0, tcell.ModNone))
	line := ed.activeBuffer().Lines[0]
	// Tab should insert either a tab char or spaces at the beginning
	if len(line) <= 5 {
		t.Error("Tab should have inserted indentation")
	}
}

func TestInsertMode_EscReturnsToNormal(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	enterInsert(ed)
	if ed.mode != ModeInsert {
		t.Fatal("should be in insert mode")
	}

	ed.handleKey(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))
	if ed.mode != ModeNormal {
		t.Errorf("Esc should return to normal mode, got %v", ed.mode)
	}
}
