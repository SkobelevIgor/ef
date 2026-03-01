package editor

import (
	"testing"
)

func TestIsLineEmpty(t *testing.T) {
	// NOTE: hasContent is misnamed — it returns true when line has non-whitespace content
	tests := []struct {
		name string
		line []rune
		want bool
	}{
		{"empty line", []rune{}, false},
		{"whitespace only", []rune("   \t  "), false},
		{"has content", []rune("  hello  "), true},
		{"only content", []rune("x"), true},
		{"tabs and spaces then content", []rune("\t  a"), true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := hasContent(tt.line)
			if got != tt.want {
				t.Errorf("hasContent(%q) = %v, want %v", string(tt.line), got, tt.want)
			}
		})
	}
}

func TestGetVisualColumn(t *testing.T) {
	tests := []struct {
		name    string
		line    []rune
		charCol int
		tabStop int
		want    int
	}{
		{"no tabs", []rune("hello"), 3, 4, 3},
		{"tab at start", []rune("\thello"), 1, 4, 4},
		{"tab after text", []rune("ab\thello"), 3, 4, 4},
		{"two tabs", []rune("\t\thello"), 2, 8, 16},
		{"empty line", []rune{}, 0, 4, 0},
		{"col beyond line", []rune("hi"), 5, 4, 2},
		{"tab stop 2", []rune("\thello"), 1, 2, 2},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			buf := &Buffer{
				Lines: [][]rune{tt.line},
				Config: FileTypeConfig{
					TabStop: tt.tabStop,
				},
			}
			got := buf.GetVisualColumn(tt.line, tt.charCol)
			if got != tt.want {
				t.Errorf("got %d, want %d", got, tt.want)
			}
		})
	}
}
