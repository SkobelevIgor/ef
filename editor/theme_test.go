package editor

import "testing"

func TestCurrentLineNumStyle(t *testing.T) {
	// Normal mode
	style := CurrentLineNumStyle(ModeNormal)
	if style != NormalLineNumStyle {
		t.Error("ModeNormal should return NormalLineNumStyle")
	}

	// Insert mode
	style = CurrentLineNumStyle(ModeInsert)
	if style != InsertLineNumStyle {
		t.Error("ModeInsert should return InsertLineNumStyle")
	}

	// Visual mode
	style = CurrentLineNumStyle(ModeVisual)
	if style != VisualLineNumStyle {
		t.Error("ModeVisual should return VisualLineNumStyle")
	}

	// Unknown mode defaults to normal
	style = CurrentLineNumStyle(Mode(99))
	if style != NormalLineNumStyle {
		t.Error("unknown mode should default to NormalLineNumStyle")
	}
}
