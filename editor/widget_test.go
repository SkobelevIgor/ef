package editor

import "testing"

// ---------------------------------------------------------------------------
// WidgetKind / FocusTarget constants
// ---------------------------------------------------------------------------

func TestWidgetKindConstants(t *testing.T) {
	if WidgetNone != 0 {
		t.Errorf("WidgetNone = %d, want 0", WidgetNone)
	}
	if WidgetSearch != 1 {
		t.Errorf("WidgetSearch = %d, want 1", WidgetSearch)
	}
	if WidgetFindReplace != 2 {
		t.Errorf("WidgetFindReplace = %d, want 2", WidgetFindReplace)
	}
}

func TestFocusTargetConstants(t *testing.T) {
	if FocusFindBar != 0 {
		t.Errorf("FocusFindBar = %d, want 0", FocusFindBar)
	}
	if FocusReplaceBar != 1 {
		t.Errorf("FocusReplaceBar = %d, want 1", FocusReplaceBar)
	}
	if FocusEditor != 2 {
		t.Errorf("FocusEditor = %d, want 2", FocusEditor)
	}
}

// ---------------------------------------------------------------------------
// NewWidgetSession
// ---------------------------------------------------------------------------

func TestNewWidgetSession(t *testing.T) {
	s := NewWidgetSession(5, 10)
	if s.Query != "" {
		t.Errorf("Query = %q, want empty", s.Query)
	}
	if s.ReplaceText != "" {
		t.Errorf("ReplaceText = %q, want empty", s.ReplaceText)
	}
	if s.CurrentIndex != -1 {
		t.Errorf("CurrentIndex = %d, want -1", s.CurrentIndex)
	}
	if s.NoMatches {
		t.Error("NoMatches should be false")
	}
	if s.CursorRow != 5 || s.CursorCol != 10 {
		t.Errorf("Cursor = (%d,%d), want (5,10)", s.CursorRow, s.CursorCol)
	}
	if len(s.Matches) != 0 {
		t.Errorf("Matches should be empty, got %d", len(s.Matches))
	}
}

func TestNewWidgetSession_ZeroPosition(t *testing.T) {
	s := NewWidgetSession(0, 0)
	if s.CursorRow != 0 || s.CursorCol != 0 {
		t.Errorf("Cursor = (%d,%d), want (0,0)", s.CursorRow, s.CursorCol)
	}
}

// ---------------------------------------------------------------------------
// NewWidgetState
// ---------------------------------------------------------------------------

func TestNewWidgetState_Search(t *testing.T) {
	w := NewWidgetState(WidgetSearch, 3, 7)
	if !w.Active {
		t.Error("Active should be true")
	}
	if w.Kind != WidgetSearch {
		t.Errorf("Kind = %d, want WidgetSearch", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	if w.AnchorRow != 3 || w.AnchorCol != 7 {
		t.Errorf("Anchor = (%d,%d), want (3,7)", w.AnchorRow, w.AnchorCol)
	}
	if w.SearchSession == nil {
		t.Error("SearchSession should be non-nil for WidgetSearch")
	}
	if w.FindReplaceSession != nil {
		t.Error("FindReplaceSession should be nil for WidgetSearch")
	}
}

func TestNewWidgetState_FindReplace(t *testing.T) {
	w := NewWidgetState(WidgetFindReplace, 1, 2)
	if !w.Active {
		t.Error("Active should be true")
	}
	if w.Kind != WidgetFindReplace {
		t.Errorf("Kind = %d, want WidgetFindReplace", w.Kind)
	}
	if w.Focus != FocusFindBar {
		t.Errorf("Focus = %d, want FocusFindBar", w.Focus)
	}
	if w.FindReplaceSession == nil {
		t.Error("FindReplaceSession should be non-nil for WidgetFindReplace")
	}
	if w.SearchSession != nil {
		t.Error("SearchSession should be nil for WidgetFindReplace")
	}
}

// ---------------------------------------------------------------------------
// CurrentSession
// ---------------------------------------------------------------------------

func TestCurrentSession_Search(t *testing.T) {
	w := NewWidgetState(WidgetSearch, 0, 0)
	s := w.CurrentSession()
	if s != w.SearchSession {
		t.Error("CurrentSession should return SearchSession for WidgetSearch")
	}
}

func TestCurrentSession_FindReplace(t *testing.T) {
	w := NewWidgetState(WidgetFindReplace, 0, 0)
	s := w.CurrentSession()
	if s != w.FindReplaceSession {
		t.Error("CurrentSession should return FindReplaceSession for WidgetFindReplace")
	}
}

func TestCurrentSession_None(t *testing.T) {
	w := &WidgetState{Kind: WidgetNone}
	s := w.CurrentSession()
	if s != nil {
		t.Error("CurrentSession should return nil for WidgetNone")
	}
}

func TestCurrentSession_NilSessions(t *testing.T) {
	w := &WidgetState{Kind: WidgetSearch, SearchSession: nil}
	s := w.CurrentSession()
	if s != nil {
		t.Error("CurrentSession should return nil when SearchSession is nil")
	}
}

// ---------------------------------------------------------------------------
// CalculateBarRows
// ---------------------------------------------------------------------------

func TestCalculateBarRows_Empty(t *testing.T) {
	rows := CalculateBarRows("", 80)
	if rows != 1 {
		t.Errorf("empty text should be 1 row, got %d", rows)
	}
}

func TestCalculateBarRows_ShortText(t *testing.T) {
	rows := CalculateBarRows("hello", 80)
	if rows != 1 {
		t.Errorf("short text should be 1 row, got %d", rows)
	}
}

func TestCalculateBarRows_ExactWidth(t *testing.T) {
	// 20 chars exactly fills width 20
	text := "12345678901234567890"
	rows := CalculateBarRows(text, 20)
	if rows != 1 {
		t.Errorf("exact width text should be 1 row, got %d", rows)
	}
}

func TestCalculateBarRows_Overflow(t *testing.T) {
	// 21 chars at width 20 → wraps to 2 rows
	text := "123456789012345678901"
	rows := CalculateBarRows(text, 20)
	if rows != 2 {
		t.Errorf("overflow text should be 2 rows, got %d", rows)
	}
}

func TestCalculateBarRows_VeryLong(t *testing.T) {
	// 60 chars at width 20 → 3 rows (20 + 20 + 20)
	text := "123456789012345678901234567890123456789012345678901234567890"
	rows := CalculateBarRows(text, 20)
	if rows != 3 {
		t.Errorf("very long text should be 3 rows, got %d", rows)
	}
}

func TestCalculateBarRows_WidthOne(t *testing.T) {
	// 3 chars at width 1 → 3 rows
	rows := CalculateBarRows("abc", 1)
	if rows != 3 {
		t.Errorf("width 1 with 3 chars should be 3 rows, got %d", rows)
	}
}

func TestCalculateBarRows_Unicode(t *testing.T) {
	// 6 unicode characters at width 20 → 1 row
	rows := CalculateBarRows("日本語テスト", 20)
	if rows != 1 {
		t.Errorf("unicode text should be 1 row, got %d", rows)
	}
}

func TestCalculateBarRows_ZeroWidth(t *testing.T) {
	rows := CalculateBarRows("hello", 0)
	if rows != 1 {
		t.Errorf("zero width should be 1 row, got %d", rows)
	}
}

func TestCalculateBarRows_NegativeWidth(t *testing.T) {
	rows := CalculateBarRows("hello", -5)
	if rows != 1 {
		t.Errorf("negative width should be 1 row, got %d", rows)
	}
}

// ---------------------------------------------------------------------------
// BarHeight
// ---------------------------------------------------------------------------

func TestBarHeight_Search_NoWrap(t *testing.T) {
	w := NewWidgetState(WidgetSearch, 0, 0)
	h := w.BarHeight(80)
	if h != 1 {
		t.Errorf("Search with empty query should be 1 row, got %d", h)
	}
}

func TestBarHeight_FindReplace_NoWrap(t *testing.T) {
	w := NewWidgetState(WidgetFindReplace, 0, 0)
	h := w.BarHeight(80)
	if h != 2 {
		t.Errorf("FindReplace with empty query should be 2 rows, got %d", h)
	}
}

func TestBarHeight_Search_WithWrap(t *testing.T) {
	w := NewWidgetState(WidgetSearch, 0, 0)
	// 21 chars at width 20 (no label) → wraps to 2 rows
	w.SearchSession.Query = "123456789012345678901"
	h := w.BarHeight(20)
	if h < 2 {
		t.Errorf("Search with long query should wrap, got %d rows", h)
	}
}

func TestBarHeight_FindReplace_WithWrap(t *testing.T) {
	w := NewWidgetState(WidgetFindReplace, 0, 0)
	// 21 chars each at width 20 (no label) → 2 rows each = 4 total
	w.FindReplaceSession.Query = "123456789012345678901"
	w.FindReplaceSession.ReplaceText = "123456789012345678901"
	h := w.BarHeight(20)
	if h < 4 {
		t.Errorf("FindReplace with long text should wrap both rows, got %d", h)
	}
}

func TestBarHeight_WidgetNone(t *testing.T) {
	w := &WidgetState{Kind: WidgetNone}
	h := w.BarHeight(80)
	if h != 0 {
		t.Errorf("WidgetNone should have 0 bar height, got %d", h)
	}
}
