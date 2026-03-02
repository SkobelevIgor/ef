package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
	"go.uber.org/mock/gomock"
)

// ---------------------------------------------------------------------------
// 1. handleMarkInput
// ---------------------------------------------------------------------------

func TestHandleMarkInput_EscapeCancelsPendingMark(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyEscape, 0, 0))

	if e.inputState.PendingMark {
		t.Error("PendingMark should be false after Escape")
	}
}

func TestHandleMarkInput_EscapeCancelsPendingJump(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingJumpToMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyEscape, 0, 0))

	if e.inputState.PendingJumpToMark {
		t.Error("PendingJumpToMark should be false after Escape")
	}
}

func TestHandleMarkInput_NonRuneCancels(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyEnter, 0, 0))

	if e.inputState.PendingMark {
		t.Error("PendingMark should be false after non-rune key")
	}
}

func TestHandleMarkInput_InvalidIdentifierCancels(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingMark = true

	// Space is not a valid mark identifier
	e.handleMarkInput(tcell.NewEventKey(tcell.KeyRune, ' ', 0))

	if e.inputState.PendingMark {
		t.Error("PendingMark should be false after invalid identifier")
	}
	if _, exists := e.globalMarks[' ']; exists {
		t.Error("no mark should be set for invalid identifier")
	}
}

func TestHandleMarkInput_SetMark(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 1
	pane.CursorCol = 3
	e.inputState.PendingMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyRune, 'a', 0))

	if e.inputState.PendingMark {
		t.Error("PendingMark should be false after setting mark")
	}
	mark, exists := e.globalMarks['a']
	if !exists {
		t.Fatal("mark 'a' should exist")
	}
	if mark.Row != 1 || mark.Col != 3 {
		t.Errorf("mark position = (%d,%d), want (1,3)", mark.Row, mark.Col)
	}
	if mark.Buffer != pane.Buffer {
		t.Error("mark buffer should match active pane buffer")
	}
}

func TestHandleMarkInput_JumpToMark(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	pane := e.activePane()

	// Set a mark at row 2, col 5
	e.globalMarks['b'] = GlobalMark{
		Buffer: pane.Buffer,
		Row:    2,
		Col:    5,
	}

	pane.CursorRow = 0
	pane.CursorCol = 0
	e.inputState.PendingJumpToMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyRune, 'b', 0))

	if e.inputState.PendingJumpToMark {
		t.Error("PendingJumpToMark should be false after jump")
	}
	if pane.CursorRow != 2 || pane.CursorCol != 5 {
		t.Errorf("cursor = (%d,%d), want (2,5)", pane.CursorRow, pane.CursorCol)
	}
}

func TestHandleMarkInput_JumpToNonexistentMark(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 0
	pane.CursorCol = 0
	e.inputState.PendingJumpToMark = true

	e.handleMarkInput(tcell.NewEventKey(tcell.KeyRune, 'z', 0))

	if e.inputState.PendingJumpToMark {
		t.Error("PendingJumpToMark should be false")
	}
	// Cursor should remain unchanged
	if pane.CursorRow != 0 || pane.CursorCol != 0 {
		t.Errorf("cursor should not move for nonexistent mark, got (%d,%d)", pane.CursorRow, pane.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// 2. handleFindCharInput
// ---------------------------------------------------------------------------

func TestHandleFindCharInput_EscapeResets(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingFindForward = true

	e.handleFindCharInput(tcell.NewEventKey(tcell.KeyEscape, 0, 0))

	if e.inputState.PendingFindForward {
		t.Error("PendingFindForward should be reset after Escape")
	}
}

func TestHandleFindCharInput_Forward(t *testing.T) {
	env := newTestEditor(t, "hello world")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 0
	e.inputState.PendingFindForward = true

	e.handleFindCharInput(tcell.NewEventKey(tcell.KeyRune, 'o', 0))

	if pane.CursorCol != 4 {
		t.Errorf("CursorCol = %d, want 4 (position of first 'o')", pane.CursorCol)
	}
	if !e.inputState.HasLastFind {
		t.Error("HasLastFind should be true")
	}
	if e.inputState.LastFindChar != 'o' {
		t.Errorf("LastFindChar = %c, want 'o'", e.inputState.LastFindChar)
	}
	if !e.inputState.LastFindForward {
		t.Error("LastFindForward should be true")
	}
}

func TestHandleFindCharInput_Backward(t *testing.T) {
	env := newTestEditor(t, "hello world")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 10 // at 'd'
	e.inputState.PendingFindBackward = true

	e.handleFindCharInput(tcell.NewEventKey(tcell.KeyRune, 'o', 0))

	if pane.CursorCol != 7 {
		t.Errorf("CursorCol = %d, want 7 (position of 'o' in 'world')", pane.CursorCol)
	}
	if e.inputState.LastFindForward {
		t.Error("LastFindForward should be false for backward find")
	}
}

func TestHandleFindCharInput_NoMatchStaysPut(t *testing.T) {
	env := newTestEditor(t, "hello world")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 0
	e.inputState.PendingFindForward = true

	e.handleFindCharInput(tcell.NewEventKey(tcell.KeyRune, 'z', 0))

	if pane.CursorCol != 0 {
		t.Errorf("CursorCol = %d, want 0 (no match should not move)", pane.CursorCol)
	}
}

func TestHandleFindCharInput_WithCount(t *testing.T) {
	env := newTestEditor(t, "abcabc")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 0
	e.inputState.PendingFindForward = true
	e.inputState.Count = 2
	e.inputState.HasCount = true

	e.handleFindCharInput(tcell.NewEventKey(tcell.KeyRune, 'b', 0))

	// First 'b' at 1, second 'b' at 4
	if pane.CursorCol != 4 {
		t.Errorf("CursorCol = %d, want 4 (second 'b')", pane.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// 3. handleGotoLineInput
// ---------------------------------------------------------------------------

func TestHandleGotoLineInput_EscapeResets(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "5"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyEscape, 0, 0))

	if e.inputState.PendingGotoLine {
		t.Error("PendingGotoLine should be reset after Escape")
	}
}

func TestHandleGotoLineInput_EnterZeroGoesToFirstLine(t *testing.T) {
	env := newTestEditor(t, "line1", "line2", "line3", "line4", "line5")
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 3
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "0"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyEnter, 0, 0))

	if pane.CursorRow != 0 {
		t.Errorf("CursorRow = %d, want 0 (first line)", pane.CursorRow)
	}
}

func TestHandleGotoLineInput_EnterDollarGoesToLastLine(t *testing.T) {
	env := newTestEditor(t, "line1", "line2", "line3", "line4", "line5")
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 0
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "$"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyEnter, 0, 0))

	if pane.CursorRow != 4 {
		t.Errorf("CursorRow = %d, want 4 (last line, 0-indexed)", pane.CursorRow)
	}
}

func TestHandleGotoLineInput_EnterDigitGoesToLine(t *testing.T) {
	env := newTestEditor(t, "line1", "line2", "line3", "line4", "line5")
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 0
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "3"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyEnter, 0, 0))

	if pane.CursorRow != 2 {
		t.Errorf("CursorRow = %d, want 2 (line 3, 0-indexed)", pane.CursorRow)
	}
}

func TestHandleGotoLineInput_BackspaceDeletesChar(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "12"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyBackspace2, 0, 0))

	if e.inputState.GotoLineBuffer != "1" {
		t.Errorf("GotoLineBuffer = %q, want %q", e.inputState.GotoLineBuffer, "1")
	}
}

func TestHandleGotoLineInput_RuneDigitAppends(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "1"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyRune, '5', 0))

	if e.inputState.GotoLineBuffer != "15" {
		t.Errorf("GotoLineBuffer = %q, want %q", e.inputState.GotoLineBuffer, "15")
	}
}

func TestHandleGotoLineInput_DollarAppends(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = ""

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyRune, '$', 0))

	if e.inputState.GotoLineBuffer != "$" {
		t.Errorf("GotoLineBuffer = %q, want %q", e.inputState.GotoLineBuffer, "$")
	}
}

func TestHandleGotoLineInput_NonDigitIgnored(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = "1"

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyRune, 'x', 0))

	if e.inputState.GotoLineBuffer != "1" {
		t.Errorf("GotoLineBuffer = %q, want %q (non-digit should be ignored)", e.inputState.GotoLineBuffer, "1")
	}
}

func TestHandleGotoLineInput_EmptyBufferEnterNoOp(t *testing.T) {
	env := newTestEditor(t, "line1", "line2", "line3")
	e := env.Editor
	pane := e.activePane()
	pane.CursorRow = 1
	e.inputState.PendingGotoLine = true
	e.inputState.GotoLineBuffer = ""

	e.handleGotoLineInput(tcell.NewEventKey(tcell.KeyEnter, 0, 0))

	if pane.CursorRow != 1 {
		t.Errorf("CursorRow = %d, want 1 (empty buffer should be no-op)", pane.CursorRow)
	}
}

// ---------------------------------------------------------------------------
// 4. deleteCharUnderCursorCmd
// ---------------------------------------------------------------------------

func TestDeleteCharUnderCursorCmd_DeletesChar(t *testing.T) {
	env := newTestEditor(t, "hello")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	pane.CursorCol = 1

	e.deleteCharUnderCursorCmd()

	line := string(pane.Buffer.Lines[0])
	if line != "hllo" {
		t.Errorf("line = %q, want %q", line, "hllo")
	}
}

func TestDeleteCharUnderCursorCmd_AtEndOfLine(t *testing.T) {
	env := newTestEditor(t, "ab")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	pane.CursorCol = 1

	e.deleteCharUnderCursorCmd()

	line := string(pane.Buffer.Lines[0])
	if line != "a" {
		t.Errorf("line = %q, want %q", line, "a")
	}
}

func TestDeleteCharUnderCursorCmd_EmptyLine(t *testing.T) {
	env := newTestEditor(t, "")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	pane.CursorCol = 0

	e.deleteCharUnderCursorCmd()

	line := string(pane.Buffer.Lines[0])
	if line != "" {
		t.Errorf("line = %q, want empty", line)
	}
}

// ---------------------------------------------------------------------------
// 5. acceptAutocomplete
// ---------------------------------------------------------------------------

func TestAcceptAutocomplete_ReplacesPrefix(t *testing.T) {
	env := newTestEditor(t, "hel")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	pane.CursorCol = 3 // end of "hel"

	e.inputState.Autocomplete = &AutocompleteState{
		Active:      true,
		Prefix:      "hel",
		PrefixCol:   0,
		Suggestions: []Suggestion{{Word: "hello", Score: 100}},
		SelectedIdx: 0,
	}

	e.acceptAutocomplete()

	line := string(pane.Buffer.Lines[0])
	if line != "hello" {
		t.Errorf("line = %q, want %q", line, "hello")
	}
	if pane.CursorCol != 5 {
		t.Errorf("CursorCol = %d, want 5", pane.CursorCol)
	}
	if e.inputState.Autocomplete != nil {
		t.Error("Autocomplete should be nil after accept")
	}
}

func TestAcceptAutocomplete_NilState(t *testing.T) {
	env := newTestEditor(t, "test")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 4

	// Should not panic
	e.acceptAutocomplete()

	line := string(pane.Buffer.Lines[0])
	if line != "test" {
		t.Errorf("line = %q, want %q (should be unchanged)", line, "test")
	}
}

func TestAcceptAutocomplete_EmptySuggestions(t *testing.T) {
	env := newTestEditor(t, "test")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 4

	e.inputState.Autocomplete = &AutocompleteState{
		Active:      true,
		Prefix:      "te",
		Suggestions: []Suggestion{},
		SelectedIdx: 0,
	}

	e.acceptAutocomplete()

	line := string(pane.Buffer.Lines[0])
	if line != "test" {
		t.Errorf("line = %q, want %q (should be unchanged)", line, "test")
	}
}

func TestAcceptAutocomplete_WithTrailingText(t *testing.T) {
	env := newTestEditor(t, "hel world")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	pane.CursorCol = 3 // after "hel"

	e.inputState.Autocomplete = &AutocompleteState{
		Active:      true,
		Prefix:      "hel",
		PrefixCol:   0,
		Suggestions: []Suggestion{{Word: "hello", Score: 100}},
		SelectedIdx: 0,
	}

	e.acceptAutocomplete()

	line := string(pane.Buffer.Lines[0])
	if line != "hello world" {
		t.Errorf("line = %q, want %q", line, "hello world")
	}
	if pane.CursorCol != 5 {
		t.Errorf("CursorCol = %d, want 5", pane.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// 6. getPanesForBuffer
// ---------------------------------------------------------------------------

func TestGetPanesForBuffer_MatchingPanes(t *testing.T) {
	env := newTestEditor(t, "line1", "line2")
	e := env.Editor
	buf := e.activeBuffer()

	panes := e.getPanesForBuffer(buf)

	if len(panes) != 1 {
		t.Fatalf("got %d panes, want 1", len(panes))
	}
	if panes[0].Buffer != buf {
		t.Error("returned pane should reference the same buffer")
	}
}

func TestGetPanesForBuffer_NoMatch(t *testing.T) {
	env := newTestEditor(t, "line1")
	e := env.Editor

	otherBuf := newTestBuffer("other")
	panes := e.getPanesForBuffer(otherBuf)

	if panes != nil {
		t.Errorf("got %d panes, want nil for unmatched buffer", len(panes))
	}
}

func TestGetPanesForBuffer_MultiplePanes(t *testing.T) {
	env := newTestEditor(t, "line1", "line2")
	e := env.Editor
	buf := e.activeBuffer()

	// Add a second pane viewing the same buffer
	e.panes = append(e.panes, NewPane(buf))

	panes := e.getPanesForBuffer(buf)

	if len(panes) != 2 {
		t.Fatalf("got %d panes, want 2", len(panes))
	}
}

// ---------------------------------------------------------------------------
// 7. adjustOtherPaneCursors
// ---------------------------------------------------------------------------

func TestAdjustOtherPaneCursors_ZeroDeltaNoOp(t *testing.T) {
	env := newTestEditor(t, "a", "b", "c")
	e := env.Editor
	buf := e.activeBuffer()

	// Add second pane
	p2 := NewPane(buf)
	p2.CursorRow = 2
	e.panes = append(e.panes, p2)

	e.adjustOtherPaneCursors(buf, 0, 0)

	if p2.CursorRow != 2 {
		t.Errorf("CursorRow = %d, want 2 (should not change with zero delta)", p2.CursorRow)
	}
}

func TestAdjustOtherPaneCursors_PositiveDelta(t *testing.T) {
	env := newTestEditor(t, "a", "b", "c", "d", "e")
	e := env.Editor
	buf := e.activeBuffer()

	// Active pane at row 0
	e.activePane().CursorRow = 0

	// Add second pane at row 3
	p2 := NewPane(buf)
	p2.CursorRow = 3
	e.panes = append(e.panes, p2)

	// Simulate inserting 2 lines at row 1
	buf.Lines = append(buf.Lines, []rune("f"), []rune("g"))
	e.adjustOtherPaneCursors(buf, 1, 2)

	if p2.CursorRow != 5 {
		t.Errorf("p2.CursorRow = %d, want 5 (shifted by 2)", p2.CursorRow)
	}
	// Active pane should NOT be adjusted
	if e.activePane().CursorRow != 0 {
		t.Errorf("active pane CursorRow = %d, want 0 (should not be adjusted)", e.activePane().CursorRow)
	}
}

func TestAdjustOtherPaneCursors_DifferentBuffer(t *testing.T) {
	env := newTestEditor(t, "a", "b", "c")
	e := env.Editor

	otherBuf := newTestBuffer("x", "y", "z")
	p2 := NewPane(otherBuf)
	p2.CursorRow = 1
	e.panes = append(e.panes, p2)

	// Adjust for active buffer should not affect pane viewing different buffer
	e.adjustOtherPaneCursors(e.activeBuffer(), 0, 2)

	if p2.CursorRow != 1 {
		t.Errorf("p2.CursorRow = %d, want 1 (different buffer should not be adjusted)", p2.CursorRow)
	}
}

// ---------------------------------------------------------------------------
// 8. triggerAutocomplete
// ---------------------------------------------------------------------------

func TestTriggerAutocomplete_ShortPrefixClears(t *testing.T) {
	env := newTestEditor(t, "a")
	e := env.Editor
	pane := e.activePane()
	pane.CursorCol = 1 // after "a" (prefix is "a", length 1)

	e.inputState.Autocomplete = &AutocompleteState{Active: true}

	e.triggerAutocomplete()

	if e.inputState.Autocomplete != nil {
		t.Error("Autocomplete should be nil for short prefix")
	}
}

func TestTriggerAutocomplete_MatchingSetsState(t *testing.T) {
	env := newTestEditor(t, "hello world hello_there")
	e := env.Editor
	pane := e.activePane()
	// Position cursor after "hel" typed on a second line
	buf := pane.Buffer
	buf.Lines = append(buf.Lines, []rune("hel"))
	pane.CursorRow = 1
	pane.CursorCol = 3 // after "hel"

	e.triggerAutocomplete()

	ac := e.inputState.Autocomplete
	if ac == nil {
		t.Fatal("Autocomplete should not be nil for matching prefix")
	}
	if !ac.Active {
		t.Error("Autocomplete should be active")
	}
	if ac.Prefix != "hel" {
		t.Errorf("Prefix = %q, want %q", ac.Prefix, "hel")
	}
	if len(ac.Suggestions) == 0 {
		t.Error("should have at least one suggestion")
	}
}

func TestTriggerAutocomplete_NoMatchClears(t *testing.T) {
	env := newTestEditor(t, "hello world")
	e := env.Editor
	pane := e.activePane()
	buf := pane.Buffer
	buf.Lines = append(buf.Lines, []rune("zz"))
	pane.CursorRow = 1
	pane.CursorCol = 2

	e.triggerAutocomplete()

	if e.inputState.Autocomplete != nil {
		t.Error("Autocomplete should be nil when no suggestions match")
	}
}

// ---------------------------------------------------------------------------
// 9. flushPendingMapKeys
// ---------------------------------------------------------------------------

func TestFlushPendingMapKeys_InsertsChars(t *testing.T) {
	env := newTestEditor(t, "")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	e.mode = ModeInsert
	e.pendingMapKeys = "abc"

	e.flushPendingMapKeys()

	line := string(e.activePane().Buffer.Lines[0])
	if line != "abc" {
		t.Errorf("line = %q, want %q", line, "abc")
	}
	if e.pendingMapKeys != "" {
		t.Errorf("pendingMapKeys = %q, want empty", e.pendingMapKeys)
	}
}

func TestFlushPendingMapKeys_EmptyNoOp(t *testing.T) {
	env := newTestEditor(t, "existing")
	e := env.Editor
	e.pendingMapKeys = ""

	e.flushPendingMapKeys()

	line := string(e.activePane().Buffer.Lines[0])
	if line != "existing" {
		t.Errorf("line = %q, want %q", line, "existing")
	}
}

func TestFlushPendingMapKeys_ClearsMapKeyTime(t *testing.T) {
	env := newTestEditor(t, "")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	e.pendingMapKeys = "x"

	e.flushPendingMapKeys()

	if !e.mapKeyTime.IsZero() {
		t.Error("mapKeyTime should be zero after flush")
	}
}

// ---------------------------------------------------------------------------
// 10. executeMapping
// ---------------------------------------------------------------------------

func TestExecuteMapping_InsertModeInsertsText(t *testing.T) {
	env := newTestEditor(t, "")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	buf := pane.Buffer

	// Enter insert mode with history session
	e.mode = ModeInsert
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)

	e.executeMapping("abc")

	line := string(buf.Lines[0])
	if line != "abc" {
		t.Errorf("line = %q, want %q", line, "abc")
	}
	if pane.CursorCol != 3 {
		t.Errorf("CursorCol = %d, want 3", pane.CursorCol)
	}
}

func TestExecuteMapping_InsertModeEscape(t *testing.T) {
	env := newTestEditor(t, "hello")
	e := env.Editor
	pane := e.activePane()
	buf := pane.Buffer

	e.mode = ModeInsert
	pane.CursorCol = 3
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)

	e.executeMapping("<Esc>")

	if e.mode != ModeNormal {
		t.Errorf("mode = %d, want ModeNormal", e.mode)
	}
	// Cursor moves back one position like vim
	if pane.CursorCol != 2 {
		t.Errorf("CursorCol = %d, want 2 (moved back on Escape)", pane.CursorCol)
	}
}

func TestExecuteMapping_InsertModeEnter(t *testing.T) {
	env := newTestEditor(t, "hello")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	buf := pane.Buffer

	e.mode = ModeInsert
	pane.CursorCol = 5
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)

	e.executeMapping("<CR>")

	if len(buf.Lines) != 2 {
		t.Errorf("line count = %d, want 2 after Enter", len(buf.Lines))
	}
	if pane.CursorRow != 1 {
		t.Errorf("CursorRow = %d, want 1", pane.CursorRow)
	}
}

func TestExecuteMapping_NormalModeNavigation(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	e := env.Editor
	pane := e.activePane()

	e.mode = ModeNormal
	pane.CursorRow = 0
	pane.CursorCol = 0

	e.executeMapping("j$")

	if pane.CursorRow != 1 {
		t.Errorf("CursorRow = %d, want 1 (after 'j')", pane.CursorRow)
	}
	expectedCol := len([]rune("second line")) // MoveToLineEnd sets col = len(line)
	if pane.CursorCol != expectedCol {
		t.Errorf("CursorCol = %d, want %d (after '$')", pane.CursorCol, expectedCol)
	}
}

func TestExecuteMapping_InsertModeBackspace(t *testing.T) {
	env := newTestEditor(t, "abc")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	buf := pane.Buffer

	e.mode = ModeInsert
	pane.CursorCol = 3
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)

	e.executeMapping("<BS>")

	line := string(buf.Lines[0])
	if line != "ab" {
		t.Errorf("line = %q, want %q", line, "ab")
	}
}

func TestExecuteMapping_InsertModeTab(t *testing.T) {
	env := newTestEditor(t, "")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()
	pane := e.activePane()
	buf := pane.Buffer

	e.mode = ModeInsert
	e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)

	e.executeMapping("<Tab>")

	line := string(buf.Lines[0])
	if len(line) == 0 {
		t.Error("line should have tab/spaces inserted")
	}
}
