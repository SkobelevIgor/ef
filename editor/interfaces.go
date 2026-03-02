package editor

//go:generate mockgen -source=interfaces.go -destination=mock_interfaces_test.go -package=editor -self_package=ef/editor

// Interfaces for dependency inversion in the editor package.
// Existing interfaces (SyntaxHighlighter in syntax.go, Formatter in formatter.go)
// are preserved. These new interfaces follow the same Go idiom of small, focused contracts.

import "github.com/gdamore/tcell/v2"

// ScreenRenderer abstracts terminal screen operations.
type ScreenRenderer interface {
	Render(panes []*Pane, activePaneIdx int, mode Mode, inputState *InputState, splitMode SplitMode)
	PollEvent() tcell.Event
	PostEvent(ev tcell.Event) error
	Size() (width, height int)
	Sync()
	Close()
	Suspend() error
	Resume() error
}

// CellWriter abstracts low-level terminal cell operations for rendering.
// This enables testing of rendering methods without a real terminal.
type CellWriter interface {
	SetContent(x, y int, mainc rune, combc []rune, style tcell.Style)
	ShowCursor(x, y int)
	Show()
	Clear()
	Size() (width, height int)
}

// EventPoster is a minimal interface for posting events to the screen.
// Used by FileWatcher to decouple from the full ScreenRenderer.
type EventPoster interface {
	PostEvent(ev tcell.Event) error
}

// FileWatcherService abstracts filesystem monitoring.
type FileWatcherService interface {
	Watch(filename string) error
	Close() error
	UpdateModTime(filename string)
}

// ConfigProvider abstracts configuration access.
type ConfigProvider interface {
	GetFileTypeConfig(ftName string) FileTypeConfig
	GetKeyMappings() map[string]string
	GetMaps() map[string]string
}

// FileTypeDetector abstracts file type detection and registry lookup.
type FileTypeDetector interface {
	DetectFileType(filename string) string
	GetHighlighter(ft string) SyntaxHighlighter
	GetFormatter(ft string) Formatter
	GetConfig(ft string) FileTypeConfig
}
