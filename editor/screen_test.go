package editor

import (
	"testing"
)

func TestGetVisualColumnScreen(t *testing.T) {
	tests := []struct {
		name    string
		line    []rune
		charCol int
		tabStop int
		want    int
	}{
		{"no tabs", []rune("hello"), 3, 4, 3},
		{"single tab", []rune("\thello"), 1, 4, 4},
		{"tab after chars", []rune("ab\tcd"), 2, 4, 2},
		{"tab after chars expanded", []rune("ab\tcd"), 3, 4, 4},
		{"empty line", []rune{}, 0, 4, 0},
		{"default tabstop on zero", []rune("\tx"), 1, 0, 4},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := getVisualColumn(tt.line, tt.charCol, tt.tabStop)
			if got != tt.want {
				t.Errorf("getVisualColumn(%q, %d, %d) = %d, want %d",
					string(tt.line), tt.charCol, tt.tabStop, got, tt.want)
			}
		})
	}
}

func TestGetVisualLineWidth(t *testing.T) {
	tests := []struct {
		name    string
		line    []rune
		tabStop int
		want    int
	}{
		{"simple text", []rune("hello"), 4, 5},
		{"with tab", []rune("a\tb"), 4, 5}, // a=1, tab=3 to next stop, b=1
		{"only tab", []rune("\t"), 4, 4},
		{"empty", []rune{}, 4, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := getVisualLineWidth(tt.line, tt.tabStop)
			if got != tt.want {
				t.Errorf("getVisualLineWidth(%q, %d) = %d, want %d",
					string(tt.line), tt.tabStop, got, tt.want)
			}
		})
	}
}

func TestLineNumberWidth(t *testing.T) {
	tests := []struct {
		totalLines int
		want       int
	}{
		{1, 4},   // min width 3 + 1 space = 4
		{9, 4},   // 1 digit, min 3 + 1 = 4
		{10, 4},  // 2 digits, min 3 + 1 = 4
		{99, 4},  // 2 digits, min 3 + 1 = 4
		{100, 4}, // 3 digits = 3 + 1 = 4
		{999, 4},
		{1000, 5}, // 4 digits + 1 = 5
		{9999, 5},
		{10000, 6}, // 5 digits + 1 = 6
	}

	for _, tt := range tests {
		got := lineNumberWidth(tt.totalLines)
		if got != tt.want {
			t.Errorf("lineNumberWidth(%d) = %d, want %d", tt.totalLines, got, tt.want)
		}
	}
}
