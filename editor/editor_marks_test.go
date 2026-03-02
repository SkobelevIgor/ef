package editor

import (
	"testing"

	"go.uber.org/mock/gomock"
)

func TestSetMark(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 1
	pane.CursorCol = 3

	ed.setMark('a')

	mark, exists := ed.globalMarks['a']
	if !exists {
		t.Fatal("mark 'a' not set")
	}
	if mark.Row != 1 || mark.Col != 3 {
		t.Errorf("mark = (%d,%d), want (1,3)", mark.Row, mark.Col)
	}
}

func TestJumpToMark(t *testing.T) {
	env := newTestEditor(t, "hello world", "second line")
	ed := env.Editor
	pane := ed.activePane()

	// Set mark at (1,3)
	pane.CursorRow = 1
	pane.CursorCol = 3
	ed.setMark('a')

	// Move cursor away
	pane.CursorRow = 0
	pane.CursorCol = 0

	// Jump back
	ed.jumpToMark('a')
	if pane.CursorRow != 1 || pane.CursorCol != 3 {
		t.Errorf("cursor = (%d,%d), want (1,3)", pane.CursorRow, pane.CursorCol)
	}
}

func TestJumpToMark_NotSet(t *testing.T) {
	env := newTestEditor(t, "hello")
	ed := env.Editor
	pane := ed.activePane()
	pane.CursorRow = 0
	pane.CursorCol = 2

	ed.jumpToMark('z') // not set

	if pane.CursorRow != 0 || pane.CursorCol != 2 {
		t.Error("cursor should not move for unset mark")
	}
}

func TestSetMark_CrossBuffer(t *testing.T) {
	ctrl := gomock.NewController(t)
	mockScreen := NewMockScreenRenderer(ctrl)
	mockFW := NewMockFileWatcherService(ctrl)
	mockCfg := NewMockConfigProvider(ctrl)
	mockFT := NewMockFileTypeDetector(ctrl)

	mockScreen.EXPECT().Size().Return(80, 24).AnyTimes()
	mockCfg.EXPECT().GetKeyMappings().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetMaps().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetFileTypeConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()

	buf1 := newTestBuffer("buffer one")
	buf2 := newTestBuffer("buffer two")
	pane1 := NewPane(buf1)
	pane2 := NewPane(buf2)

	deps := EditorDeps{Screen: mockScreen, FileWatcher: mockFW, Config: mockCfg, FileTypes: mockFT}
	ed := NewEditorWithDeps(deps, map[string]*Buffer{"a.txt": buf1, "b.txt": buf2}, []*Pane{pane1, pane2}, SplitHorizontal)

	// Set mark in pane 0
	ed.activePaneIdx = 0
	pane1.CursorRow = 0
	pane1.CursorCol = 5
	ed.setMark('A')

	// Switch to pane 1
	ed.activePaneIdx = 1
	pane2.CursorRow = 0
	pane2.CursorCol = 0

	// Jump to mark should switch back to pane 0
	ed.jumpToMark('A')
	if ed.activePaneIdx != 0 {
		t.Errorf("activePaneIdx = %d, want 0", ed.activePaneIdx)
	}
}

func TestIsValidMarkIdentifier(t *testing.T) {
	tests := []struct {
		r    rune
		want bool
	}{
		{'a', true},
		{'z', true},
		{'A', true},
		{'0', true},
		{'9', true},
		{'.', false},
		{' ', false},
		{'!', false},
	}

	for _, tt := range tests {
		got := isValidMarkIdentifier(tt.r)
		if got != tt.want {
			t.Errorf("isValidMarkIdentifier(%q) = %v, want %v", tt.r, got, tt.want)
		}
	}
}
