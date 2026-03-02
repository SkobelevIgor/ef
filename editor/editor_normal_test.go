package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

func pressKey(ed *Editor, key tcell.Key, r rune) {
	ed.handleKey(tcell.NewEventKey(key, r, tcell.ModNone))
}

func pressRune(ed *Editor, r rune) {
	pressKey(ed, tcell.KeyRune, r)
}

func TestNormalMode_CursorMovement(t *testing.T) {
	env := newTestEditor(t, "hello", "world", "three")
	ed := env.Editor

	// j moves down
	pressRune(ed, 'j')
	if ed.activePane().CursorRow != 1 {
		t.Errorf("j: expected row 1, got %d", ed.activePane().CursorRow)
	}

	// l moves right
	pressRune(ed, 'l')
	if ed.activePane().CursorCol != 1 {
		t.Errorf("l: expected col 1, got %d", ed.activePane().CursorCol)
	}

	// k moves up
	pressRune(ed, 'k')
	if ed.activePane().CursorRow != 0 {
		t.Errorf("k: expected row 0, got %d", ed.activePane().CursorRow)
	}

	// h moves left
	pressRune(ed, 'h')
	if ed.activePane().CursorCol != 0 {
		t.Errorf("h: expected col 0, got %d", ed.activePane().CursorCol)
	}
}

func TestNormalMode_WordMotions(t *testing.T) {
	env := newTestEditor(t, "hello world test")
	ed := env.Editor

	// w moves to next word
	pressRune(ed, 'w')
	if ed.activePane().CursorCol != 6 {
		t.Errorf("w: expected col 6, got %d", ed.activePane().CursorCol)
	}

	// b moves to previous word
	pressRune(ed, 'b')
	if ed.activePane().CursorCol != 0 {
		t.Errorf("b: expected col 0, got %d", ed.activePane().CursorCol)
	}
}

func TestNormalMode_LineMotions(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// $ moves to end of line
	pressRune(ed, '$')
	if ed.activePane().CursorCol != 11 {
		t.Errorf("$: expected col 11, got %d", ed.activePane().CursorCol)
	}

	// 0 moves to start of line
	pressRune(ed, '0')
	if ed.activePane().CursorCol != 0 {
		t.Errorf("0: expected col 0, got %d", ed.activePane().CursorCol)
	}
}

func TestNormalMode_DeleteChar(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor

	pressRune(ed, 'x')
	got := string(ed.activeBuffer().Lines[0])
	if got != "ello" {
		t.Errorf("x: expected 'ello', got '%s'", got)
	}
}

func TestNormalMode_DeleteLine(t *testing.T) {
	env := newTestEditor(t, "first", "second", "third")
	ed := env.Editor

	// dd deletes current line
	pressRune(ed, 'd')
	pressRune(ed, 'd')
	if len(ed.activeBuffer().Lines) != 2 {
		t.Errorf("dd: expected 2 lines, got %d", len(ed.activeBuffer().Lines))
	}
	got := string(ed.activeBuffer().Lines[0])
	if got != "second" {
		t.Errorf("dd: expected first line 'second', got '%s'", got)
	}
}

func TestNormalMode_YankLine(t *testing.T) {
	env := newTestEditor(t, "hello", "world")
	ed := env.Editor

	// yy yanks current line
	pressRune(ed, 'y')
	pressRune(ed, 'y')
	if len(ed.clipboard) != 1 {
		t.Errorf("yy: expected 1 clipboard entry, got %d", len(ed.clipboard))
	}
	if string(ed.clipboard[0]) != "hello" {
		t.Errorf("yy: expected 'hello' in clipboard, got '%s'", string(ed.clipboard[0]))
	}
	if !ed.clipboardLine {
		t.Error("yy: clipboardLine should be true")
	}
}

func TestNormalMode_PutAfter(t *testing.T) {
	env := newTestEditor(t, "hello", "world")
	ed := env.Editor

	// yy then p pastes line below
	pressRune(ed, 'y')
	pressRune(ed, 'y')
	pressRune(ed, 'p')
	if len(ed.activeBuffer().Lines) != 3 {
		t.Errorf("p: expected 3 lines, got %d", len(ed.activeBuffer().Lines))
	}
	got := string(ed.activeBuffer().Lines[1])
	if got != "hello" {
		t.Errorf("p: expected pasted line 'hello', got '%s'", got)
	}
}
