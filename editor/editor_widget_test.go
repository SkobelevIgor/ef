package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

// ---------------------------------------------------------------------------
// openSearchWidget (F4)
// ---------------------------------------------------------------------------

func TestOpenSearchWidget_Fresh(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 1
	pane.CursorCol = 3

	ed.openSearchWidget()

	w := ed.inputState.Widget
	if w == nil {
		t.Fatal("Widget should be non-nil after openSearchWidget")
	}
	if w.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	if w.AnchorRow != 1 || w.AnchorCol != 3 {
		t.Errorf("Anchor = (%d,%d), want (1,3)", w.AnchorRow, w.AnchorCol)
	}
	if w.SearchSession == nil {
		t.Fatal("SearchSession should be non-nil")
	}
	if w.SearchSession.CursorRow != 1 || w.SearchSession.CursorCol != 3 {
		t.Errorf("Session cursor = (%d,%d), want (1,3)",
			w.SearchSession.CursorRow, w.SearchSession.CursorCol)
	}
}

func TestOpenSearchWidget_FromInsertMode(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	ed.mode = ModeInsert

	ed.openSearchWidget()

	if ed.inputState.Widget == nil {
		t.Fatal("Widget should be non-nil from Insert mode")
	}
	if ed.inputState.Widget.Kind != WidgetSearch {
		t.Error("Kind should be WidgetSearch")
	}
}

func TestOpenSearchWidget_FromVisualMode(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	ed.mode = ModeVisual

	ed.openSearchWidget()

	if ed.inputState.Widget == nil {
		t.Fatal("Widget should be non-nil from Visual mode")
	}
}

// F4 when Search active → cycle focus (FindBar ↔ Editor)
func TestOpenSearchWidget_CycleFocus(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Fatal("initial focus should be FocusFindBar")
	}

	// F4 again → Editor
	ed.openSearchWidget()
	if ed.inputState.Widget.Focus != FocusEditor {
		t.Errorf("second F4: Focus = %d, want FocusEditor", ed.inputState.Widget.Focus)
	}

	// F4 again → back to FindBar
	ed.openSearchWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Errorf("third F4: Focus = %d, want FocusFindBar", ed.inputState.Widget.Focus)
	}
}

// F4 when FindReplace active → switch to Search
func TestOpenSearchWidget_SwitchFromFindReplace(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// Open FindReplace first
	ed.openFindReplaceWidget()
	w := ed.inputState.Widget
	if w.Kind != WidgetFindReplace {
		t.Fatal("should be FindReplace after F3")
	}
	w.FindReplaceSession.Query = "test"

	// F4 → switch to Search
	ed.openSearchWidget()
	w = ed.inputState.Widget
	if w.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch after F4", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	// SearchSession should be lazy-created
	if w.SearchSession == nil {
		t.Fatal("SearchSession should be lazy-created")
	}
	// FindReplaceSession should be preserved
	if w.FindReplaceSession == nil {
		t.Fatal("FindReplaceSession should be preserved")
	}
	if w.FindReplaceSession.Query != "test" {
		t.Errorf("FindReplaceSession.Query = %q, want 'test'", w.FindReplaceSession.Query)
	}
}

// ---------------------------------------------------------------------------
// openFindReplaceWidget (F3)
// ---------------------------------------------------------------------------

func TestOpenFindReplaceWidget_Fresh(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 0
	pane.CursorCol = 5

	ed.openFindReplaceWidget()

	w := ed.inputState.Widget
	if w == nil {
		t.Fatal("Widget should be non-nil")
	}
	if w.Kind != WidgetFindReplace {
		t.Errorf("Kind = %d, want WidgetFindReplace", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	if w.AnchorRow != 0 || w.AnchorCol != 5 {
		t.Errorf("Anchor = (%d,%d), want (0,5)", w.AnchorRow, w.AnchorCol)
	}
}

// F3 when FindReplace already active → no-op (Tab now cycles focus)
func TestWidgetFindReplace_F3WhenActiveIsNoOp(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Fatal("initial focus should be FocusFindBar")
	}

	// F3 again → should stay on FocusFindBar (no-op)
	ed.openFindReplaceWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar (no-op)", ed.inputState.Widget.Focus)
	}

	// Manually set to ReplaceBar and press F3 — should stay
	ed.inputState.Widget.Focus = FocusReplaceBar
	ed.openFindReplaceWidget()
	if ed.inputState.Widget.Focus != FocusReplaceBar {
		t.Errorf("Focus = %d, want FocusReplaceBar (no-op)", ed.inputState.Widget.Focus)
	}
}

// F3 when Search active → switch to FindReplace
func TestOpenFindReplaceWidget_SwitchFromSearch(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	w := ed.inputState.Widget
	w.SearchSession.Query = "hello"

	ed.openFindReplaceWidget()
	w = ed.inputState.Widget
	if w.Kind != WidgetFindReplace {
		t.Errorf("Kind = %d, want WidgetFindReplace", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	// FindReplaceSession should be lazy-created
	if w.FindReplaceSession == nil {
		t.Fatal("FindReplaceSession should be lazy-created")
	}
	// SearchSession should be preserved
	if w.SearchSession == nil {
		t.Fatal("SearchSession should be preserved")
	}
	if w.SearchSession.Query != "hello" {
		t.Errorf("SearchSession.Query = %q, want 'hello'", w.SearchSession.Query)
	}
}

// ---------------------------------------------------------------------------
// closeWidget (Escape)
// ---------------------------------------------------------------------------

func TestCloseWidget_RestoresAnchor(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 1
	pane.CursorCol = 5

	ed.openSearchWidget()

	// Move cursor away (simulating search navigation)
	pane.CursorRow = 0
	pane.CursorCol = 0

	ed.closeWidget()

	if ed.inputState.Widget != nil {
		t.Error("Widget should be nil after closeWidget")
	}
	if pane.CursorRow != 1 || pane.CursorCol != 5 {
		t.Errorf("Cursor = (%d,%d), want (1,5)", pane.CursorRow, pane.CursorCol)
	}
}

func TestCloseWidget_FromFindReplace(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 0
	pane.CursorCol = 3

	ed.openFindReplaceWidget()
	pane.CursorRow = 0
	pane.CursorCol = 0

	ed.closeWidget()

	if ed.inputState.Widget != nil {
		t.Error("Widget should be nil after closeWidget")
	}
	if pane.CursorRow != 0 || pane.CursorCol != 3 {
		t.Errorf("Cursor = (%d,%d), want (0,3)", pane.CursorRow, pane.CursorCol)
	}
}

func TestCloseWidget_ClearsBothSessions(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	ed.inputState.Widget.SearchSession.Query = "test"
	ed.closeWidget()

	if ed.inputState.Widget != nil {
		t.Error("Widget should be nil after close")
	}
}

func TestCloseWidget_NilWidgetNoOp(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	// Should not panic
	ed.closeWidget()
}

// ---------------------------------------------------------------------------
// Dual session independence
// ---------------------------------------------------------------------------

func TestDualSessions_IndependentState(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	// Open F4 search and type query
	ed.openSearchWidget()
	ed.inputState.Widget.SearchSession.Query = "hello"

	// Switch to F3
	ed.openFindReplaceWidget()
	ed.inputState.Widget.FindReplaceSession.Query = "world"
	ed.inputState.Widget.FindReplaceSession.ReplaceText = "earth"

	// Switch back to F4
	ed.openSearchWidget()
	w := ed.inputState.Widget
	if w.SearchSession.Query != "hello" {
		t.Errorf("SearchSession.Query = %q, want 'hello'", w.SearchSession.Query)
	}

	// Switch back to F3
	ed.openFindReplaceWidget()
	w = ed.inputState.Widget
	if w.FindReplaceSession.Query != "world" {
		t.Errorf("FindReplaceSession.Query = %q, want 'world'", w.FindReplaceSession.Query)
	}
	if w.FindReplaceSession.ReplaceText != "earth" {
		t.Errorf("FindReplaceSession.ReplaceText = %q, want 'earth'", w.FindReplaceSession.ReplaceText)
	}
}

// ---------------------------------------------------------------------------
// F3/F4 via handleKey
// ---------------------------------------------------------------------------

func TestHandleKey_F4OpensSearchWidget(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.handleKey(tcell.NewEventKey(tcell.KeyF4, 0, tcell.ModNone))

	if ed.inputState.Widget == nil {
		t.Fatal("F4 should open widget")
	}
	if ed.inputState.Widget.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch", ed.inputState.Widget.Kind)
	}
}

func TestHandleKey_F3OpensFindReplaceWidget(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.handleKey(tcell.NewEventKey(tcell.KeyF3, 0, tcell.ModNone))

	if ed.inputState.Widget == nil {
		t.Fatal("F3 should open widget")
	}
	if ed.inputState.Widget.Kind != WidgetFindReplace {
		t.Errorf("Kind = %d, want WidgetFindReplace", ed.inputState.Widget.Kind)
	}
}

// ---------------------------------------------------------------------------
// Cross-session match refresh on switch
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Widget Input: FindBar typing
// ---------------------------------------------------------------------------

func TestWidgetFindBar_TypeBuildsQuery(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone))
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'e', tcell.ModNone))

	s := ed.inputState.Widget.SearchSession
	if s.Query != "he" {
		t.Errorf("Query = %q, want 'he'", s.Query)
	}
}

func TestWidgetFindBar_AutoSearchAt2Chars(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar")
	ed := env.Editor

	ed.openSearchWidget()

	// 1 char: matches found but no navigation
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone))
	s := ed.inputState.Widget.SearchSession
	if len(s.Matches) == 0 {
		t.Error("1 char: should have matches for highlighting")
	}

	// 2 chars: auto-navigate to first match
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'e', tcell.ModNone))
	pane := ed.activePane()
	if s.CurrentIndex < 0 {
		t.Error("2 chars: should auto-navigate to first match")
	}
	match := s.Matches[s.CurrentIndex]
	if pane.CursorRow != match.Row || pane.CursorCol != match.Col {
		t.Errorf("cursor should be at match (%d,%d), got (%d,%d)",
			match.Row, match.Col, pane.CursorRow, pane.CursorCol)
	}
}

func TestWidgetFindBar_NoAutoSearchAt1Char(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 0
	pane.CursorCol = 5

	ed.openSearchWidget()

	// Type 1 char — cursor should NOT move to match
	origRow, origCol := pane.CursorRow, pane.CursorCol
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone))
	if pane.CursorRow != origRow || pane.CursorCol != origCol {
		t.Error("1 char should not navigate cursor")
	}
}

func TestWidgetFindBar_Backspace(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone))
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'e', tcell.ModNone))

	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	s := ed.inputState.Widget.SearchSession
	if s.Query != "h" {
		t.Errorf("after backspace, Query = %q, want 'h'", s.Query)
	}

	// Backspace to empty
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	if s.Query != "" {
		t.Errorf("after second backspace, Query = %q, want empty", s.Query)
	}

	// Backspace on empty — no panic
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	if s.Query != "" {
		t.Error("backspace on empty should stay empty")
	}
}

func TestWidgetFindBar_NoMatches(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	for _, r := range "zzz" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	s := ed.inputState.Widget.SearchSession
	if !s.NoMatches {
		t.Error("should indicate no matches")
	}
	if len(s.Matches) != 0 {
		t.Errorf("expected 0 matches, got %d", len(s.Matches))
	}
}

func TestWidgetFindBar_Unicode(t *testing.T) {
	env := newTestEditor(t, "日本語テスト hello")
	ed := env.Editor

	ed.openSearchWidget()
	for _, r := range "日本" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	s := ed.inputState.Widget.SearchSession
	if s.Query != "日本" {
		t.Errorf("Query = %q, want '日本'", s.Query)
	}
	if len(s.Matches) != 1 {
		t.Errorf("expected 1 match for '日本', got %d", len(s.Matches))
	}
}

// ---------------------------------------------------------------------------
// Widget Input: Escape from any focus
// ---------------------------------------------------------------------------

func TestWidgetMode_EscapeFromFindBar(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))

	if ed.inputState.Widget != nil {
		t.Error("Escape should close widget")
	}
}

func TestWidgetMode_EscapeFromReplaceBar(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	ed.inputState.Widget.Focus = FocusReplaceBar

	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))

	if ed.inputState.Widget != nil {
		t.Error("Escape from ReplaceBar should close widget")
	}
}

func TestWidgetMode_EscapeFromEditor(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	ed.inputState.Widget.Focus = FocusEditor

	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEscape, 0, tcell.ModNone))

	if ed.inputState.Widget != nil {
		t.Error("Escape from Editor should close widget")
	}
}

// ---------------------------------------------------------------------------
// Widget Input: ReplaceBar typing
// ---------------------------------------------------------------------------

func TestWidgetReplaceBar_TypeBuildsText(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	ed.inputState.Widget.Focus = FocusReplaceBar

	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'h', tcell.ModNone))
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'i', tcell.ModNone))

	s := ed.inputState.Widget.FindReplaceSession
	if s.ReplaceText != "hi" {
		t.Errorf("ReplaceText = %q, want 'hi'", s.ReplaceText)
	}
}

func TestWidgetReplaceBar_Backspace(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	ed.inputState.Widget.Focus = FocusReplaceBar

	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'a', tcell.ModNone))
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'b', tcell.ModNone))
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))

	s := ed.inputState.Widget.FindReplaceSession
	if s.ReplaceText != "a" {
		t.Errorf("ReplaceText = %q, want 'a'", s.ReplaceText)
	}

	// Backspace to empty
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
	if s.ReplaceText != "" {
		t.Errorf("ReplaceText = %q, want empty", s.ReplaceText)
	}

	// Backspace on empty — no panic
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyBackspace2, 0, tcell.ModNone))
}

// ---------------------------------------------------------------------------
// Widget Input: Enter in Search mode (confirm + enable n/N)
// ---------------------------------------------------------------------------

func TestWidgetSearchMode_EnterConfirms(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar")
	ed := env.Editor

	ed.openSearchWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	// Enter confirms and switches focus to editor
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	w := ed.inputState.Widget
	if w.Focus != FocusEditor {
		t.Errorf("Focus = %d, want FocusEditor after Enter", w.Focus)
	}
}

// ---------------------------------------------------------------------------
// Widget Input: n/N navigation in FocusEditor
// ---------------------------------------------------------------------------

func TestWidgetEditorFocus_NextPrev(t *testing.T) {
	env := newTestEditor(t, "hello world hello again hello")
	ed := env.Editor

	ed.openSearchWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	// Confirm
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	s := ed.inputState.Widget.SearchSession
	firstIdx := s.CurrentIndex

	// n → next match
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'n', tcell.ModNone))
	if s.CurrentIndex == firstIdx {
		t.Error("'n' should move to next match")
	}

	// N → previous match
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, 'N', tcell.ModNone))
	if s.CurrentIndex != firstIdx {
		t.Errorf("'N' should return to first match, got %d", s.CurrentIndex)
	}
}

func TestWidgetEditorFocus_WrapAround(t *testing.T) {
	env := newTestEditor(t, "hello world", "foo hello bar", "hello again")
	ed := env.Editor

	ed.openSearchWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	s := ed.inputState.Widget.SearchSession
	total := len(s.Matches)
	if total < 3 {
		t.Fatalf("expected 3+ matches, got %d", total)
	}

	// Navigate to last match
	s.CurrentIndex = total - 1
	ed.widgetNextMatch()
	if s.CurrentIndex != 0 {
		t.Errorf("should wrap to 0, got %d", s.CurrentIndex)
	}

	ed.widgetPrevMatch()
	if s.CurrentIndex != total-1 {
		t.Errorf("should wrap to %d, got %d", total-1, s.CurrentIndex)
	}
}

// ---------------------------------------------------------------------------
// Widget Input: Enter in FindReplace mode (replace + advance)
// ---------------------------------------------------------------------------

func TestWidgetFindReplace_EnterOnEditorReplaces(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	ed.openFindReplaceWidget()

	// Type "hello" in Find bar
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	// Set replace text and move focus to Editor
	ed.inputState.Widget.FindReplaceSession.ReplaceText = "hi"
	ed.inputState.Widget.Focus = FocusEditor

	// Enter on editor replaces current match
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	buf := ed.activeBuffer()
	line := string(buf.Lines[0])
	if line != "hi world hello" {
		t.Errorf("after first replace: %q, want 'hi world hello'", line)
	}

	// Focus should stay on editor
	if ed.inputState.Widget.Focus != FocusEditor {
		t.Errorf("Focus = %d, want FocusEditor after replace", ed.inputState.Widget.Focus)
	}
}

func TestWidgetFindReplace_EnterInFindBarNoOp(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	ed.openFindReplaceWidget()

	// Type in Find bar
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	// Set replace text via session directly
	ed.inputState.Widget.FindReplaceSession.ReplaceText = "hi"

	// Enter from FindBar in FR mode should be no-op
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	line := string(ed.activeBuffer().Lines[0])
	if line != "hello world hello" {
		t.Errorf("Enter in FindBar should not replace: %q, want 'hello world hello'", line)
	}
	// Focus should stay on FindBar
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar (no change)", ed.inputState.Widget.Focus)
	}
}

func TestWidgetFindReplace_EnterWithEmptyReplace(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	ed.openFindReplaceWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}

	// Move to editor focus, empty replace text → deletes match
	ed.inputState.Widget.Focus = FocusEditor
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	line := string(ed.activeBuffer().Lines[0])
	if line != " world hello" {
		t.Errorf("after delete-replace: %q, want ' world hello'", line)
	}
}

// ---------------------------------------------------------------------------
// Widget Input: F3/F4 within widget mode switch correctly
// ---------------------------------------------------------------------------

func TestWidgetMode_F3SwitchesDuringSearch(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	// F3 while in search mode
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyF3, 0, tcell.ModNone))

	if ed.inputState.Widget.Kind != WidgetFindReplace {
		t.Errorf("Kind = %d, want WidgetFindReplace", ed.inputState.Widget.Kind)
	}
}

func TestWidgetMode_F4SwitchesDuringFindReplace(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	// F4 while in find-replace mode
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyF4, 0, tcell.ModNone))

	if ed.inputState.Widget.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch", ed.inputState.Widget.Kind)
	}
}

// ---------------------------------------------------------------------------
// Cross-session match refresh on switch
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Tab cycling in FindReplace mode
// ---------------------------------------------------------------------------

func TestWidgetFindReplace_TabCyclesFocus(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openFindReplaceWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Fatal("initial focus should be FocusFindBar")
	}

	// Tab → ReplaceBar
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyTab, 0, tcell.ModNone))
	if ed.inputState.Widget.Focus != FocusReplaceBar {
		t.Errorf("Focus = %d, want FocusReplaceBar", ed.inputState.Widget.Focus)
	}

	// Tab → Editor
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyTab, 0, tcell.ModNone))
	if ed.inputState.Widget.Focus != FocusEditor {
		t.Errorf("Focus = %d, want FocusEditor", ed.inputState.Widget.Focus)
	}

	// Tab → back to FindBar
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyTab, 0, tcell.ModNone))
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", ed.inputState.Widget.Focus)
	}
}

func TestWidgetSearch_TabDoesNotCycleFocus(t *testing.T) {
	env := newTestEditor(t, "hello world")
	ed := env.Editor

	ed.openSearchWidget()
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Fatal("initial focus should be FocusFindBar")
	}

	// Tab in Search mode should not change focus
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyTab, 0, tcell.ModNone))
	if ed.inputState.Widget.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar (Tab is no-op in Search)", ed.inputState.Widget.Focus)
	}
}

// ---------------------------------------------------------------------------
// Enter in ReplaceBar is no-op
// ---------------------------------------------------------------------------

func TestWidgetFindReplace_EnterInReplaceBarNoOp(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	ed.openFindReplaceWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	ed.inputState.Widget.FindReplaceSession.ReplaceText = "hi"
	ed.inputState.Widget.Focus = FocusReplaceBar

	// Enter in ReplaceBar should be no-op
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))

	line := string(ed.activeBuffer().Lines[0])
	if line != "hello world hello" {
		t.Errorf("Enter in ReplaceBar should not replace: %q, want 'hello world hello'", line)
	}
	if ed.inputState.Widget.Focus != FocusReplaceBar {
		t.Errorf("Focus = %d, want FocusReplaceBar", ed.inputState.Widget.Focus)
	}
}

// ---------------------------------------------------------------------------
// Repeated Enter on editor replaces one-by-one
// ---------------------------------------------------------------------------

func TestWidgetFindReplace_RepeatedEnterOnEditor(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor

	ed.openFindReplaceWidget()
	for _, r := range "hello" {
		ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyRune, r, tcell.ModNone))
	}
	ed.inputState.Widget.FindReplaceSession.ReplaceText = "hi"
	ed.inputState.Widget.Focus = FocusEditor

	// First Enter
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))
	line := string(ed.activeBuffer().Lines[0])
	if line != "hi world hello" {
		t.Errorf("after first replace: %q, want 'hi world hello'", line)
	}
	if ed.inputState.Widget.Focus != FocusEditor {
		t.Error("focus should stay on FocusEditor after replace")
	}

	// Second Enter
	ed.handleWidgetMode(tcell.NewEventKey(tcell.KeyEnter, 0, tcell.ModNone))
	line = string(ed.activeBuffer().Lines[0])
	if line != "hi world hi" {
		t.Errorf("after second replace: %q, want 'hi world hi'", line)
	}
	if ed.inputState.Widget.Focus != FocusEditor {
		t.Error("focus should stay on FocusEditor after second replace")
	}
}

// ---------------------------------------------------------------------------
// Cross-session match refresh on switch
// ---------------------------------------------------------------------------

func TestSwitchToWidget_RefreshesMatches(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	ed := env.Editor
	buf := ed.activeBuffer()

	// Open search, set query with matches
	ed.openSearchWidget()
	session := ed.inputState.Widget.SearchSession
	session.Query = "hello"
	session.Matches = buf.FindAllMatches("hello")
	session.CurrentIndex = 0

	// Switch to FindReplace (simulating user modifying buffer)
	ed.openFindReplaceWidget()

	// Modify buffer: remove first "hello"
	buf.Lines[0] = []rune("hi world hello")

	// Switch back to Search — matches should refresh
	ed.openSearchWidget()
	session = ed.inputState.Widget.SearchSession
	if len(session.Matches) != 1 {
		t.Errorf("after refresh, expected 1 match, got %d", len(session.Matches))
	}
}
