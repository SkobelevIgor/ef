package editor

import "testing"

func TestNormalizeRange(t *testing.T) {
	tests := []struct {
		name                               string
		sr, sc, er, ec                     int
		wantSR, wantSC, wantER, wantEC int
	}{
		{"already normalized", 0, 0, 1, 5, 0, 0, 1, 5},
		{"reversed rows", 1, 5, 0, 0, 0, 0, 1, 5},
		{"same row reversed cols", 0, 5, 0, 2, 0, 2, 0, 5},
		{"same position", 0, 0, 0, 0, 0, 0, 0, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			sr, sc, er, ec := NormalizeRange(tt.sr, tt.sc, tt.er, tt.ec)
			if sr != tt.wantSR || sc != tt.wantSC || er != tt.wantER || ec != tt.wantEC {
				t.Errorf("got (%d,%d,%d,%d), want (%d,%d,%d,%d)", sr, sc, er, ec, tt.wantSR, tt.wantSC, tt.wantER, tt.wantEC)
			}
		})
	}
}

func TestClampPosition(t *testing.T) {
	lines := [][]rune{[]rune("hello"), []rune("world")}

	tests := []struct {
		name         string
		row, col     int
		wantR, wantC int
	}{
		{"in bounds", 0, 3, 0, 3},
		{"row too large", 5, 0, 1, 0},
		{"col too large", 0, 100, 0, 5},
		{"negative row", -1, 0, 0, 0},
		{"negative col", 0, -1, 0, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			r, c := ClampPosition(lines, tt.row, tt.col)
			if r != tt.wantR || c != tt.wantC {
				t.Errorf("got (%d,%d), want (%d,%d)", r, c, tt.wantR, tt.wantC)
			}
		})
	}

	t.Run("empty buffer", func(t *testing.T) {
		r, c := ClampPosition([][]rune{}, 5, 5)
		if r != 0 || c != 0 {
			t.Errorf("got (%d,%d), want (0,0)", r, c)
		}
	})
}

func TestIsWhitespace(t *testing.T) {
	if !isWhitespace(' ') || !isWhitespace('\t') {
		t.Error("space and tab should be whitespace")
	}
	if isWhitespace('a') || isWhitespace('\n') {
		t.Error("a and newline should not be whitespace")
	}
}

func TestIsWordChar(t *testing.T) {
	if !isWordChar('a') || !isWordChar('Z') || !isWordChar('0') || !isWordChar('_') {
		t.Error("letters, digits, underscore should be word chars")
	}
	if isWordChar(' ') || isWordChar('.') || isWordChar('-') {
		t.Error("space, dot, dash should not be word chars")
	}
}
