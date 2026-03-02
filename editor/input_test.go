package editor

import "testing"

func TestNewInputState(t *testing.T) {
	s := NewInputState()
	if s.HasCount || s.Count != 0 {
		t.Error("new state should have no count")
	}
	if s.HasPending() {
		t.Error("new state should have no pending")
	}
}

func TestInputStateReset(t *testing.T) {
	s := NewInputState()
	s.Count = 5
	s.HasCount = true
	s.PendingFindForward = true
	s.PendingOperator = 'd'
	s.PendingMark = true
	s.PendingGotoLine = true
	s.GotoLineBuffer = "42"

	// Set last find (should NOT be reset)
	s.SaveLastFind('x', true)

	s.Reset()

	if s.Count != 0 || s.HasCount {
		t.Error("count should be reset")
	}
	if s.PendingFindForward || s.PendingOperator != 0 || s.PendingMark || s.PendingGotoLine {
		t.Error("pending states should be reset")
	}
	if s.GotoLineBuffer != "" {
		t.Error("goto line buffer should be reset")
	}
	// Last find should be preserved
	if !s.HasLastFind || s.LastFindChar != 'x' || !s.LastFindForward {
		t.Error("last find should be preserved across reset")
	}
}

func TestInputStateGetCount(t *testing.T) {
	s := NewInputState()
	if s.GetCount() != 1 {
		t.Error("default count should be 1")
	}
	s.AddDigit(3)
	if s.GetCount() != 3 {
		t.Errorf("count = %d, want 3", s.GetCount())
	}
	s.AddDigit(5)
	if s.GetCount() != 35 {
		t.Errorf("count = %d, want 35", s.GetCount())
	}
}

func TestInputStateHasPending(t *testing.T) {
	s := NewInputState()
	if s.HasPending() {
		t.Error("should not have pending initially")
	}

	s.PendingFindForward = true
	if !s.HasPending() {
		t.Error("should have pending with PendingFindForward")
	}
	s.Reset()

	s.PendingOperator = 'd'
	if !s.HasPending() {
		t.Error("should have pending with PendingOperator")
	}
	s.Reset()

	s.PendingMark = true
	if !s.HasPending() {
		t.Error("should have pending with PendingMark")
	}
	s.Reset()

	s.PendingJumpToMark = true
	if !s.HasPending() {
		t.Error("should have pending with PendingJumpToMark")
	}
}

func TestInputStatePendingString(t *testing.T) {
	s := NewInputState()
	if s.PendingString() != "" {
		t.Error("should be empty initially")
	}

	s.PendingFindForward = true
	if s.PendingString() != "f_" {
		t.Errorf("got %q", s.PendingString())
	}
	s.Reset()

	s.PendingFindBackward = true
	if s.PendingString() != "F_" {
		t.Errorf("got %q", s.PendingString())
	}
	s.Reset()

	s.PendingGotoLine = true
	s.GotoLineBuffer = "42"
	if s.PendingString() != ":42" {
		t.Errorf("got %q", s.PendingString())
	}
	s.Reset()

	s.PendingOperator = 'd'
	if s.PendingString() != "d" {
		t.Errorf("got %q", s.PendingString())
	}
	s.Reset()

	s.PendingMark = true
	if s.PendingString() != "m_" {
		t.Errorf("got %q", s.PendingString())
	}
	s.Reset()

	s.PendingJumpToMark = true
	if s.PendingString() != "`_" {
		t.Errorf("got %q", s.PendingString())
	}
}

// ---------------------------------------------------------------------------
// HasActiveWidget
// ---------------------------------------------------------------------------

func TestHasActiveWidget_Nil(t *testing.T) {
	s := NewInputState()
	if s.HasActiveWidget() {
		t.Error("should be false when Widget is nil")
	}
}

func TestHasActiveWidget_Active(t *testing.T) {
	s := NewInputState()
	s.Widget = NewWidgetState(WidgetSearch, 0, 0)
	if !s.HasActiveWidget() {
		t.Error("should be true when Widget is active")
	}
}

func TestHasActiveWidget_Inactive(t *testing.T) {
	s := NewInputState()
	s.Widget = &WidgetState{Active: false}
	if s.HasActiveWidget() {
		t.Error("should be false when Widget is inactive")
	}
}

// ---------------------------------------------------------------------------
// Reset preserves Widget
// ---------------------------------------------------------------------------

func TestInputStateReset_PreservesWidget(t *testing.T) {
	s := NewInputState()
	w := NewWidgetState(WidgetSearch, 0, 0)
	s.Widget = w
	s.Count = 5
	s.HasCount = true

	s.Reset()

	if s.Widget != w {
		t.Error("Reset should preserve Widget")
	}
}

func TestInputStateSaveLastFind(t *testing.T) {
	s := NewInputState()
	s.SaveLastFind('x', true)
	if !s.HasLastFind || s.LastFindChar != 'x' || !s.LastFindForward {
		t.Error("SaveLastFind not working")
	}
	s.SaveLastFind('y', false)
	if s.LastFindChar != 'y' || s.LastFindForward {
		t.Error("second SaveLastFind not working")
	}
}
