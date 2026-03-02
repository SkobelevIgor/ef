package editor

import (
	"os"
	"testing"
	"time"

	"go.uber.org/mock/gomock"
)

// ---------------------------------------------------------------------------
// getOrCreateBuffer
// ---------------------------------------------------------------------------

func TestGetOrCreateBuffer_NewFile(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor

	// Mock file type detection for the new file
	env.FileTypes.EXPECT().DetectFileType(gomock.Any()).Return("").AnyTimes()
	env.FileTypes.EXPECT().GetConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()
	env.FileTypes.EXPECT().GetHighlighter(gomock.Any()).Return(nil).AnyTimes()

	tmp := t.TempDir()
	path := tmp + "/newfile.txt"
	os.WriteFile(path, []byte("hello\nworld"), 0644)

	buf, err := e.getOrCreateBuffer(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if buf == nil {
		t.Fatal("buffer should not be nil")
	}
	if len(buf.Lines) != 2 {
		t.Errorf("Lines count = %d, want 2", len(buf.Lines))
	}
}

func TestGetOrCreateBuffer_ExistingBuffer(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor

	// Mock file type detection
	env.FileTypes.EXPECT().DetectFileType(gomock.Any()).Return("").AnyTimes()
	env.FileTypes.EXPECT().GetConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()
	env.FileTypes.EXPECT().GetHighlighter(gomock.Any()).Return(nil).AnyTimes()

	tmp := t.TempDir()
	path := tmp + "/existing.txt"
	os.WriteFile(path, []byte("content"), 0644)

	buf1, err := e.getOrCreateBuffer(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	buf2, err := e.getOrCreateBuffer(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	if buf1 != buf2 {
		t.Error("should return same buffer for same file")
	}
}

// ---------------------------------------------------------------------------
// getPanesForBuffer
// ---------------------------------------------------------------------------

func TestGetPanesForBuffer(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	buf := e.activeBuffer()

	panes := e.getPanesForBuffer(buf)
	if len(panes) != 1 {
		t.Errorf("got %d panes, want 1", len(panes))
	}

	// Unknown buffer
	otherBuf := newTestBuffer("other")
	panes = e.getPanesForBuffer(otherBuf)
	if len(panes) != 0 {
		t.Errorf("got %d panes for unknown buffer, want 0", len(panes))
	}
}

// ---------------------------------------------------------------------------
// adjustOtherPaneCursors
// ---------------------------------------------------------------------------

func TestAdjustOtherPaneCursors_ZeroDelta(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor
	buf := e.activeBuffer()

	// Zero delta should be a no-op
	e.adjustOtherPaneCursors(buf, 0, 0)
	// No assertion needed, just shouldn't panic
}

func TestAdjustOtherPaneCursors_WithMultiplePanes(t *testing.T) {
	ctrl := gomock.NewController(t)
	mockScreen := NewMockScreenRenderer(ctrl)
	mockFW := NewMockFileWatcherService(ctrl)
	mockCfg := NewMockConfigProvider(ctrl)
	mockFT := NewMockFileTypeDetector(ctrl)

	mockScreen.EXPECT().Size().Return(80, 24).AnyTimes()
	mockCfg.EXPECT().GetKeyMappings().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetMaps().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetFileTypeConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()

	buf := newTestBuffer("line1", "line2", "line3", "line4", "line5")
	pane1 := NewPane(buf)
	pane2 := NewPane(buf) // Same buffer, different pane
	pane2.CursorRow = 3

	deps := EditorDeps{
		Screen:      mockScreen,
		FileWatcher: mockFW,
		Config:      mockCfg,
		FileTypes:   mockFT,
	}
	buffers := map[string]*Buffer{"test.txt": buf}
	ed := NewEditorWithDeps(deps, buffers, []*Pane{pane1, pane2}, SplitHorizontal)

	// Active pane is pane1 (idx 0). Inserting a line at row 1 should adjust pane2's cursor.
	ed.adjustOtherPaneCursors(buf, 1, 1)

	if pane2.CursorRow != 4 {
		t.Errorf("pane2 CursorRow = %d, want 4", pane2.CursorRow)
	}
}

// ---------------------------------------------------------------------------
// stopAutoSave
// ---------------------------------------------------------------------------

func TestStopAutoSave(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor

	// No timer set - should not panic
	e.stopAutoSave()

	// Set a timer and stop it
	e.autoSaveTimer = time.AfterFunc(time.Hour, func() {})
	e.stopAutoSave()
	// Timer should be stopped (not panic)
}

// ---------------------------------------------------------------------------
// handleExternalFileChange
// ---------------------------------------------------------------------------

func TestHandleExternalFileChange_UnknownFile(t *testing.T) {
	env := newTestEditor(t)
	e := env.Editor

	// Unknown file should be a no-op
	e.handleExternalFileChange("/nonexistent/file.txt")
	// Should not panic
}

func TestHandleExternalFileChange_KnownFile(t *testing.T) {
	tmp := t.TempDir()
	path := tmp + "/test.txt"
	os.WriteFile(path, []byte("original"), 0644)

	ctrl := gomock.NewController(t)
	mockScreen := NewMockScreenRenderer(ctrl)
	mockFW := NewMockFileWatcherService(ctrl)
	mockCfg := NewMockConfigProvider(ctrl)
	mockFT := NewMockFileTypeDetector(ctrl)

	mockScreen.EXPECT().Size().Return(80, 24).AnyTimes()
	mockCfg.EXPECT().GetKeyMappings().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetMaps().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetFileTypeConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()
	mockFW.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()

	buf, _ := NewBuffer(path)
	pane := NewPane(buf)

	absPath, _ := os.Getwd()
	_ = absPath

	deps := EditorDeps{
		Screen:      mockScreen,
		FileWatcher: mockFW,
		Config:      mockCfg,
		FileTypes:   mockFT,
	}

	// Use absolute path as buffer registry key
	import_path := path
	buffers := map[string]*Buffer{import_path: buf}
	ed := NewEditorWithDeps(deps, buffers, []*Pane{pane}, SplitHorizontal)

	// Modify the file externally
	os.WriteFile(path, []byte("modified content"), 0644)

	// Trigger external file change
	ed.handleExternalFileChange(import_path)

	// Buffer should have reloaded
	if string(buf.Lines[0]) != "modified content" {
		t.Errorf("buffer not reloaded: %q", string(buf.Lines[0]))
	}
}

// ---------------------------------------------------------------------------
// EventMappingTimeout.When / FileChangedEvent.When
// ---------------------------------------------------------------------------

func TestEventMappingTimeout_When(t *testing.T) {
	now := time.Now()
	ev := &EventMappingTimeout{when: now, snapshot: now}
	if !ev.When().Equal(now) {
		t.Error("When should return the creation time")
	}
}

func TestFileChangedEvent_When(t *testing.T) {
	now := time.Now()
	ev := &FileChangedEvent{when: now, Filename: "test.txt"}
	if !ev.When().Equal(now) {
		t.Error("When should return the creation time")
	}
}

// ---------------------------------------------------------------------------
// replaceCurrentMatch
// ---------------------------------------------------------------------------

func TestReplaceCurrentMatch(t *testing.T) {
	env := newTestEditor(t, "hello world hello")
	e := env.Editor
	env.FileWatcher.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()

	pane := e.activePane()
	buf := pane.Buffer

	// Set up search state with matches
	search := NewSearchState(0, 0)
	search.Active = true
	search.Confirmed = true
	search.Query = "hello"
	search.IsReplaceMode = true
	search.ReplaceText = "hi"
	search.Matches = buf.FindAllMatches("hello")
	search.CurrentIndex = 0
	e.inputState.Search = search

	e.replaceCurrentMatch()

	line := string(buf.Lines[0])
	if line != "hi world hello" {
		t.Errorf("after replace: %q, want 'hi world hello'", line)
	}
}

func TestReplaceCurrentMatch_NilSearch(t *testing.T) {
	env := newTestEditor(t, "hello")
	e := env.Editor

	// Should not panic with nil search
	e.replaceCurrentMatch()
}

func TestReplaceCurrentMatch_NotReplaceMode(t *testing.T) {
	env := newTestEditor(t, "hello")
	e := env.Editor

	search := NewSearchState(0, 0)
	search.IsReplaceMode = false
	e.inputState.Search = search

	// Should not do anything
	e.replaceCurrentMatch()

	if string(e.activeBuffer().Lines[0]) != "hello" {
		t.Error("should not modify buffer when not in replace mode")
	}
}

// ---------------------------------------------------------------------------
// LoadConfig
// ---------------------------------------------------------------------------

func TestLoadConfig_NoFile(t *testing.T) {
	// LoadConfig reads from ~/.efconfig which may or may not exist
	// It should not panic and return a valid config
	cfg := LoadConfig()
	if cfg == nil {
		t.Fatal("config should not be nil")
	}
}

// ---------------------------------------------------------------------------
// calcCursorScreenPos
// ---------------------------------------------------------------------------

func TestCalcCursorScreenPos_Simple(t *testing.T) {
	lines := [][]rune{[]rune("hello"), []rune("world")}

	x, y := calcCursorScreenPos(lines, 0, 3, 0, 80, 4, 4)
	if x != 4+3 { // lineNumWidth + cursorCol
		t.Errorf("x = %d, want %d", x, 7)
	}
	if y != 0 {
		t.Errorf("y = %d, want 0", y)
	}
}

func TestCalcCursorScreenPos_SecondLine(t *testing.T) {
	lines := [][]rune{[]rune("hello"), []rune("world")}

	x, y := calcCursorScreenPos(lines, 1, 2, 0, 80, 4, 4)
	if y != 1 {
		t.Errorf("y = %d, want 1", y)
	}
	if x != 4+2 { // lineNumWidth + cursorCol
		t.Errorf("x = %d, want %d", x, 6)
	}
}

func TestCalcCursorScreenPos_WithScrollOffset(t *testing.T) {
	lines := [][]rune{[]rune("a"), []rune("b"), []rune("c"), []rune("d")}

	// Scroll offset 2, cursor on line 3
	x, y := calcCursorScreenPos(lines, 3, 0, 2, 80, 4, 4)
	if y != 1 { // line 2 takes 1 row, so cursor on line 3 is at screen row 1
		t.Errorf("y = %d, want 1", y)
	}
	if x != 4 { // lineNumWidth, col 0
		t.Errorf("x = %d, want 4", x)
	}
}

func TestCalcCursorScreenPos_EmptyLine(t *testing.T) {
	lines := [][]rune{{}, []rune("hello")}

	x, y := calcCursorScreenPos(lines, 1, 0, 0, 80, 4, 4)
	if y != 1 { // empty line takes 1 row
		t.Errorf("y = %d, want 1", y)
	}
	if x != 4 {
		t.Errorf("x = %d, want 4", x)
	}
}

func TestCalcCursorScreenPos_MinTextWidth(t *testing.T) {
	lines := [][]rune{[]rune("hello")}

	// Very narrow pane: textWidth would be 0, clamped to 1
	x, y := calcCursorScreenPos(lines, 0, 0, 0, 4, 4, 4)
	if y != 0 {
		t.Errorf("y = %d, want 0", y)
	}
	_ = x // just shouldn't panic
}

// ---------------------------------------------------------------------------
// getCursorScreenPosFromPane
// ---------------------------------------------------------------------------

func TestGetCursorScreenPosFromPane(t *testing.T) {
	p := newTestPane("hello", "world")
	p.CursorRow = 1
	p.CursorCol = 3

	x, y := getCursorScreenPosFromPane(p, 80, 4)
	if y != 1 {
		t.Errorf("y = %d, want 1", y)
	}
	if x != 7 { // 4 (lineNum) + 3 (col)
		t.Errorf("x = %d, want 7", x)
	}
}
