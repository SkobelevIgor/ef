# Data Model: Editor Interface Contracts

**Feature**: 012-editor-test-coverage
**Date**: 2026-03-01

## Interface Definitions

### ScreenRenderer

Abstracts all terminal screen operations that the Editor depends on.

**Methods**:
| Method | Signature | Purpose |
|--------|-----------|---------|
| Render | `Render(panes []*Pane, activePaneIdx int, mode Mode, inputState *InputState, splitMode SplitMode)` | Draw all panes and UI to screen |
| PollEvent | `PollEvent() tcell.Event` | Block until next terminal event |
| PostEvent | `PostEvent(ev tcell.Event) error` | Inject synthetic event into queue |
| Size | `Size() (width, height int)` | Return current terminal dimensions |
| Close | `Close()` | Shut down terminal |
| Suspend | `Suspend() error` | Suspend terminal (for shell-out) |
| Resume | `Resume() error` | Resume terminal after suspend |

**Implemented by**: `Screen` struct (wraps `tcell.Screen`)

---

### EventPoster

Minimal interface for components that need to post events to the screen (e.g., FileWatcher notifying of external changes).

**Methods**:
| Method | Signature | Purpose |
|--------|-----------|---------|
| PostEvent | `PostEvent(ev tcell.Event) error` | Post event to terminal queue |

**Implemented by**: `Screen` struct
**Note**: This is a subset of `ScreenRenderer`. `FileWatcher` depends on this instead of the full `ScreenRenderer`.

---

### FileWatcherService

Abstracts filesystem monitoring so Editor doesn't depend on concrete `FileWatcher`.

**Methods**:
| Method | Signature | Purpose |
|--------|-----------|---------|
| Watch | `Watch(filename string) error` | Start watching a file for changes |
| Close | `Close() error` | Stop all watches and clean up |
| UpdateModTime | `UpdateModTime(filename string)` | Update cached mod time after save |

**Implemented by**: `FileWatcher` struct (wraps `fsnotify.Watcher`)

---

### ConfigProvider

Abstracts configuration access so Editor doesn't depend on concrete `Config` or filesystem config files.

**Methods**:
| Method | Signature | Purpose |
|--------|-----------|---------|
| GetFileTypeConfig | `GetFileTypeConfig(ftName string) FileTypeConfig` | Get settings for a file type |
| GetKeyMappings | `GetKeyMappings() map[string]string` | Get custom key mappings |

**Implemented by**: `Config` struct (loads from `.efconfig`)
**Note**: Method `GetKeyMappings` needs to be added to `Config` — currently key mappings are accessed via `config.KeyMappings` field directly.

---

### FileTypeDetector

Abstracts file type detection and registry lookup.

**Methods**:
| Method | Signature | Purpose |
|--------|-----------|---------|
| DetectFileType | `DetectFileType(filename string) string` | Detect file type from filename/extension |
| GetHighlighter | `GetHighlighter(ft string) SyntaxHighlighter` | Get syntax highlighter for file type |
| GetFormatter | `GetFormatter(ft string) Formatter` | Get code formatter for file type |
| GetConfig | `GetConfig(ft string) FileTypeConfig` | Get file type specific config |

**Implemented by**: `FileTypeRegistry` struct

---

## Dependencies Struct

### EditorDeps

Aggregates all interface dependencies for the Editor constructor.

**Fields**:
| Field | Type | Purpose |
|-------|------|---------|
| Screen | `ScreenRenderer` | Terminal rendering |
| FileWatcher | `FileWatcherService` | File change monitoring |
| Config | `ConfigProvider` | Configuration access |
| FileTypes | `FileTypeDetector` | File type detection |

---

## Existing Types (Unchanged)

These types are data-centric and do NOT need interface abstraction:

- **Buffer**: Text content model (`Lines [][]rune`, `Filename`, `Modified`, etc.)
- **Pane**: Viewport state (`Buffer *Buffer`, `CursorRow`, `CursorCol`, `ScrollOffset`, etc.)
- **History**: Undo/redo stacks (`undoStack`, `redoStack`, `*Change`)
- **InputState**: Session state (`Search *SearchState`, `Autocomplete *AutocompleteState`)
- **SearchState**: Search session data
- **AutocompleteState**: Autocomplete session data
- **Mode**: Enum (ModeNormal, ModeInsert, ModeVisual)
