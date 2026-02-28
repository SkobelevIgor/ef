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

func TestGetIndentLevel(t *testing.T) {
	tests := []struct {
		name       string
		line       []rune
		shiftWidth int
		expandTab  bool
		want       int
	}{
		{"no indent", []rune("hello"), 4, true, 0},
		{"one tab", []rune("\thello"), 4, false, 1},
		{"two tabs", []rune("\t\thello"), 4, false, 2},
		{"4 spaces", []rune("    hello"), 4, true, 1},
		{"8 spaces", []rune("        hello"), 4, true, 2},
		{"2 spaces with sw=2", []rune("  hello"), 2, true, 1},
		{"mixed tab and spaces", []rune("\t    hello"), 4, true, 2},
		{"empty line", []rune{}, 4, true, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			buf := &Buffer{
				Lines: [][]rune{tt.line},
				Config: FileTypeConfig{
					ShiftWidth: tt.shiftWidth,
					ExpandTab:  tt.expandTab,
				},
			}
			got := buf.getIndentLevel(tt.line)
			if got != tt.want {
				t.Errorf("got %d, want %d", got, tt.want)
			}
		})
	}
}

func TestMakeIndent(t *testing.T) {
	tests := []struct {
		name       string
		level      int
		expandTab  bool
		shiftWidth int
		want       string
	}{
		{"zero level", 0, true, 4, ""},
		{"negative level", -1, true, 4, ""},
		{"tabs level 1", 1, false, 4, "\t"},
		{"tabs level 2", 2, false, 4, "\t\t"},
		{"spaces level 1 sw=4", 1, true, 4, "    "},
		{"spaces level 2 sw=2", 2, true, 2, "    "},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			buf := &Buffer{
				Lines: [][]rune{{}},
				Config: FileTypeConfig{
					ShiftWidth: tt.shiftWidth,
					ExpandTab:  tt.expandTab,
				},
			}
			got := string(buf.makeIndent(tt.level))
			if got != tt.want {
				t.Errorf("got %q, want %q", got, tt.want)
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
