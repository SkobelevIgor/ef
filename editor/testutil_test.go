package editor

import (
	"testing"

	"go.uber.org/mock/gomock"
)

// newTestBuffer creates a Buffer with the given lines for testing.
func newTestBuffer(lines ...string) *Buffer {
	runeLines := make([][]rune, len(lines))
	for i, l := range lines {
		runeLines[i] = []rune(l)
	}
	if len(runeLines) == 0 {
		runeLines = [][]rune{{}}
	}
	return &Buffer{
		Lines:    runeLines,
		Filename: "test.txt",
		Config:   FileTypeConfig{TabStop: 4},
	}
}

// newTestPane creates a Pane wrapping a test buffer for testing.
func newTestPane(lines ...string) *Pane {
	return NewPane(newTestBuffer(lines...))
}

// testEditorEnv holds all components returned by newTestEditor.
type testEditorEnv struct {
	Editor      *Editor
	Ctrl        *gomock.Controller
	Screen      *MockScreenRenderer
	FileWatcher *MockFileWatcherService
	Config      *MockConfigProvider
	FileTypes   *MockFileTypeDetector
}

// newTestEditor creates an Editor with all mocks wired up for testing.
// Default mock expectations: Size() returns 80x24, GetKeyMappings/GetMaps return empty map.
func newTestEditor(t *testing.T, lines ...string) *testEditorEnv {
	t.Helper()
	ctrl := gomock.NewController(t)

	mockScreen := NewMockScreenRenderer(ctrl)
	mockFW := NewMockFileWatcherService(ctrl)
	mockCfg := NewMockConfigProvider(ctrl)
	mockFT := NewMockFileTypeDetector(ctrl)

	// Default expectations
	mockScreen.EXPECT().Size().Return(80, 24).AnyTimes()
	mockCfg.EXPECT().GetKeyMappings().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetMaps().Return(map[string]string{}).AnyTimes()
	mockCfg.EXPECT().GetFileTypeConfig(gomock.Any()).Return(FileTypeConfig{TabStop: 4}).AnyTimes()
	mockFW.EXPECT().UpdateModTime(gomock.Any()).AnyTimes()

	if len(lines) == 0 {
		lines = []string{"hello world", "second line", "third line"}
	}
	buf := newTestBuffer(lines...)
	buffers := map[string]*Buffer{"test.txt": buf}
	panes := []*Pane{NewPane(buf)}

	deps := EditorDeps{
		Screen:      mockScreen,
		FileWatcher: mockFW,
		Config:      mockCfg,
		FileTypes:   mockFT,
	}

	ed := NewEditorWithDeps(deps, buffers, panes, SplitHorizontal)

	// Stop auto-save timer on test cleanup to prevent goroutine leaks
	t.Cleanup(func() {
		ed.stopAutoSave()
	})

	return &testEditorEnv{
		Editor:      ed,
		Ctrl:        ctrl,
		Screen:      mockScreen,
		FileWatcher: mockFW,
		Config:      mockCfg,
		FileTypes:   mockFT,
	}
}
