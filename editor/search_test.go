package editor

import (
	"testing"
)

func TestFindFirstMatchAfterCursor(t *testing.T) {
	matches := []SearchMatch{
		{Row: 0, Col: 5, Length: 3},
		{Row: 2, Col: 10, Length: 3},
		{Row: 5, Col: 0, Length: 3},
	}

	tests := []struct {
		name      string
		cursorRow int
		cursorCol int
		want      int
	}{
		{"before all matches", 0, 0, 0},
		{"at first match", 0, 5, 0},
		{"between matches", 1, 0, 1},
		{"at second match", 2, 10, 1},
		{"after second match", 3, 0, 2},
		{"after all matches wraps to 0", 6, 0, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := FindFirstMatchAfterCursor(matches, tt.cursorRow, tt.cursorCol)
			if got != tt.want {
				t.Errorf("got %d, want %d", got, tt.want)
			}
		})
	}

	t.Run("empty matches", func(t *testing.T) {
		got := FindFirstMatchAfterCursor(nil, 0, 0)
		if got != -1 {
			t.Errorf("got %d, want -1", got)
		}
	})
}
