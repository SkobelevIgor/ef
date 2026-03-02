# Interface Contracts: Go Source Reference

**File**: `editor/interfaces.go`

```go
package editor

import "github.com/gdamore/tcell/v2"

// ScreenRenderer abstracts terminal screen operations.
type ScreenRenderer interface {
	Render(panes []*Pane, activePaneIdx int, mode Mode, inputState *InputState, splitMode SplitMode)
	PollEvent() tcell.Event
	PostEvent(ev tcell.Event) error
	Size() (width, height int)
	Close()
	Suspend() error
	Resume() error
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
}

// FileTypeDetector abstracts file type detection and registry lookup.
type FileTypeDetector interface {
	DetectFileType(filename string) string
	GetHighlighter(ft string) SyntaxHighlighter
	GetFormatter(ft string) Formatter
	GetConfig(ft string) FileTypeConfig
}
```

## mockgen Directive

```go
//go:generate mockgen -source=interfaces.go -destination=mocks/mock_interfaces.go -package=mocks
```

This directive placed at the top of `interfaces.go` generates all mock implementations into `editor/mocks/mock_interfaces.go`.
