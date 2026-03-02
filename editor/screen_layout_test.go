package editor

import "testing"

// ---------------------------------------------------------------------------
// calculatePaneLayoutHorizontal
// ---------------------------------------------------------------------------
func TestCalculatePaneLayoutHorizontal(t *testing.T) {
	tests := []struct {
		name          string
		numPanes      int
		width         int
		height        int
		contentStartY int
		want          []PaneLayout
	}{
		{
			name:     "zero panes returns nil",
			numPanes: 0, width: 80, height: 24, contentStartY: 0,
			want: nil,
		},
		{
			name:     "single pane gets full height",
			numPanes: 1, width: 80, height: 24, contentStartY: 0,
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 24},
			},
		},
		{
			name:     "single pane with contentStartY offset",
			numPanes: 1, width: 80, height: 24, contentStartY: 1,
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 1, Width: 80, Height: 24},
			},
		},
		{
			name:     "two panes split height with separator",
			numPanes: 2, width: 80, height: 25, contentStartY: 0,
			// available = 25 - 1 sep = 24, base = 12, remainder = 0
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 12},
				{PaneIndex: 1, StartX: 0, StartY: 13, Width: 80, Height: 12},
			},
		},
		{
			name:     "two panes with odd height - remainder goes to last",
			numPanes: 2, width: 80, height: 24, contentStartY: 0,
			// available = 24 - 1 = 23, base = 11, remainder = 1 -> last pane gets +1
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 11},
				{PaneIndex: 1, StartX: 0, StartY: 12, Width: 80, Height: 12},
			},
		},
		{
			name:     "three panes distribute remainder to last panes",
			numPanes: 3, width: 80, height: 20, contentStartY: 0,
			// available = 20 - 2 seps = 18, base = 6, remainder = 0
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 6},
				{PaneIndex: 1, StartX: 0, StartY: 7, Width: 80, Height: 6},
				{PaneIndex: 2, StartX: 0, StartY: 14, Width: 80, Height: 6},
			},
		},
		{
			name:     "three panes with remainder 2",
			numPanes: 3, width: 80, height: 22, contentStartY: 0,
			// available = 22 - 2 = 20, base = 6, remainder = 2 -> last 2 get +1
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 6},
				{PaneIndex: 1, StartX: 0, StartY: 7, Width: 80, Height: 7},
				{PaneIndex: 2, StartX: 0, StartY: 15, Width: 80, Height: 7},
			},
		},
		{
			name:     "enforces MinPaneHeightHorizontal by reducing visible panes",
			numPanes: 10, width: 80, height: 10, contentStartY: 0,
			// With 10 panes: 10 - 9 seps = 1 -> base=0 < MinPaneHeightHorizontal(3)
			// Reduce until base >= 3. With 3 panes: 10 - 2 = 8, base = 2 < 3
			// With 2 panes: 10 - 1 = 9, base = 4 >= 3. remainder = 1
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 4},
				{PaneIndex: 1, StartX: 0, StartY: 5, Width: 80, Height: 5},
			},
		},
		{
			name:     "contentStartY offsets all panes",
			numPanes: 2, width: 80, height: 25, contentStartY: 2,
			// available = 25 - 1 = 24, base = 12, remainder = 0
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 2, Width: 80, Height: 12},
				{PaneIndex: 1, StartX: 0, StartY: 15, Width: 80, Height: 12},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := calculatePaneLayoutHorizontal(tt.numPanes, tt.width, tt.height, tt.contentStartY)
			if tt.want == nil {
				if got != nil {
					t.Fatalf("expected nil, got %v", got)
				}
				return
			}
			if len(got) != len(tt.want) {
				t.Fatalf("expected %d layouts, got %d: %+v", len(tt.want), len(got), got)
			}
			for i := range tt.want {
				if got[i] != tt.want[i] {
					t.Errorf("layout[%d]: expected %+v, got %+v", i, tt.want[i], got[i])
				}
			}
		})
	}
}

// ---------------------------------------------------------------------------
// calculatePaneLayoutVertical
// ---------------------------------------------------------------------------
func TestCalculatePaneLayoutVertical(t *testing.T) {
	tests := []struct {
		name          string
		numPanes      int
		width         int
		height        int
		contentStartY int
		want          []PaneLayout
	}{
		{
			name:     "zero panes returns nil",
			numPanes: 0, width: 80, height: 24, contentStartY: 0,
			want: nil,
		},
		{
			name:     "single pane gets full width",
			numPanes: 1, width: 80, height: 24, contentStartY: 0,
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 80, Height: 24},
			},
		},
		{
			name:     "two panes split width with separator",
			numPanes: 2, width: 81, height: 24, contentStartY: 0,
			// available = 81 - 1 = 80, base = 40, remainder = 0
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 40, Height: 24},
				{PaneIndex: 1, StartX: 41, StartY: 0, Width: 40, Height: 24},
			},
		},
		{
			name:     "two panes with even width - remainder goes to last",
			numPanes: 2, width: 80, height: 24, contentStartY: 0,
			// available = 80 - 1 = 79, base = 39, remainder = 1 -> last gets +1
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 39, Height: 24},
				{PaneIndex: 1, StartX: 40, StartY: 0, Width: 40, Height: 24},
			},
		},
		{
			name:     "three panes with remainder",
			numPanes: 3, width: 80, height: 24, contentStartY: 0,
			// available = 80 - 2 = 78, base = 26, remainder = 0
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 26, Height: 24},
				{PaneIndex: 1, StartX: 27, StartY: 0, Width: 26, Height: 24},
				{PaneIndex: 2, StartX: 54, StartY: 0, Width: 26, Height: 24},
			},
		},
		{
			name:     "contentStartY sets StartY for all panes",
			numPanes: 2, width: 81, height: 24, contentStartY: 3,
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 3, Width: 40, Height: 24},
				{PaneIndex: 1, StartX: 41, StartY: 3, Width: 40, Height: 24},
			},
		},
		{
			name:     "enforces MinPaneWidthVertical by reducing visible panes",
			numPanes: 10, width: 50, height: 24, contentStartY: 0,
			// With 10 panes: 50 - 9 = 41, base = 4 < MinPaneWidthVertical(20)
			// With 2 panes: 50 - 1 = 49, base = 24 >= 20, remainder = 1
			want: []PaneLayout{
				{PaneIndex: 0, StartX: 0, StartY: 0, Width: 24, Height: 24},
				{PaneIndex: 1, StartX: 25, StartY: 0, Width: 25, Height: 24},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := calculatePaneLayoutVertical(tt.numPanes, tt.width, tt.height, tt.contentStartY)
			if tt.want == nil {
				if got != nil {
					t.Fatalf("expected nil, got %v", got)
				}
				return
			}
			if len(got) != len(tt.want) {
				t.Fatalf("expected %d layouts, got %d: %+v", len(tt.want), len(got), got)
			}
			for i := range tt.want {
				if got[i] != tt.want[i] {
					t.Errorf("layout[%d]: expected %+v, got %+v", i, tt.want[i], got[i])
				}
			}
		})
	}
}

// ---------------------------------------------------------------------------
// lineNumberWidth
// ---------------------------------------------------------------------------
func TestLineNumberWidthLayout(t *testing.T) {
	tests := []struct {
		name       string
		totalLines int
		want       int
	}{
		{name: "single digit (1 line)", totalLines: 1, want: 4},   // digits=1, min=3 -> 3+1=4
		{name: "single digit (9 lines)", totalLines: 9, want: 4},  // digits=1, min=3 -> 3+1=4
		{name: "two digits (10 lines)", totalLines: 10, want: 4},  // digits=2, min=3 -> 3+1=4
		{name: "two digits (99 lines)", totalLines: 99, want: 4},  // digits=2, min=3 -> 3+1=4
		{name: "three digits (100 lines)", totalLines: 100, want: 4}, // digits=3 -> 3+1=4
		{name: "three digits (999 lines)", totalLines: 999, want: 4}, // digits=3 -> 3+1=4
		{name: "four digits (1000 lines)", totalLines: 1000, want: 5}, // digits=4 -> 4+1=5
		{name: "five digits (10000 lines)", totalLines: 10000, want: 6}, // digits=5 -> 5+1=6
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := lineNumberWidth(tt.totalLines)
			if got != tt.want {
				t.Errorf("lineNumberWidth(%d) = %d, want %d", tt.totalLines, got, tt.want)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// isInSearchMatch
// ---------------------------------------------------------------------------
func TestIsInSearchMatch(t *testing.T) {
	search := &SearchState{
		Active: true,
		Matches: []SearchMatch{
			{Row: 0, Col: 5, Length: 3},  // match 0: row 0, cols 5-7
			{Row: 2, Col: 10, Length: 4}, // match 1: row 2, cols 10-13
			{Row: 2, Col: 20, Length: 2}, // match 2: row 2, cols 20-21
		},
	}

	tests := []struct {
		name      string
		row       int
		col       int
		wantIdx   int
		wantFound bool
	}{
		{name: "hit first match start", row: 0, col: 5, wantIdx: 0, wantFound: true},
		{name: "hit first match middle", row: 0, col: 6, wantIdx: 0, wantFound: true},
		{name: "hit first match end", row: 0, col: 7, wantIdx: 0, wantFound: true},
		{name: "miss just before first match", row: 0, col: 4, wantIdx: -1, wantFound: false},
		{name: "miss just after first match", row: 0, col: 8, wantIdx: -1, wantFound: false},
		{name: "miss wrong row", row: 1, col: 5, wantIdx: -1, wantFound: false},
		{name: "hit second match", row: 2, col: 12, wantIdx: 1, wantFound: true},
		{name: "hit third match", row: 2, col: 20, wantIdx: 2, wantFound: true},
		{name: "miss between two matches on same row", row: 2, col: 15, wantIdx: -1, wantFound: false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			gotIdx, gotFound := isInSearchMatch(search, tt.row, tt.col)
			if gotIdx != tt.wantIdx || gotFound != tt.wantFound {
				t.Errorf("isInSearchMatch(row=%d, col=%d) = (%d, %v), want (%d, %v)",
					tt.row, tt.col, gotIdx, gotFound, tt.wantIdx, tt.wantFound)
			}
		})
	}

	t.Run("empty matches returns not found", func(t *testing.T) {
		empty := &SearchState{Matches: nil}
		idx, found := isInSearchMatch(empty, 0, 0)
		if found || idx != -1 {
			t.Errorf("expected (-1, false), got (%d, %v)", idx, found)
		}
	})
}

// ---------------------------------------------------------------------------
// getLineNumberWidth (via Buffer)
// ---------------------------------------------------------------------------
func TestGetLineNumberWidth(t *testing.T) {
	tests := []struct {
		name  string
		lines []string
		want  int
	}{
		{name: "single line buffer", lines: []string{"hello"}, want: 4},
		{name: "1000 line buffer", lines: nil, want: 5}, // built separately
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.lines != nil {
				buf := newTestBuffer(tt.lines...)
				got := getLineNumberWidth(buf)
				if got != tt.want {
					t.Errorf("getLineNumberWidth() = %d, want %d", got, tt.want)
				}
			}
		})
	}

	t.Run("1000 line buffer", func(t *testing.T) {
		lines := make([][]rune, 1000)
		for i := range lines {
			lines[i] = []rune("x")
		}
		buf := &Buffer{Lines: lines, Filename: "big.txt"}
		got := getLineNumberWidth(buf)
		if got != 5 {
			t.Errorf("getLineNumberWidth(1000 lines) = %d, want 5", got)
		}
	})
}

// ---------------------------------------------------------------------------
// getLineNumberWidthFromPane
// ---------------------------------------------------------------------------
func TestGetLineNumberWidthFromPane(t *testing.T) {
	tests := []struct {
		name  string
		lines []string
		want  int
	}{
		{name: "small pane", lines: []string{"a", "b", "c"}, want: 4},
		{name: "single line pane", lines: []string{"only"}, want: 4},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			pane := newTestPane(tt.lines...)
			got := getLineNumberWidthFromPane(pane)
			if got != tt.want {
				t.Errorf("getLineNumberWidthFromPane() = %d, want %d", got, tt.want)
			}
		})
	}
}
