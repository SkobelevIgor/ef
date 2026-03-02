package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

// TestSearchMode_EnterSearchMode verifies that F4 activates the search widget
// with a non-nil, Active Widget state.
func TestSearchMode_EnterSearchMode(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	if ed.activePane().HasActiveWidget() {
		t.Fatal("Widget should not be active before F4")
	}

	ed.handleKey(tcell.NewEventKey(tcell.KeyF4, 0, tcell.ModNone))

	if !ed.activePane().HasActiveWidget() {
		t.Fatal("Widget should be active after F4")
	}
	w := ed.activePane().Widget
	if w.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch", w.Kind)
	}
	if w.SearchSession.Query != "" {
		t.Errorf("Query should be empty on fresh enter, got %q", w.SearchSession.Query)
	}
}

// TestSearchMode_TypeQuery verifies that typing characters in search mode
// builds up the query and updates matches incrementally.
func TestSearchMode_TypeQuery(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	ed.enterSearchMode()

	search := ed.inputState.Search
	if search == nil {
		t.Fatal("Search state should not be nil after enterSearchMode")
	}

	// Type 'h'
	ev := tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone)
	ed.handleSearchMode(ev)

	if search.Query != "h" {
		t.Errorf("after typing 'h', query should be 'h', got %q", search.Query)
	}
	if len(search.Matches) == 0 {
		t.Error("expected matches for 'h' in buffer with 'hello'")
	}

	// Type 'e'
	ev = tcell.NewEventKey(tcell.KeyRune, 'e', tcell.ModNone)
	ed.handleSearchMode(ev)

	if search.Query != "he" {
		t.Errorf("after typing 'e', query should be 'he', got %q", search.Query)
	}
	if len(search.Matches) == 0 {
		t.Error("expected matches for 'he' in buffer with 'hello'")
	}

	// Type 'l', 'l', 'o' to form "hello"
	for _, r := range "llo" {
		ev = tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone)
		ed.handleSearchMode(ev)
	}

	if search.Query != "hello" {
		t.Errorf("expected query 'hello', got %q", search.Query)
	}
	// "hello" appears in line 0 ("hello world") and line 1 ("foo hello bar")
	if len(search.Matches) != 2 {
		t.Errorf("expected 2 matches for 'hello', got %d", len(search.Matches))
	}
}

// TestSearchMode_ConfirmAndNavigate verifies that pressing Enter confirms the
// search and 'n'/'N' navigate between matches.
func TestSearchMode_ConfirmAndNavigate(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	ed.enterSearchMode()

	// Type "hello"
	for _, r := range "hello" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	search := ed.inputState.Search
	if len(search.Matches) != 2 {
		t.Fatalf("expected 2 matches before confirm, got %d", len(search.Matches))
	}

	// Press Enter to confirm
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	if !search.Confirmed {
		t.Error("Search.Confirmed should be true after Enter")
	}

	firstIndex := search.CurrentIndex

	// Press 'n' to go to next match
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, 'n', tcell.ModNone))

	if search.CurrentIndex == firstIndex {
		t.Error("'n' should have moved to the next match")
	}
	secondIndex := search.CurrentIndex

	// Press 'N' to go back to previous match
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, 'N', tcell.ModNone))

	if search.CurrentIndex != firstIndex {
		t.Errorf("'N' should have moved back to match %d, got %d", firstIndex, search.CurrentIndex)
	}
	_ = secondIndex
}

// TestSearchMode_EscapeCancels verifies that Escape exits search mode and
// restores the cursor to CursorZero position.
func TestSearchMode_EscapeCancels(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	pane := ed.activePane()
	// Move cursor to a known position
	pane.CursorRow = 1
	pane.CursorCol = 3

	ed.enterSearchMode()

	search := ed.inputState.Search
	if search == nil {
		t.Fatal("Search state should not be nil")
	}

	// CursorZero should be saved from when we entered search mode
	if search.CursorZeroRow != 1 || search.CursorZeroCol != 3 {
		t.Errorf("CursorZero should be (1,3), got (%d,%d)", search.CursorZeroRow, search.CursorZeroCol)
	}

	// Type a query that moves cursor to a match
	for _, r := range "hello" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	// Escape should cancel and restore cursor
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))

	if ed.inputState.Search != nil {
		t.Error("Search state should be nil after Escape")
	}
	if pane.CursorRow != 1 || pane.CursorCol != 3 {
		t.Errorf("cursor should be restored to (1,3), got (%d,%d)", pane.CursorRow, pane.CursorCol)
	}
}

// TestSearchMode_BackspaceDeletesChar verifies that pressing Backspace removes
// the last character from the query.
func TestSearchMode_BackspaceDeletesChar(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	ed.enterSearchMode()

	// Type "hello"
	for _, r := range "hello" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	search := ed.inputState.Search
	if search.Query != "hello" {
		t.Fatalf("expected query 'hello', got %q", search.Query)
	}

	// Backspace once -> "hell"
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace, 0, tcell.ModNone))
	if search.Query != "hell" {
		t.Errorf("after backspace, expected 'hell', got %q", search.Query)
	}

	// Backspace again -> "hel"
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	if search.Query != "hel" {
		t.Errorf("after second backspace, expected 'hel', got %q", search.Query)
	}

	// Backspace three more times to empty the query
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace, 0, tcell.ModNone))
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace, 0, tcell.ModNone))
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace, 0, tcell.ModNone))

	if search.Query != "" {
		t.Errorf("after backspacing all chars, expected '', got %q", search.Query)
	}

	// Backspace on empty query should be a no-op (not panic)
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyBackspace, 0, tcell.ModNone))
	if search.Query != "" {
		t.Errorf("backspace on empty query should leave it empty, got %q", search.Query)
	}
}

// TestSearchMode_NoMatches verifies that searching for text that does not
// exist in the buffer sets NoMatches to true.
func TestSearchMode_NoMatches(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	ed.enterSearchMode()

	// Type a query with no matches
	for _, r := range "zzzzz" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	search := ed.inputState.Search
	if !search.NoMatches {
		t.Error("NoMatches should be true when query has no matches")
	}
	if len(search.Matches) != 0 {
		t.Errorf("expected 0 matches, got %d", len(search.Matches))
	}
	if search.CurrentIndex != -1 {
		t.Errorf("CurrentIndex should be -1 when no matches, got %d", search.CurrentIndex)
	}
}

// TestSearchMode_RestorePreviousQuery verifies that after confirming a search
// and exiting, re-entering search mode restores the previous query.
func TestSearchMode_RestorePreviousQuery(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "third line")
	ed := env.Editor

	// First search session: type "hello" and confirm
	ed.enterSearchMode()
	for _, r := range "hello" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	// Exit search mode via Escape (after confirming, escape should save lastSearchQuery)
	// We need to call exitSearchMode directly since Escape on confirmed search still exits
	ed.exitSearchMode()

	if ed.lastSearchQuery != "hello" {
		t.Errorf("lastSearchQuery should be 'hello' after confirmed search exit, got %q", ed.lastSearchQuery)
	}

	// Second search session: query should be restored
	ed.enterSearchMode()

	search := ed.inputState.Search
	if search == nil {
		t.Fatal("Search state should not be nil after second enterSearchMode")
	}
	if search.Query != "hello" {
		t.Errorf("expected restored query 'hello', got %q", search.Query)
	}
	// Confirmed should be true so n/N work immediately
	if !search.Confirmed {
		t.Error("Confirmed should be true when query is restored from lastSearchQuery")
	}
}

// TestSearchMode_WrapAround verifies that navigating past the last match wraps
// to the first match, and navigating before the first match wraps to the last.
func TestSearchMode_WrapAround(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "hello again")
	ed := env.Editor

	ed.enterSearchMode()

	// Type "hello" - appears in lines 0, 1, 2
	for _, r := range "hello" {
		ed.handleSearchMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	search := ed.inputState.Search
	if len(search.Matches) != 3 {
		t.Fatalf("expected 3 matches for 'hello', got %d", len(search.Matches))
	}

	// Confirm search
	ed.handleSearchMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	// Navigate to a known index by pressing 'n' until we are at the last match
	// Force currentIndex to last match
	search.CurrentIndex = len(search.Matches) - 1

	// Navigate to next - should wrap to 0
	ed.nextMatch()
	if search.CurrentIndex != 0 {
		t.Errorf("after wrapping past last match, expected index 0, got %d", search.CurrentIndex)
	}

	// Navigate to previous from index 0 - should wrap to last
	ed.prevMatch()
	if search.CurrentIndex != len(search.Matches)-1 {
		t.Errorf("after wrapping before first match, expected index %d, got %d",
			len(search.Matches)-1, search.CurrentIndex)
	}
}
