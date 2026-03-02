package editor

import (
	"path/filepath"
	"time"

	"github.com/gdamore/tcell/v2"
)

// SplitMode determines how multiple panes are arranged on screen
type SplitMode int

const (
	// SplitHorizontal arranges panes top-to-bottom (stacked)
	// This is the default when no -v flag is provided
	SplitHorizontal SplitMode = iota

	// SplitVertical arranges panes left-to-right (side-by-side)
	// Activated with the -v command line flag
	SplitVertical
)

// FileInfo contains filename and optional starting line number
type FileInfo struct {
	Filename string
	Line     int // 1-based line number, 0 means not specified
}

// EditorDeps holds all injected dependencies for the Editor.
// Used by NewEditorWithDeps to allow mock injection in tests.
type EditorDeps struct {
	Screen      ScreenRenderer
	FileWatcher FileWatcherService
	Config      ConfigProvider
	FileTypes   FileTypeDetector
}

// Editor is the main editor struct
type Editor struct {
	// Shared buffer model: bufferRegistry holds unique buffers, panes reference them
	bufferRegistry map[string]*Buffer // Buffers keyed by absolute file path
	panes          []*Pane            // Visual panes (each references a buffer)
	activePaneIdx  int                // Index of currently active pane

	screen        ScreenRenderer
	splitMode     SplitMode // horizontal or vertical layout
	lastShiftTime time.Time
	autoSaveTimer *time.Timer
	mode          Mode
	inputState    *InputState
	clipboard     [][]rune // Stores copied lines/text
	clipboardLine bool     // true if clipboard contains whole lines (yy/dd)
	history       *History // Undo/redo history
	fileWatcher   FileWatcherService

	// Configuration and filetype support
	config           ConfigProvider
	fileTypeRegistry FileTypeDetector

	// Key mapping state
	pendingMapKeys string
	mapKeyTime     time.Time

	// Search query persistence (for F4 re-entry)
	lastSearchQuery string

	// Global marks registry (cross-file navigation)
	globalMarks map[rune]GlobalMark

}

// activePane returns the currently active pane
func (e *Editor) activePane() *Pane {
	return e.panes[e.activePaneIdx]
}

// activeBuffer returns the buffer of the currently active pane
func (e *Editor) activeBuffer() *Buffer {
	return e.activePane().Buffer
}

// getOrCreateBuffer returns an existing buffer for the file or creates a new one
func (e *Editor) getOrCreateBuffer(filename string) (*Buffer, error) {
	absPath, err := filepath.Abs(filename)
	if err != nil {
		return nil, err
	}

	// Check if buffer already exists
	if buf, exists := e.bufferRegistry[absPath]; exists {
		return buf, nil
	}

	// Create new buffer
	buf, err := NewBufferWithRegistry(filename, e.fileTypeRegistry)
	if err != nil {
		return nil, err
	}

	// Register buffer
	e.bufferRegistry[absPath] = buf
	return buf, nil
}

// getPanesForBuffer returns all panes viewing the given buffer
func (e *Editor) getPanesForBuffer(buf *Buffer) []*Pane {
	var result []*Pane
	for _, p := range e.panes {
		if p.Buffer == buf {
			result = append(result, p)
		}
	}
	return result
}

// adjustOtherPaneCursors adjusts cursors in other panes viewing the same buffer
// after lines are added or removed
func (e *Editor) adjustOtherPaneCursors(buf *Buffer, editRow, linesDelta int) {
	if linesDelta == 0 {
		return
	}
	activeP := e.activePane()
	for _, p := range e.panes {
		if p.Buffer == buf && p != activeP {
			p.AdjustCursorForEdit(editRow, linesDelta)
		}
	}
}

// New creates a new editor instance
func New(fileInfos []FileInfo, splitMode SplitMode) (*Editor, error) {
	// Load configuration
	config := LoadConfig()
	fileTypeRegistry := NewFileTypeRegistry(config)

	// Initialize buffer registry and panes
	bufferRegistry := make(map[string]*Buffer)
	var panes []*Pane

	for _, fi := range fileInfos {
		// Get absolute path for deduplication
		absPath, err := filepath.Abs(fi.Filename)
		if err != nil {
			return nil, err
		}

		// Check if buffer already exists (same file opened multiple times)
		buf, exists := bufferRegistry[absPath]
		if !exists {
			// Create new buffer
			buf, err = NewBufferWithRegistry(fi.Filename, fileTypeRegistry)
			if err != nil {
				return nil, err
			}
			bufferRegistry[absPath] = buf
		}

		// Create pane for this file (even if buffer already exists)
		var pane *Pane
		if fi.Line > 0 {
			pane = NewPaneAtLine(buf, fi.Line)
		} else {
			pane = NewPane(buf)
		}
		panes = append(panes, pane)
	}

	scr, err := NewScreen()
	if err != nil {
		return nil, err
	}

	// Initialize file watcher
	fw, err := NewFileWatcher(scr)
	if err != nil {
		scr.Close()
		return nil, err
	}

	// Watch all unique buffers (not duplicate panes)
	for _, buf := range bufferRegistry {
		fw.Watch(buf.Filename)
	}

	return &Editor{
		bufferRegistry:   bufferRegistry,
		panes:            panes,
		activePaneIdx:    0,
		screen:           scr,
		splitMode:        splitMode,
		mode:             ModeNormal,
		inputState:       NewInputState(),
		history:          NewHistory(100),
		fileWatcher:      fw,
		config:           config,
		fileTypeRegistry: fileTypeRegistry,
		globalMarks:      make(map[rune]GlobalMark),
	}, nil
}

// NewEditorWithDeps creates an Editor with explicit dependencies for testing.
func NewEditorWithDeps(deps EditorDeps, buffers map[string]*Buffer, panes []*Pane, splitMode SplitMode) *Editor {
	return &Editor{
		bufferRegistry:   buffers,
		panes:            panes,
		activePaneIdx:    0,
		screen:           deps.Screen,
		splitMode:        splitMode,
		mode:             ModeNormal,
		inputState:       NewInputState(),
		history:          NewHistory(100),
		fileWatcher:      deps.FileWatcher,
		config:           deps.Config,
		fileTypeRegistry: deps.FileTypes,
		globalMarks:      make(map[rune]GlobalMark),
	}
}

// Run starts the main editor loop
func (e *Editor) Run() error {
	defer e.screen.Close()
	defer e.stopAutoSave()
	defer e.fileWatcher.Close()

	for {
		e.screen.Render(e.panes, e.activePaneIdx, e.mode, e.inputState, e.splitMode)

		ev := e.screen.PollEvent()

		switch ev := ev.(type) {
		case *tcell.EventKey:
			if e.handleKey(ev) {
				// Save any pending changes before quitting
				e.saveAllModified()
				return nil
			}
		case *tcell.EventResize:
			e.screen.Sync()
		case *FileChangedEvent:
			e.handleExternalFileChange(ev.Filename)
		case *EventMappingTimeout:
			if ev.snapshot.Equal(e.mapKeyTime) && e.pendingMapKeys != "" {
				e.flushPendingMapKeys()
			}
		}
	}
}

// scheduleAutoSave resets the auto-save timer
func (e *Editor) scheduleAutoSave() {
	if e.autoSaveTimer != nil {
		e.autoSaveTimer.Stop()
	}
	e.autoSaveTimer = time.AfterFunc(autoSaveDelay, func() {
		e.saveAllModified()
	})
}

// stopAutoSave stops the auto-save timer
func (e *Editor) stopAutoSave() {
	if e.autoSaveTimer != nil {
		e.autoSaveTimer.Stop()
	}
}

// saveAllModified saves all buffers that have been modified
func (e *Editor) saveAllModified() {
	// Iterate over unique buffers from registry (not panes, to avoid saving same buffer twice)
	for _, buf := range e.bufferRegistry {
		if buf.Modified {
			buf.Save()
			// Update watcher's mod time tracking to avoid false external change detection
			e.fileWatcher.UpdateModTime(buf.Filename)
		}
	}
}

// handleExternalFileChange reloads a file that was modified externally
func (e *Editor) handleExternalFileChange(filename string) {
	// Find the buffer for this file using the registry (already keyed by absolute path)
	buf, exists := e.bufferRegistry[filename]
	if !exists {
		// Try to find by checking absolute paths
		for absPath, b := range e.bufferRegistry {
			if absPath == filename || b.Filename == filename {
				buf = b
				break
			}
		}
	}
	if buf == nil {
		return
	}

	// If in insert mode, commit the current session for the ACTIVE buffer
	// (not the externally changed file, which may be different)
	if e.mode == ModeInsert {
		e.history.CommitSession(e.activeBuffer().Lines)
		e.mode = ModeNormal
	}

	// Snapshot current content for undo using active pane's cursor
	oldLines := copyLines(buf.Lines)
	pane := e.activePane()
	cursorRow, cursorCol := pane.CursorRow, pane.CursorCol
	hadUnsavedChanges := buf.Modified

	// Reload the file
	if err := buf.Load(); err != nil {
		return
	}

	// Update watcher's mod time
	e.fileWatcher.UpdateModTime(filename)

	// Record the change in history so user can undo
	e.history.Push(&Change{
		Type:    ChangeReplace,
		Buffer:  buf,
		Row:     cursorRow,
		Col:     cursorCol,
		Text:    copyLines(buf.Lines),
		OldText: oldLines,
	})

	// If user had unsaved changes, mark buffer as modified so they know
	if hadUnsavedChanges {
		buf.Modified = true
	}

	// Clamp cursors in all panes viewing this buffer
	for _, p := range e.getPanesForBuffer(buf) {
		p.clampCursor()
	}
}

// suspend moves the editor process to background (ctrl+z)
func (e *Editor) suspend() {
	// Save any pending changes before suspending
	e.saveAllModified()
	// Suspend the screen and process
	if err := e.screen.Suspend(); err != nil {
		return
	}
	// Resume will be called after SIGCONT when process is foregrounded
	e.screen.Resume()
}

// checkDoubleShift detects double-shift press to switch panes
// Also handles Shift+Tab as a reliable alternative
func (e *Editor) checkDoubleShift(ev *tcell.EventKey) bool {
	if len(e.panes) <= 1 {
		return false
	}

	mods := ev.Modifiers()

	// Shift+Tab is a reliable way to switch panes
	if ev.Key() == tcell.KeyBacktab {
		// If in insert mode, commit session for current buffer before switching
		if e.mode == ModeInsert {
			e.history.CommitSession(e.activeBuffer().Lines)
		}
		e.activePaneIdx = (e.activePaneIdx + 1) % len(e.panes)
		// If in insert mode, start new session for the new buffer
		if e.mode == ModeInsert {
			pane := e.activePane()
			e.history.StartSession(pane.Buffer, pane.CursorRow, pane.CursorCol, pane.Buffer.Lines)
		}
		return true
	}

	// Detect shift-only events for double-shift detection
	// This works when terminal sends shift key events
	isShiftOnlyMod := mods == tcell.ModShift
	isNonPrintable := ev.Key() != tcell.KeyRune || ev.Rune() == 0

	if isShiftOnlyMod && isNonPrintable {
		now := time.Now()
		if !e.lastShiftTime.IsZero() && now.Sub(e.lastShiftTime) < DoubleShiftTimeout {
			// Double shift detected - switch panes
			// If in insert mode, commit session for current buffer before switching
			if e.mode == ModeInsert {
				e.history.CommitSession(e.activeBuffer().Lines)
			}
			e.activePaneIdx = (e.activePaneIdx + 1) % len(e.panes)
			// If in insert mode, start new session for the new buffer
			if e.mode == ModeInsert {
				pane := e.activePane()
				e.history.StartSession(pane.Buffer, pane.CursorRow, pane.CursorCol, pane.Buffer.Lines)
			}
			e.lastShiftTime = time.Time{} // Reset to prevent triple-switch
			return true
		}
		e.lastShiftTime = now
	} else if mods == 0 || (mods&tcell.ModShift == 0) {
		// Reset shift tracking on non-shift key press
		e.lastShiftTime = time.Time{}
	}

	return false
}

// handleKey processes key events, returns true if editor should quit
func (e *Editor) handleKey(ev *tcell.EventKey) bool {
	// Check for double-shift to switch panes (works in all modes)
	if e.checkDoubleShift(ev) {
		return false
	}

	// Global keys that work in all modes
	// Note: Escape+F10 through pty may arrive as Alt+F10, handle both
	if ev.Key() == tcell.KeyF10 || ev.Name() == "Alt+F10" {
		e.saveAllModified()
		return true // quit
	}
	switch ev.Key() {
	case tcell.KeyCtrlZ:
		e.suspend()
		return false
	case tcell.KeyF3:
		e.openFindReplaceWidget()
		return false
	case tcell.KeyF4:
		e.openSearchWidget()
		return false
	}

	// Check if widget mode is active - handle widget input first
	if e.inputState.HasActiveWidget() {
		return e.handleWidgetMode(ev)
	}

	// Check if legacy search mode is active
	if e.inputState.Search != nil && e.inputState.Search.Active {
		return e.handleSearchMode(ev)
	}

	// Try to handle key mapping (only in insert mode for now)
	if e.mode == ModeInsert && e.tryKeyMapping(ev) {
		return false
	}

	// Dispatch to mode-specific handler
	switch e.mode {
	case ModeNormal:
		return e.handleNormalMode(ev)
	case ModeInsert:
		return e.handleInsertMode(ev)
	case ModeVisual:
		return e.handleVisualMode(ev)
	}

	return false
}

// tryKeyMapping checks if the current key should trigger a mapping
// Returns true if the key was consumed by the mapping system
func (e *Editor) tryKeyMapping(ev *tcell.EventKey) bool {
	maps := e.config.GetMaps()
	if len(maps) == 0 {
		return false
	}

	// Only handle rune keys for mappings
	if ev.Key() != tcell.KeyRune {
		// Reset pending keys on non-rune keys
		if e.pendingMapKeys != "" {
			// Process pending keys as normal input
			e.flushPendingMapKeys()
		}
		return false
	}

	ch := ev.Rune()
	e.pendingMapKeys += string(ch)

	// Check if any mapping starts with our pending keys
	hasPrefix := false
	for trigger := range maps {
		if len(trigger) >= len(e.pendingMapKeys) && trigger[:len(e.pendingMapKeys)] == e.pendingMapKeys {
			hasPrefix = true
			break
		}
	}

	if !hasPrefix {
		// No mapping matches this prefix - process pending keys as normal
		e.flushPendingMapKeys()
		return true // Key was consumed by flushPendingMapKeys
	}

	// Check for exact match
	if expansion, ok := maps[e.pendingMapKeys]; ok {
		// Execute the mapping
		e.executeMapping(expansion)
		e.pendingMapKeys = ""
		e.mapKeyTime = time.Time{}
		return true
	}

	// We have a prefix match but not complete - start/update timeout
	e.mapKeyTime = time.Now()

	// Set up a timeout to flush pending keys if no more input comes
	snapshot := e.mapKeyTime
	go func() {
		time.Sleep(MappingTimeout)
		e.screen.PostEvent(&EventMappingTimeout{
			when:     time.Now(),
			snapshot: snapshot,
		})
	}()

	return true // Consume the key, waiting for more
}

// flushPendingMapKeys processes pending map keys as normal input
func (e *Editor) flushPendingMapKeys() {
	pane := e.activePane()
	buf := pane.Buffer

	for _, ch := range e.pendingMapKeys {
		pane.CursorCol = buf.InsertChar(pane.CursorRow, pane.CursorCol, ch)
	}

	if len(e.pendingMapKeys) > 0 {
		e.scheduleAutoSave()
		e.triggerAutocomplete()
	}
	e.pendingMapKeys = ""
	e.mapKeyTime = time.Time{}
}

// executeMapping executes a key mapping expansion
func (e *Editor) executeMapping(expansion string) {
	keys := ParseSpecialKeys(expansion)
	pane := e.activePane()
	buf := pane.Buffer

	// Dismiss autocomplete popup when a mapping is triggered
	e.inputState.Autocomplete = nil

	for _, key := range keys {
		switch key.Key {
		case tcell.KeyEscape:
			e.mode = ModeNormal
			e.history.CommitSession(buf.Lines)
			e.inputState.Reset()
			if pane.CursorCol > 0 {
				pane.CursorCol--
			}
		case tcell.KeyEnter:
			if e.mode == ModeInsert {
				pane.CursorRow, pane.CursorCol = buf.InsertNewlineWithIndent(pane.CursorRow, pane.CursorCol)
				e.scheduleAutoSave()
			}
		case tcell.KeyTab:
			if e.mode == ModeInsert {
				pane.CursorCol = buf.InsertTab(pane.CursorRow, pane.CursorCol)
				e.scheduleAutoSave()
			}
		case tcell.KeyBackspace2:
			if e.mode == ModeInsert {
				pane.CursorRow, pane.CursorCol = buf.DeleteChar(pane.CursorRow, pane.CursorCol)
				e.scheduleAutoSave()
			}
		case tcell.KeyLeft:
			pane.MoveLeft()
		case tcell.KeyRight:
			pane.MoveRight()
		case tcell.KeyUp:
			pane.MoveUp()
		case tcell.KeyDown:
			pane.MoveDown()
		case tcell.KeyRune:
			if e.mode == ModeInsert {
				pane.CursorCol = buf.InsertChar(pane.CursorRow, pane.CursorCol, key.Rune)
				e.scheduleAutoSave()
			} else if e.mode == ModeNormal {
				e.executeMappingNormalRune(key.Rune)
			}
		}
	}
}

// executeMappingNormalRune handles a rune key in normal mode during mapping execution
func (e *Editor) executeMappingNormalRune(r rune) {
	pane := e.activePane()
	buf := pane.Buffer

	switch r {
	// Navigation
	case 'h':
		pane.MoveLeft()
	case 'j':
		pane.MoveDown()
	case 'k':
		pane.MoveUp()
	case 'l':
		pane.MoveRight()
	case '0':
		pane.MoveToLineStart()
	case '$':
		pane.MoveToLineEnd()
	case 'w':
		pane.MoveToNextWord()
	case 'b':
		pane.MoveToPrevWord()
	case 'G':
		pane.CursorRow = len(buf.Lines) - 1
		pane.CursorCol = 0

	// Mode switching
	case 'i':
		e.enterInsertMode()
	case 'a':
		if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol++
		}
		e.enterInsertMode()
	case 'A':
		pane.MoveToLineEnd()
		e.enterInsertMode()
	case 'I':
		pane.MoveToLineStart()
		e.enterInsertMode()
	case 'o':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineBelow(pane.CursorRow)
		e.scheduleAutoSave()
	case 'O':
		e.enterInsertMode()
		pane.CursorRow, pane.CursorCol = buf.OpenLineAbove(pane.CursorRow)
		e.scheduleAutoSave()

	// Deletion
	case 'x':
		e.deleteCharUnderCursorCmd()
	}
}
