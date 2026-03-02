package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

// newTestScreen creates a Screen backed by a simulation screen for testing.
func newTestScreen(width, height int) (*Screen, tcell.SimulationScreen) {
	sim := tcell.NewSimulationScreen("")
	sim.Init()
	sim.SetSize(width, height)
	return NewScreenFromTcell(sim), sim
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

func TestRender_SinglePane(t *testing.T) {
	scr, sim := newTestScreen(40, 10)
	pane := newTestPane("hello world", "second line")
	panes := []*Pane{pane}
	input := NewInputState()

	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)

	// Verify something was drawn by checking the simulation screen
	w, h := sim.Size()
	if w != 40 || h != 10 {
		t.Errorf("size = %dx%d, want 40x10", w, h)
	}

	// Check that line number area has content (line 1)
	content, _, _, _ := sim.GetContent(0, 0)
	if content == 0 {
		t.Error("expected content at (0,0)")
	}
}

func TestRender_WithSearch(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world")
	panes := []*Pane{pane}
	input := NewInputState()
	input.Search = NewSearchState(0, 0)
	input.Search.Active = true
	input.Search.Query = "hello"

	// Should not panic
	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)
}

func TestRender_VerticalSplit(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	pane1 := newTestPane("hello")
	pane2 := newTestPane("world")
	panes := []*Pane{pane1, pane2}
	input := NewInputState()

	// Should not panic
	scr.Render(panes, 0, ModeNormal, input, SplitVertical)
}

func TestRender_HorizontalSplit(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	pane1 := newTestPane("hello")
	pane2 := newTestPane("world")
	panes := []*Pane{pane1, pane2}
	input := NewInputState()

	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)
}

func TestRender_InsertMode(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello")
	panes := []*Pane{pane}
	input := NewInputState()

	scr.Render(panes, 0, ModeInsert, input, SplitHorizontal)
}

func TestRender_VisualMode(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world")
	pane.StartSelection()
	pane.CursorCol = 5
	panes := []*Pane{pane}
	input := NewInputState()

	scr.Render(panes, 0, ModeVisual, input, SplitHorizontal)
}

func TestRender_WithAutocomplete(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hel")
	pane.CursorCol = 3
	panes := []*Pane{pane}
	input := NewInputState()
	input.Autocomplete = NewAutocompleteState("hel", 0, []Suggestion{
		{Word: "hello", Score: 100},
		{Word: "help", Score: 90},
	})

	scr.Render(panes, 0, ModeInsert, input, SplitHorizontal)
}

func TestRender_EmptyBuffer(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("")
	panes := []*Pane{pane}
	input := NewInputState()

	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)
}

func TestRender_ManyLines(t *testing.T) {
	lines := make([]string, 100)
	for i := range lines {
		lines[i] = "line content here"
	}
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane(lines...)
	pane.CursorRow = 50
	pane.ScrollOffset = 45
	panes := []*Pane{pane}
	input := NewInputState()

	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)
}

func TestRender_SearchConfirmed(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world hello")
	panes := []*Pane{pane}
	input := NewInputState()
	search := NewSearchState(0, 0)
	search.Active = true
	search.Confirmed = true
	search.Query = "hello"
	search.Matches = pane.Buffer.FindAllMatches("hello")
	search.CurrentIndex = 0
	input.Search = search

	scr.Render(panes, 0, ModeNormal, input, SplitHorizontal)
}

// ---------------------------------------------------------------------------
// renderSearchBar
// ---------------------------------------------------------------------------

func TestRenderSearchBar(t *testing.T) {
	scr, sim := newTestScreen(40, 10)
	search := NewSearchState(0, 0)
	search.Active = true
	search.Query = "test"

	scr.renderSearchBar(search, 40)

	// Check that "Search: test" appears on row 0
	// The prompt "Search: " starts at x=0
	content, _, _, _ := sim.GetContent(0, 0)
	if content != 'S' {
		t.Errorf("expected 'S' at (0,0), got %q", content)
	}
}

func TestRenderSearchBar_ReplaceMode(t *testing.T) {
	scr, sim := newTestScreen(40, 10)
	search := NewSearchState(0, 0)
	search.Active = true
	search.IsReplaceMode = true
	search.Query = "old"

	scr.renderSearchBar(search, 40)

	// "Replace: " prompt
	content, _, _, _ := sim.GetContent(0, 0)
	if content != 'R' {
		t.Errorf("expected 'R' at (0,0), got %q", content)
	}
}

func TestRenderSearchBar_NoMatches(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	search := NewSearchState(0, 0)
	search.Active = true
	search.Query = "xyz"
	search.NoMatches = true

	// Should not panic
	scr.renderSearchBar(search, 40)
}

func TestRenderSearchBar_WithMatchCount(t *testing.T) {
	scr, _ := newTestScreen(60, 10)
	search := NewSearchState(0, 0)
	search.Active = true
	search.Query = "hello"
	search.Matches = []SearchMatch{{Row: 0, Col: 0, Length: 5}, {Row: 0, Col: 10, Length: 5}}
	search.CurrentIndex = 0

	scr.renderSearchBar(search, 60)
}

// ---------------------------------------------------------------------------
// renderPaneWithSearch
// ---------------------------------------------------------------------------

func TestRenderPaneWithSearch_Basic(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world", "second")

	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeNormal, nil)
}

func TestRenderPaneWithSearch_WithHighlighting(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	buf := newTestBuffer("hello world")

	// Create a highlight cache with a simple highlighter
	h := &BaseSyntaxHighlighter{}
	h.addRule(`\bhello\b`, tcell.StyleDefault.Foreground(tcell.ColorRed), 1)
	buf.HighlightCache = NewHighlightCache(h)

	pane := NewPane(buf)
	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeNormal, nil)
}

func TestRenderPaneWithSearch_WithSearchMatches(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world hello")
	search := NewSearchState(0, 0)
	search.Active = true
	search.Matches = pane.Buffer.FindAllMatches("hello")
	search.CurrentIndex = 0

	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeNormal, search)
}

func TestRenderPaneWithSearch_WithSelection(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("hello world")
	pane.StartSelection()
	pane.CursorCol = 5

	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeVisual, nil)
}

func TestRenderPaneWithSearch_EmptyLines(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("", "hello", "")

	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeNormal, nil)
}

func TestRenderPaneWithSearch_WithTabs(t *testing.T) {
	scr, _ := newTestScreen(40, 10)
	pane := newTestPane("\thello\tworld")

	scr.renderPaneWithSearch(pane, 0, 0, 40, 10, ModeNormal, nil)
}

func TestRenderPaneWithSearch_NarrowPane(t *testing.T) {
	scr, _ := newTestScreen(10, 5)
	pane := newTestPane("hello world this is a long line")

	scr.renderPaneWithSearch(pane, 0, 0, 10, 5, ModeNormal, nil)
}

// ---------------------------------------------------------------------------
// renderHorizontalSeparator / renderVerticalSeparator
// ---------------------------------------------------------------------------

func TestRenderHorizontalSeparator(t *testing.T) {
	scr, sim := newTestScreen(20, 5)

	scr.renderHorizontalSeparator(2, 20)

	// Check that row 2 has separator chars
	content, _, _, _ := sim.GetContent(0, 2)
	if content != '─' {
		t.Errorf("expected separator char, got %q", content)
	}
}

func TestRenderVerticalSeparator(t *testing.T) {
	scr, sim := newTestScreen(20, 5)

	scr.renderVerticalSeparator(10, 0, 5)

	content, _, _, _ := sim.GetContent(10, 0)
	if content != '│' {
		t.Errorf("expected separator char, got %q", content)
	}
}

// ---------------------------------------------------------------------------
// renderAutocomplete
// ---------------------------------------------------------------------------

func TestRenderAutocomplete(t *testing.T) {
	scr, _ := newTestScreen(40, 20)

	ac := NewAutocompleteState("hel", 0, []Suggestion{
		{Word: "hello", Score: 100},
		{Word: "help", Score: 90},
		{Word: "helmet", Score: 80},
	})

	scr.renderAutocomplete(ac, 5, 3, 20)
}

func TestRenderAutocomplete_Nil(t *testing.T) {
	scr, _ := newTestScreen(40, 20)

	// Should not panic with nil
	scr.renderAutocomplete(nil, 5, 3, 20)
}

func TestRenderAutocomplete_Empty(t *testing.T) {
	scr, _ := newTestScreen(40, 20)

	ac := &AutocompleteState{Suggestions: nil}
	scr.renderAutocomplete(ac, 5, 3, 20)
}

func TestRenderAutocomplete_NotEnoughSpaceBelow(t *testing.T) {
	scr, _ := newTestScreen(40, 10)

	ac := NewAutocompleteState("hel", 0, []Suggestion{
		{Word: "hello", Score: 100},
		{Word: "help", Score: 90},
	})

	// Cursor near bottom, should show above
	scr.renderAutocomplete(ac, 5, 8, 10)
}

// ---------------------------------------------------------------------------
// Screen wrapper methods (via SimulationScreen)
// ---------------------------------------------------------------------------

func TestScreen_Size(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	w, h := scr.Size()
	if w != 80 || h != 24 {
		t.Errorf("Size = %dx%d, want 80x24", w, h)
	}
}

func TestScreen_Close(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	// Should not panic
	scr.Close()
}

func TestScreen_Sync(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	// Should not panic
	scr.Sync()
}

func TestScreen_PostEvent(t *testing.T) {
	scr, _ := newTestScreen(80, 24)
	ev := &FileChangedEvent{Filename: "test.txt"}
	err := scr.PostEvent(ev)
	if err != nil {
		t.Errorf("PostEvent error: %v", err)
	}
}
