package editor

import (
	"path/filepath"
	"time"

	"github.com/gdamore/tcell/v2"
)

const autoSaveDelay = 200 * time.Millisecond

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

const (
	// MinPaneHeightHorizontal is the minimum height for horizontal splits
	MinPaneHeightHorizontal = 3

	// MinPaneWidthVertical is the minimum width for vertical splits
	MinPaneWidthVertical = 20
)

// FileInfo contains filename and optional starting line number
type FileInfo struct {
	Filename string
	Line     int // 1-based line number, 0 means not specified
}

// Editor is the main editor struct
type Editor struct {
	// Shared buffer model: bufferRegistry holds unique buffers, panes reference them
	bufferRegistry map[string]*Buffer // Buffers keyed by absolute file path
	panes          []*Pane            // Visual panes (each references a buffer)
	activePaneIdx  int                // Index of currently active pane

	screen        *Screen
	splitMode     SplitMode // horizontal or vertical layout
	lastShiftTime time.Time
	autoSaveTimer *time.Timer
	mode          Mode
	inputState    *InputState
	clipboard     [][]rune // Stores copied lines/text
	clipboardLine bool     // true if clipboard contains whole lines (yy/dd)
	history       *History // Undo/redo history
	fileWatcher   *FileWatcher

	// Configuration and filetype support
	config           *Config
	fileTypeRegistry *FileTypeRegistry

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
			e.screen.screen.Sync()
		case *FileChangedEvent:
			e.handleExternalFileChange(ev.Filename)
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
		if !e.lastShiftTime.IsZero() && now.Sub(e.lastShiftTime) < 400*time.Millisecond {
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
	switch ev.Key() {
	case tcell.KeyF10:
		e.saveAllModified()
		return true // quit
	case tcell.KeyCtrlZ:
		e.suspend()
		return false
	case tcell.KeyF4:
		e.enterSearchMode()
		return false
	}

	// Check if search mode is active - handle search input first
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
	if len(e.config.Maps) == 0 {
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
	for trigger := range e.config.Maps {
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
	if expansion, ok := e.config.Maps[e.pendingMapKeys]; ok {
		// Execute the mapping
		e.executeMapping(expansion)
		e.pendingMapKeys = ""
		e.mapKeyTime = time.Time{}
		return true
	}

	// We have a prefix match but not complete - start/update timeout
	e.mapKeyTime = time.Now()

	// Set up a timeout to flush pending keys if no more input comes
	go func() {
		time.Sleep(MappingTimeout)
		// Post a custom event to check timeout (simplified: just note timeout occurred)
	}()

	return true // Consume the key, waiting for more
}

// flushPendingMapKeys processes pending map keys as normal input
func (e *Editor) flushPendingMapKeys() {
	pane := e.activePane()
	buf := pane.Buffer

	// Sync pane cursor to buffer before inserting
	pane.SyncToBuffer()

	for _, ch := range e.pendingMapKeys {
		buf.InsertChar(ch)
	}

	// Sync buffer cursor back to pane after inserting
	pane.SyncFromBuffer()

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

	// Sync pane cursor to buffer before any operations
	pane.SyncToBuffer()

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
				buf.InsertNewlineWithIndent()
				pane.SyncFromBuffer()
				e.scheduleAutoSave()
			}
		case tcell.KeyTab:
			if e.mode == ModeInsert {
				buf.InsertTab()
				pane.SyncFromBuffer()
				e.scheduleAutoSave()
			}
		case tcell.KeyBackspace2:
			if e.mode == ModeInsert {
				buf.DeleteChar()
				pane.SyncFromBuffer()
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
				pane.SyncToBuffer()
				buf.InsertChar(key.Rune)
				pane.SyncFromBuffer()
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
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
	case 'a':
		// Append after cursor
		if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol++
		}
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
	case 'A':
		// Append at end of line
		pane.MoveToLineEnd()
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
	case 'I':
		// Insert at beginning of line
		pane.MoveToLineStart()
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
	case 'o':
		// Open line below
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		buf.OpenLineBelow()
		pane.SyncFromBuffer()
		e.mode = ModeInsert
		e.scheduleAutoSave()
	case 'O':
		// Open line above
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		buf.OpenLineAbove()
		pane.SyncFromBuffer()
		e.mode = ModeInsert
		e.scheduleAutoSave()

	// Deletion
	case 'x':
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		buf.DeleteCharAtCursor()
		pane.SyncFromBuffer()
		e.history.CommitSession(buf.Lines)
		e.scheduleAutoSave()
	}
}

// handleInsertMode handles key events in insert mode (text editing)
func (e *Editor) handleInsertMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	buf := pane.Buffer
	ac := e.inputState.Autocomplete

	// Sync pane cursor to buffer before operations
	pane.SyncToBuffer()

	// Handle autocomplete keys when dropdown is visible
	if ac != nil && ac.Active {
		switch ev.Key() {
		case tcell.KeyTab, tcell.KeyEnter:
			e.acceptAutocomplete()
			pane.SyncFromBuffer()
			return false
		case tcell.KeyEscape:
			e.inputState.Autocomplete = nil
			return false
		case tcell.KeyCtrlN, tcell.KeyDown:
			ac.Next()
			return false
		case tcell.KeyCtrlP, tcell.KeyUp:
			ac.Prev()
			return false
		}
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.inputState.Autocomplete = nil
		e.mode = ModeNormal
		e.history.CommitSession(buf.Lines)
		e.inputState.Reset()
		// Move cursor back one position like vim
		if pane.CursorCol > 0 {
			pane.CursorCol--
		}

	case tcell.KeyUp:
		// Only move cursor if autocomplete is not active (it handles Up itself)
		if ac == nil || !ac.Active {
			pane.MoveUp()
		}

	case tcell.KeyDown:
		// Only move cursor if autocomplete is not active (it handles Down itself)
		if ac == nil || !ac.Active {
			pane.MoveDown()
		}

	case tcell.KeyLeft:
		e.inputState.Autocomplete = nil
		pane.MoveLeft()

	case tcell.KeyRight:
		e.inputState.Autocomplete = nil
		pane.MoveRight()

	case tcell.KeyBackspace, tcell.KeyBackspace2:
		buf.DeleteChar()
		pane.SyncFromBuffer()
		e.scheduleAutoSave()
		e.triggerAutocomplete()

	case tcell.KeyDelete:
		e.inputState.Autocomplete = nil
		buf.DeleteCharForward()
		pane.SyncFromBuffer()
		e.scheduleAutoSave()

	case tcell.KeyEnter:
		e.inputState.Autocomplete = nil
		buf.InsertNewlineWithIndent()
		pane.SyncFromBuffer()
		e.scheduleAutoSave()

	case tcell.KeyCtrlD:
		e.inputState.Autocomplete = nil
		_, height := e.screen.Size()
		pane.PageDown(height)

	case tcell.KeyCtrlU:
		e.inputState.Autocomplete = nil
		_, height := e.screen.Size()
		pane.PageUp(height)

	case tcell.KeyRune:
		buf.InsertChar(ev.Rune())
		pane.SyncFromBuffer()
		e.scheduleAutoSave()
		e.triggerAutocomplete()

	case tcell.KeyTab:
		buf.InsertTab()
		pane.SyncFromBuffer()
		e.scheduleAutoSave()
		e.triggerAutocomplete()
	}

	return false
}

// handleNormalMode handles key events in normal mode (navigation/commands)
func (e *Editor) handleNormalMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	_, height := e.screen.Size()

	// Handle pending goto line mode
	if e.inputState.PendingGotoLine {
		return e.handleGotoLineInput(ev)
	}

	// Handle pending find character
	if e.inputState.PendingFindForward || e.inputState.PendingFindBackward {
		return e.handleFindCharInput(ev)
	}

	// Handle pending mark operations
	if e.inputState.PendingMark || e.inputState.PendingJumpToMark {
		return e.handleMarkInput(ev)
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.inputState.Reset()
		return false

	case tcell.KeyUp:
		pane.MoveUpN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyDown:
		pane.MoveDownN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyLeft:
		pane.MoveLeftN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyRight:
		pane.MoveRightN(e.inputState.GetCount())
		e.inputState.Reset()

	case tcell.KeyCtrlD:
		pane.PageDown(height)
		e.inputState.Reset()

	case tcell.KeyCtrlU:
		pane.PageUp(height)
		e.inputState.Reset()

	case tcell.KeyCtrlR:
		e.redo()
		e.inputState.Reset()

	case tcell.KeyRune:
		return e.handleNormalModeRune(ev.Rune())
	}

	return false
}

// handleNormalModeRune handles rune keys in normal mode
func (e *Editor) handleNormalModeRune(r rune) bool {
	pane := e.activePane()
	buf := pane.Buffer
	count := e.inputState.GetCount()

	// Handle pending operator
	if e.inputState.PendingOperator != 0 {
		return e.handlePendingOperator(r)
	}

	switch r {
	// Numeric prefix
	case '1', '2', '3', '4', '5', '6', '7', '8', '9':
		e.inputState.AddDigit(int(r - '0'))
		return false
	case '0':
		if e.inputState.HasCount {
			e.inputState.AddDigit(0)
		} else {
			pane.MoveToLineStart()
		}
		return false

	// Navigation
	case 'h':
		pane.MoveLeftN(count)
		e.inputState.Reset()
	case 'j':
		pane.MoveDownN(count)
		e.inputState.Reset()
	case 'k':
		pane.MoveUpN(count)
		e.inputState.Reset()
	case 'l':
		pane.MoveRightN(count)
		e.inputState.Reset()
	case '$':
		pane.MoveToLineEnd()
		e.inputState.Reset()

	// Mode switching
	case 'i':
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
		e.inputState.Reset()
	case 'a':
		// Append after cursor
		if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol++
		}
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
		e.inputState.Reset()
	case 'A':
		// Append at end of line
		pane.MoveToLineEnd()
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
		e.inputState.Reset()
	case 'I':
		// Insert at beginning of line
		pane.MoveToLineStart()
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		e.mode = ModeInsert
		e.inputState.Reset()
	case 'o':
		// Open line below - snapshot before modification
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		buf.OpenLineBelow()
		pane.SyncFromBuffer()
		e.mode = ModeInsert
		e.inputState.Reset()
		e.scheduleAutoSave()
	case 'O':
		// Open line above - snapshot before modification
		pane.SyncToBuffer()
		e.history.StartSession(buf, pane.CursorRow, pane.CursorCol, buf.Lines)
		buf.OpenLineAbove()
		pane.SyncFromBuffer()
		e.mode = ModeInsert
		e.inputState.Reset()
		e.scheduleAutoSave()
	case 'v':
		e.mode = ModeVisual
		pane.StartSelection()
		e.inputState.Reset()

	// Find character
	case 'f':
		e.inputState.PendingFindForward = true
	case 'F':
		e.inputState.PendingFindBackward = true
	case ';':
		// Repeat last find
		if e.inputState.HasLastFind {
			for i := 0; i < count; i++ {
				if e.inputState.LastFindForward {
					pane.FindCharForward(e.inputState.LastFindChar)
				} else {
					pane.FindCharBackward(e.inputState.LastFindChar)
				}
			}
		}
		e.inputState.Reset()
	case ',':
		// Repeat last find in reverse
		if e.inputState.HasLastFind {
			for i := 0; i < count; i++ {
				if e.inputState.LastFindForward {
					pane.FindCharBackward(e.inputState.LastFindChar)
				} else {
					pane.FindCharForward(e.inputState.LastFindChar)
				}
			}
		}
		e.inputState.Reset()

	// Goto line
	case ':':
		e.inputState.PendingGotoLine = true
		e.inputState.GotoLineBuffer = ""

	// Page navigation (like G and gg)
	case 'G':
		if e.inputState.HasCount {
			pane.GotoLine(count)
		} else {
			pane.GotoLine(len(buf.Lines)) // Go to last line
		}
		e.inputState.Reset()
	case 'g':
		// For gg, we'd need another pending state, but for simplicity
		// just go to first line on single 'g'
		pane.GotoLine(1)
		e.inputState.Reset()

	// Word navigation
	case 'w':
		for i := 0; i < count; i++ {
			pane.MoveToNextWord()
		}
		e.inputState.Reset()
	case 'b':
		for i := 0; i < count; i++ {
			pane.MoveToPrevWord()
		}
		e.inputState.Reset()

	// Operators (start pending state)
	case 'd':
		e.inputState.PendingOperator = 'd'
		return false
	case 'y':
		e.inputState.PendingOperator = 'y'
		return false

	// Delete character under cursor
	case 'x':
		pane.SyncToBuffer()
		startCol := pane.CursorCol
		var deletedChars []rune
		for i := 0; i < count; i++ {
			if pane.CursorCol < len(buf.Lines[pane.CursorRow]) {
				deleted := buf.DeleteCharAtCursor()
				pane.SyncFromBuffer()
				if deleted != 0 {
					deletedChars = append(deletedChars, deleted)
				}
			}
		}
		if len(deletedChars) > 0 {
			e.clipboard = [][]rune{deletedChars}
			e.clipboardLine = false
			e.history.RecordDelete(buf, pane.CursorRow, startCol, [][]rune{deletedChars})
		}
		e.scheduleAutoSave()
		e.inputState.Reset()

	// Paste after cursor
	case 'p':
		if len(e.clipboard) > 0 {
			pane.SyncToBuffer()
			oldLines := copyLines(buf.Lines)
			cursorRow, cursorCol := pane.CursorRow, pane.CursorCol
			e.pasteAfter()
			pane.SyncFromBuffer()
			e.history.Push(&Change{
				Type:    ChangeReplace,
				Buffer:  buf,
				Row:     cursorRow,
				Col:     cursorCol,
				Text:    copyLines(buf.Lines),
				OldText: oldLines,
			})
		}
		e.inputState.Reset()

	// Paste before cursor
	case 'P':
		if len(e.clipboard) > 0 {
			pane.SyncToBuffer()
			oldLines := copyLines(buf.Lines)
			cursorRow, cursorCol := pane.CursorRow, pane.CursorCol
			e.pasteBefore()
			pane.SyncFromBuffer()
			e.history.Push(&Change{
				Type:    ChangeReplace,
				Buffer:  buf,
				Row:     cursorRow,
				Col:     cursorCol,
				Text:    copyLines(buf.Lines),
				OldText: oldLines,
			})
		}
		e.inputState.Reset()

	// Undo
	case 'u':
		e.undo()
		e.inputState.Reset()

	// Mark operations
	case 'm':
		e.inputState.PendingMark = true
		return false
	case '`':
		e.inputState.PendingJumpToMark = true
		return false

	default:
		e.inputState.Reset()
	}

	return false
}

// pasteAfter pastes clipboard content after cursor (p command)
func (e *Editor) pasteAfter() {
	if len(e.clipboard) == 0 {
		return
	}

	buf := e.activeBuffer()

	if e.clipboardLine {
		// Paste whole lines after current line
		for i := len(e.clipboard) - 1; i >= 0; i-- {
			lineCopy := make([]rune, len(e.clipboard[i]))
			copy(lineCopy, e.clipboard[i])
			buf.InsertLineAfter(buf.CursorRow, lineCopy)
		}
		buf.CursorRow++
		buf.CursorCol = 0
	} else {
		// Paste text after cursor position
		if len(e.clipboard) == 1 {
			// Single line paste
			line := buf.Lines[buf.CursorRow]
			insertPos := buf.CursorCol + 1
			if insertPos > len(line) {
				insertPos = len(line)
			}
			newLine := make([]rune, len(line)+len(e.clipboard[0]))
			copy(newLine[:insertPos], line[:insertPos])
			copy(newLine[insertPos:], e.clipboard[0])
			copy(newLine[insertPos+len(e.clipboard[0]):], line[insertPos:])
			buf.Lines[buf.CursorRow] = newLine
			buf.CursorCol = insertPos + len(e.clipboard[0]) - 1
			if buf.CursorCol < 0 {
				buf.CursorCol = 0
			}
		} else {
			// Multi-line paste
			line := buf.Lines[buf.CursorRow]
			insertPos := buf.CursorCol + 1
			if insertPos > len(line) {
				insertPos = len(line)
			}

			// First part of current line + first clipboard line
			firstPart := make([]rune, insertPos+len(e.clipboard[0]))
			copy(firstPart[:insertPos], line[:insertPos])
			copy(firstPart[insertPos:], e.clipboard[0])

			// Last clipboard line + rest of current line
			lastPart := make([]rune, len(e.clipboard[len(e.clipboard)-1])+len(line)-insertPos)
			copy(lastPart, e.clipboard[len(e.clipboard)-1])
			copy(lastPart[len(e.clipboard[len(e.clipboard)-1]):], line[insertPos:])

			// Build new lines
			newLines := make([][]rune, len(buf.Lines)+len(e.clipboard)-1)
			copy(newLines[:buf.CursorRow], buf.Lines[:buf.CursorRow])
			newLines[buf.CursorRow] = firstPart

			// Middle clipboard lines
			for i := 1; i < len(e.clipboard)-1; i++ {
				lineCopy := make([]rune, len(e.clipboard[i]))
				copy(lineCopy, e.clipboard[i])
				newLines[buf.CursorRow+i] = lineCopy
			}

			newLines[buf.CursorRow+len(e.clipboard)-1] = lastPart
			copy(newLines[buf.CursorRow+len(e.clipboard):], buf.Lines[buf.CursorRow+1:])

			buf.Lines = newLines
			buf.CursorRow += len(e.clipboard) - 1
			buf.CursorCol = len(e.clipboard[len(e.clipboard)-1]) - 1
			if buf.CursorCol < 0 {
				buf.CursorCol = 0
			}
		}
	}

	buf.Modified = true
	e.scheduleAutoSave()
}

// undo reverses the last change
func (e *Editor) undo() {
	change := e.history.Undo()
	if change == nil {
		return
	}

	// Use the buffer from the change, not the active buffer
	buf := change.Buffer
	if buf == nil {
		// Fallback for legacy changes without buffer reference
		buf = e.activeBuffer()
	}

	// Switch to a pane viewing this buffer if current pane doesn't
	if e.activePane().Buffer != buf {
		for i, p := range e.panes {
			if p.Buffer == buf {
				e.activePaneIdx = i
				break
			}
		}
	}

	switch change.Type {
	case ChangeInsert:
		// Undo insert = delete the inserted text
		if len(change.Text) == 1 {
			// Single line: delete the characters
			line := buf.Lines[change.Row]
			textLen := len(change.Text[0])
			if change.Col+textLen <= len(line) {
				newLine := make([]rune, len(line)-textLen)
				copy(newLine[:change.Col], line[:change.Col])
				copy(newLine[change.Col:], line[change.Col+textLen:])
				buf.Lines[change.Row] = newLine
			}
		} else {
			// Multi-line: restore to original state
			// Remove inserted lines and restore original line
			endRow := change.Row + len(change.Text) - 1
			if endRow < len(buf.Lines) {
				// Get the part after inserted text on last line
				lastLine := buf.Lines[endRow]
				afterInsert := lastLine[len(change.Text[len(change.Text)-1]):]

				// Reconstruct original line
				firstPart := buf.Lines[change.Row][:change.Col]
				newLine := make([]rune, len(firstPart)+len(afterInsert))
				copy(newLine, firstPart)
				copy(newLine[len(firstPart):], afterInsert)

				// Remove inserted lines
				buf.Lines = append(buf.Lines[:change.Row], append([][]rune{newLine}, buf.Lines[endRow+1:]...)...)
			}
		}
		buf.CursorRow = change.Row
		buf.CursorCol = change.Col
		buf.Modified = true

	case ChangeDelete:
		// Undo delete = insert the deleted text back
		if change.LineDeletion {
			// Whole-line deletion (dd command) - insert lines back
			newLines := make([][]rune, len(buf.Lines)+len(change.Text))
			copy(newLines[:change.Row], buf.Lines[:change.Row])
			for i, line := range change.Text {
				lineCopy := make([]rune, len(line))
				copy(lineCopy, line)
				newLines[change.Row+i] = lineCopy
			}
			copy(newLines[change.Row+len(change.Text):], buf.Lines[change.Row:])
			buf.Lines = newLines
		} else if len(change.Text) == 1 {
			// Single line: insert the characters back
			line := buf.Lines[change.Row]
			newLine := make([]rune, len(line)+len(change.Text[0]))
			copy(newLine[:change.Col], line[:change.Col])
			copy(newLine[change.Col:], change.Text[0])
			copy(newLine[change.Col+len(change.Text[0]):], line[change.Col:])
			buf.Lines[change.Row] = newLine
		} else {
			// Multi-line: insert deleted lines back
			// Split current line at the delete position
			line := buf.Lines[change.Row]
			firstPart := line[:change.Col]
			restPart := line[change.Col:]

			// First line = firstPart + first deleted text
			newFirstLine := make([]rune, len(firstPart)+len(change.Text[0]))
			copy(newFirstLine, firstPart)
			copy(newFirstLine[len(firstPart):], change.Text[0])

			// Last line = last deleted text + restPart
			lastDeleted := change.Text[len(change.Text)-1]
			newLastLine := make([]rune, len(lastDeleted)+len(restPart))
			copy(newLastLine, lastDeleted)
			copy(newLastLine[len(lastDeleted):], restPart)

			// Build new lines slice
			newLines := make([][]rune, len(buf.Lines)+len(change.Text)-1)
			copy(newLines[:change.Row], buf.Lines[:change.Row])
			newLines[change.Row] = newFirstLine

			// Middle lines
			for i := 1; i < len(change.Text)-1; i++ {
				lineCopy := make([]rune, len(change.Text[i]))
				copy(lineCopy, change.Text[i])
				newLines[change.Row+i] = lineCopy
			}

			newLines[change.Row+len(change.Text)-1] = newLastLine
			copy(newLines[change.Row+len(change.Text):], buf.Lines[change.Row+1:])

			buf.Lines = newLines
		}
		buf.CursorRow = change.Row
		buf.CursorCol = change.Col
		buf.Modified = true

	case ChangeReplace:
		// Undo replace = restore old lines
		buf.Lines = copyLines(change.OldText)
		buf.CursorRow = change.Row
		buf.CursorCol = change.Col
		if buf.CursorRow >= len(buf.Lines) {
			buf.CursorRow = len(buf.Lines) - 1
		}
		buf.clampCursorCol()
		buf.Modified = true
	}

	buf.clampCursorCol()

	// Sync cursor position to active pane
	e.activePane().SyncFromBuffer()

	e.scheduleAutoSave()
}

// redo reapplies the last undone change
func (e *Editor) redo() {
	change := e.history.Redo()
	if change == nil {
		return
	}

	// Use the buffer from the change, not the active buffer
	buf := change.Buffer
	if buf == nil {
		// Fallback for legacy changes without buffer reference
		buf = e.activeBuffer()
	}

	// Switch to a pane viewing this buffer if current pane doesn't
	if e.activePane().Buffer != buf {
		for i, p := range e.panes {
			if p.Buffer == buf {
				e.activePaneIdx = i
				break
			}
		}
	}

	switch change.Type {
	case ChangeInsert:
		// Redo insert = insert the text again
		if len(change.Text) == 1 {
			line := buf.Lines[change.Row]
			newLine := make([]rune, len(line)+len(change.Text[0]))
			copy(newLine[:change.Col], line[:change.Col])
			copy(newLine[change.Col:], change.Text[0])
			copy(newLine[change.Col+len(change.Text[0]):], line[change.Col:])
			buf.Lines[change.Row] = newLine
			buf.CursorCol = change.Col + len(change.Text[0])
		} else {
			// Multi-line insert
			line := buf.Lines[change.Row]
			firstPart := line[:change.Col]
			restPart := line[change.Col:]

			newFirstLine := make([]rune, len(firstPart)+len(change.Text[0]))
			copy(newFirstLine, firstPart)
			copy(newFirstLine[len(firstPart):], change.Text[0])

			lastText := change.Text[len(change.Text)-1]
			newLastLine := make([]rune, len(lastText)+len(restPart))
			copy(newLastLine, lastText)
			copy(newLastLine[len(lastText):], restPart)

			newLines := make([][]rune, len(buf.Lines)+len(change.Text)-1)
			copy(newLines[:change.Row], buf.Lines[:change.Row])
			newLines[change.Row] = newFirstLine

			for i := 1; i < len(change.Text)-1; i++ {
				lineCopy := make([]rune, len(change.Text[i]))
				copy(lineCopy, change.Text[i])
				newLines[change.Row+i] = lineCopy
			}

			newLines[change.Row+len(change.Text)-1] = newLastLine
			copy(newLines[change.Row+len(change.Text):], buf.Lines[change.Row+1:])

			buf.Lines = newLines
			buf.CursorRow = change.Row + len(change.Text) - 1
			buf.CursorCol = len(lastText)
		}
		buf.Modified = true

	case ChangeDelete:
		// Redo delete = delete the text again
		if change.LineDeletion {
			// Whole-line deletion - remove the lines
			endRow := change.Row + len(change.Text)
			if endRow <= len(buf.Lines) {
				buf.Lines = append(buf.Lines[:change.Row], buf.Lines[endRow:]...)
			}
			// Ensure at least one line exists
			if len(buf.Lines) == 0 {
				buf.Lines = [][]rune{{}}
			}
		} else if len(change.Text) == 1 {
			line := buf.Lines[change.Row]
			textLen := len(change.Text[0])
			if change.Col+textLen <= len(line) {
				newLine := make([]rune, len(line)-textLen)
				copy(newLine[:change.Col], line[:change.Col])
				copy(newLine[change.Col:], line[change.Col+textLen:])
				buf.Lines[change.Row] = newLine
			}
		} else {
			// Multi-line delete
			endRow := change.Row + len(change.Text) - 1
			if endRow < len(buf.Lines) {
				lastLine := buf.Lines[endRow]
				afterDelete := lastLine[len(change.Text[len(change.Text)-1]):]

				firstPart := buf.Lines[change.Row][:change.Col]
				newLine := make([]rune, len(firstPart)+len(afterDelete))
				copy(newLine, firstPart)
				copy(newLine[len(firstPart):], afterDelete)

				buf.Lines = append(buf.Lines[:change.Row], append([][]rune{newLine}, buf.Lines[endRow+1:]...)...)
			}
		}
		buf.CursorRow = change.Row
		buf.CursorCol = change.Col
		if buf.CursorRow >= len(buf.Lines) {
			buf.CursorRow = len(buf.Lines) - 1
		}
		buf.Modified = true

	case ChangeReplace:
		// Redo replace = restore new lines
		buf.Lines = copyLines(change.Text)
		buf.CursorRow = change.Row
		buf.CursorCol = change.Col
		if buf.CursorRow >= len(buf.Lines) {
			buf.CursorRow = len(buf.Lines) - 1
		}
		buf.clampCursorCol()
		buf.Modified = true
	}

	buf.clampCursorCol()

	// Sync cursor position to active pane
	e.activePane().SyncFromBuffer()

	e.scheduleAutoSave()
}

// pasteBefore pastes clipboard content before cursor (P command)
func (e *Editor) pasteBefore() {
	if len(e.clipboard) == 0 {
		return
	}

	buf := e.activeBuffer()

	if e.clipboardLine {
		// Paste whole lines before current line
		for i := len(e.clipboard) - 1; i >= 0; i-- {
			lineCopy := make([]rune, len(e.clipboard[i]))
			copy(lineCopy, e.clipboard[i])
			buf.InsertLineBefore(buf.CursorRow, lineCopy)
		}
		buf.CursorCol = 0
	} else {
		// Paste text at cursor position
		if len(e.clipboard) == 1 {
			// Single line paste
			line := buf.Lines[buf.CursorRow]
			insertPos := buf.CursorCol
			newLine := make([]rune, len(line)+len(e.clipboard[0]))
			copy(newLine[:insertPos], line[:insertPos])
			copy(newLine[insertPos:], e.clipboard[0])
			copy(newLine[insertPos+len(e.clipboard[0]):], line[insertPos:])
			buf.Lines[buf.CursorRow] = newLine
			buf.CursorCol = insertPos + len(e.clipboard[0]) - 1
			if buf.CursorCol < 0 {
				buf.CursorCol = 0
			}
		} else {
			// Multi-line paste
			line := buf.Lines[buf.CursorRow]
			insertPos := buf.CursorCol

			// First part of current line + first clipboard line
			firstPart := make([]rune, insertPos+len(e.clipboard[0]))
			copy(firstPart[:insertPos], line[:insertPos])
			copy(firstPart[insertPos:], e.clipboard[0])

			// Last clipboard line + rest of current line
			lastPart := make([]rune, len(e.clipboard[len(e.clipboard)-1])+len(line)-insertPos)
			copy(lastPart, e.clipboard[len(e.clipboard)-1])
			copy(lastPart[len(e.clipboard[len(e.clipboard)-1]):], line[insertPos:])

			// Build new lines
			newLines := make([][]rune, len(buf.Lines)+len(e.clipboard)-1)
			copy(newLines[:buf.CursorRow], buf.Lines[:buf.CursorRow])
			newLines[buf.CursorRow] = firstPart

			// Middle clipboard lines
			for i := 1; i < len(e.clipboard)-1; i++ {
				lineCopy := make([]rune, len(e.clipboard[i]))
				copy(lineCopy, e.clipboard[i])
				newLines[buf.CursorRow+i] = lineCopy
			}

			newLines[buf.CursorRow+len(e.clipboard)-1] = lastPart
			copy(newLines[buf.CursorRow+len(e.clipboard):], buf.Lines[buf.CursorRow+1:])

			buf.Lines = newLines
			buf.CursorRow += len(e.clipboard) - 1
			buf.CursorCol = len(e.clipboard[len(e.clipboard)-1]) - 1
			if buf.CursorCol < 0 {
				buf.CursorCol = 0
			}
		}
	}

	buf.Modified = true
	e.scheduleAutoSave()
}

// handlePendingOperator handles the second key after an operator (d, y)
func (e *Editor) handlePendingOperator(r rune) bool {
	pane := e.activePane()
	buf := pane.Buffer
	op := e.inputState.PendingOperator
	count := e.inputState.GetCount()

	// Sync pane cursor to buffer before operations
	pane.SyncToBuffer()

	switch {
	// dd - delete line(s)
	case op == 'd' && r == 'd':
		startRow := pane.CursorRow
		var deleted [][]rune
		for i := 0; i < count && pane.CursorRow < len(buf.Lines); i++ {
			line := buf.DeleteLine(pane.CursorRow)
			deleted = append(deleted, line)
		}
		e.clipboard = deleted
		e.clipboardLine = true
		// Record for undo - treat as deletion of full lines
		e.history.RecordDeleteLines(buf, startRow, deleted)
		// Clamp cursor after deletion
		pane.clampCursor()
		e.scheduleAutoSave()

	// yy - yank line(s)
	case op == 'y' && r == 'y':
		var yanked [][]rune
		for i := 0; i < count && pane.CursorRow+i < len(buf.Lines); i++ {
			yanked = append(yanked, buf.CopyLine(pane.CursorRow+i))
		}
		e.clipboard = yanked
		e.clipboardLine = true

	// dw - delete word
	case op == 'd' && r == 'w':
		startRow, startCol := pane.CursorRow, pane.CursorCol
		for i := 0; i < count; i++ {
			pane.MoveToNextWord()
		}
		endRow, endCol := pane.CursorRow, pane.CursorCol
		// Restore cursor and delete range
		pane.CursorRow, pane.CursorCol = startRow, startCol
		pane.SyncToBuffer()
		deleted := buf.DeleteRange(startRow, startCol, endRow, endCol)
		pane.SyncFromBuffer()
		e.clipboard = deleted
		e.clipboardLine = false
		e.history.RecordDelete(buf, startRow, startCol, deleted)
		e.scheduleAutoSave()

	// db - delete word backward
	case op == 'd' && r == 'b':
		endRow, endCol := pane.CursorRow, pane.CursorCol
		for i := 0; i < count; i++ {
			pane.MoveToPrevWord()
		}
		startRow, startCol := pane.CursorRow, pane.CursorCol
		pane.SyncToBuffer()
		deleted := buf.DeleteRange(startRow, startCol, endRow, endCol)
		pane.SyncFromBuffer()
		e.clipboard = deleted
		e.clipboardLine = false
		e.history.RecordDelete(buf, startRow, startCol, deleted)
		e.scheduleAutoSave()

	// d$ - delete to end of line
	case op == 'd' && r == '$':
		line := buf.Lines[pane.CursorRow]
		if pane.CursorCol < len(line) {
			deleted := make([]rune, len(line)-pane.CursorCol)
			copy(deleted, line[pane.CursorCol:])
			buf.Lines[pane.CursorRow] = line[:pane.CursorCol]
			e.clipboard = [][]rune{deleted}
			e.clipboardLine = false
			e.history.RecordDelete(buf, pane.CursorRow, pane.CursorCol, [][]rune{deleted})
			buf.Modified = true
			e.scheduleAutoSave()
		}

	// d0 - delete to beginning of line
	case op == 'd' && r == '0':
		line := buf.Lines[pane.CursorRow]
		if pane.CursorCol > 0 {
			deleted := make([]rune, pane.CursorCol)
			copy(deleted, line[:pane.CursorCol])
			buf.Lines[pane.CursorRow] = line[pane.CursorCol:]
			e.history.RecordDelete(buf, pane.CursorRow, 0, [][]rune{deleted})
			pane.CursorCol = 0
			e.clipboard = [][]rune{deleted}
			e.clipboardLine = false
			buf.Modified = true
			e.scheduleAutoSave()
		}

	default:
		// Unknown operator combination, just reset
	}

	e.inputState.Reset()
	return false
}

// handleFindCharInput handles character input after f or F
func (e *Editor) handleFindCharInput(ev *tcell.EventKey) bool {
	pane := e.activePane()
	count := e.inputState.GetCount()

	if ev.Key() == tcell.KeyEscape {
		e.inputState.Reset()
		return false
	}

	if ev.Key() == tcell.KeyRune {
		ch := ev.Rune()
		forward := e.inputState.PendingFindForward

		for i := 0; i < count; i++ {
			if forward {
				pane.FindCharForward(ch)
			} else {
				pane.FindCharBackward(ch)
			}
		}

		e.inputState.SaveLastFind(ch, forward)
		e.inputState.Reset()
	}

	return false
}

// handleGotoLineInput handles input in goto line mode (after :)
func (e *Editor) handleGotoLineInput(ev *tcell.EventKey) bool {
	pane := e.activePane()

	switch ev.Key() {
	case tcell.KeyEscape:
		e.inputState.Reset()
	case tcell.KeyEnter:
		// Execute goto line
		if e.inputState.GotoLineBuffer != "" {
			lineNum := 0
			for _, ch := range e.inputState.GotoLineBuffer {
				if ch >= '0' && ch <= '9' {
					lineNum = lineNum*10 + int(ch-'0')
				}
			}
			if lineNum > 0 {
				pane.GotoLine(lineNum)
			}
		}
		e.inputState.Reset()
	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(e.inputState.GotoLineBuffer) > 0 {
			e.inputState.GotoLineBuffer = e.inputState.GotoLineBuffer[:len(e.inputState.GotoLineBuffer)-1]
		}
	case tcell.KeyRune:
		ch := ev.Rune()
		if ch >= '0' && ch <= '9' {
			e.inputState.GotoLineBuffer += string(ch)
		}
	}

	return false
}

// handleVisualMode handles key events in visual mode (selection)
func (e *Editor) handleVisualMode(ev *tcell.EventKey) bool {
	pane := e.activePane()
	buf := pane.Buffer
	_, height := e.screen.Size()

	// Handle pending find character
	if e.inputState.PendingFindForward || e.inputState.PendingFindBackward {
		if ev.Key() == tcell.KeyEscape {
			e.inputState.Reset()
			return false
		}
		if ev.Key() == tcell.KeyRune {
			ch := ev.Rune()
			forward := e.inputState.PendingFindForward
			if forward {
				pane.FindCharForward(ch)
			} else {
				pane.FindCharBackward(ch)
			}
			e.inputState.SaveLastFind(ch, forward)
			e.inputState.Reset()
		}
		return false
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.mode = ModeNormal
		pane.ClearSelection()
		e.inputState.Reset()

	case tcell.KeyUp:
		pane.MoveUp()

	case tcell.KeyDown:
		pane.MoveDown()

	case tcell.KeyLeft:
		pane.MoveLeft()

	case tcell.KeyRight:
		pane.MoveRight()

	case tcell.KeyCtrlD:
		pane.PageDown(height)

	case tcell.KeyCtrlU:
		pane.PageUp(height)

	case tcell.KeyRune:
		switch ev.Rune() {
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
		case 'v':
			// Exit visual mode
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
		case 'G':
			pane.GotoLine(len(buf.Lines))
		case 'g':
			pane.GotoLine(1)
		case 'w':
			pane.MoveToNextWord()
		case 'b':
			pane.MoveToPrevWord()

		// Find character in line
		case 'f':
			e.inputState.PendingFindForward = true
		case 'F':
			e.inputState.PendingFindBackward = true
		case ';':
			// Repeat last find
			if e.inputState.HasLastFind {
				if e.inputState.LastFindForward {
					pane.FindCharForward(e.inputState.LastFindChar)
				} else {
					pane.FindCharBackward(e.inputState.LastFindChar)
				}
			}
		case ',':
			// Repeat last find in reverse
			if e.inputState.HasLastFind {
				if e.inputState.LastFindForward {
					pane.FindCharBackward(e.inputState.LastFindChar)
				} else {
					pane.FindCharForward(e.inputState.LastFindChar)
				}
			}

		// Copy selection
		case 'y':
			startRow, startCol, endRow, endCol := pane.GetSelection()
			e.clipboard = buf.GetRange(startRow, startCol, endRow, endCol)
			e.clipboardLine = false
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()

		// Delete selection (d and x both delete selection in visual mode)
		case 'd', 'x':
			startRow, startCol, endRow, endCol := pane.GetSelection()
			pane.SyncToBuffer()
			e.clipboard = buf.DeleteRange(startRow, startCol, endRow, endCol+1)
			pane.SyncFromBuffer()
			e.clipboardLine = false
			e.history.RecordDelete(buf, startRow, startCol, e.clipboard)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Indent selection
		case '>':
			startRow, _, endRow, _ := pane.GetSelection()
			buf.IndentRange(startRow, endRow)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Unindent selection
		case '<':
			startRow, _, endRow, _ := pane.GetSelection()
			buf.UnindentRange(startRow, endRow)
			e.mode = ModeNormal
			pane.ClearSelection()
			e.inputState.Reset()
			e.scheduleAutoSave()

		// Paste (replace selection)
		case 'p', 'P':
			if len(e.clipboard) > 0 {
				// Take snapshot for undo
				oldLines := copyLines(buf.Lines)
				startRow, startCol, endRow, endCol := pane.GetSelection()

				pane.SyncToBuffer()
				buf.DeleteRange(startRow, startCol, endRow, endCol+1)
				pane.SyncFromBuffer()
				pane.ClearSelection()
				e.mode = ModeNormal
				// Now paste at cursor
				if ev.Rune() == 'p' {
					e.pasteAfter()
				} else {
					e.pasteBefore()
				}
				pane.SyncFromBuffer()

				// Record the combined delete+paste operation for undo
				e.history.Push(&Change{
					Type:    ChangeReplace,
					Buffer:  buf,
					Row:     startRow,
					Col:     startCol,
					Text:    copyLines(buf.Lines),
					OldText: oldLines,
				})
				e.scheduleAutoSave()
			}
			e.inputState.Reset()
		}
	}

	return false
}

// enterSearchMode initializes search mode with current cursor position as CursorZero
// If a previous confirmed search exists, restore the query for immediate navigation
// If already in search mode with a confirmed search, F4 returns focus to the search bar
func (e *Editor) enterSearchMode() {
	// If search mode is already active and confirmed, return focus to query editing (FR-026)
	if e.inputState.Search != nil && e.inputState.Search.Active && e.inputState.Search.Confirmed {
		e.inputState.Search.Confirmed = false
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	search := NewSearchState(pane.CursorRow, pane.CursorCol)

	// Restore previous query if available (FR-024)
	if e.lastSearchQuery != "" {
		search.Query = e.lastSearchQuery
		search.Confirmed = true // Enable immediate n/N navigation (FR-025)

		// Calculate matches for restored query (T047)
		search.Matches = buf.FindAllMatches(search.Query)
		search.NoMatches = len(search.Matches) == 0

		// Find first match after current cursor (T048)
		if len(search.Matches) > 0 {
			search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, pane.CursorRow, pane.CursorCol)
			if search.CurrentIndex >= 0 {
				match := search.Matches[search.CurrentIndex]
				pane.CursorRow = match.Row
				pane.CursorCol = match.Col
			}
		}
	}

	e.inputState.Search = search
}

// handleSearchMode processes key events while in search mode
func (e *Editor) handleSearchMode(ev *tcell.EventKey) bool {
	search := e.inputState.Search
	if search == nil {
		return false
	}

	switch ev.Key() {
	case tcell.KeyEscape:
		e.exitSearchMode()
		return false

	case tcell.KeyEnter:
		if !search.Confirmed {
			e.confirmSearch()
		} else if search.IsReplaceMode {
			e.replaceCurrentMatch()
		}
		return false

	case tcell.KeyBackspace, tcell.KeyBackspace2:
		if len(search.Query) > 0 {
			search.Query = search.Query[:len(search.Query)-1]
			e.updateSearchMatches()
		}
		return false

	case tcell.KeyRune:
		if search.Confirmed {
			// Handle n/N navigation after search confirmed
			switch ev.Rune() {
			case 'n':
				e.nextMatch()
			case 'N':
				e.prevMatch()
			}
		} else {
			// Accumulate query characters
			search.Query += string(ev.Rune())
			e.updateSearchMatches()
		}
		return false
	}

	return false
}

// updateSearchMatches recalculates matches based on current query
func (e *Editor) updateSearchMatches() {
	search := e.inputState.Search
	if search == nil {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	if search.Query == "" {
		search.Matches = nil
		search.CurrentIndex = -1
		search.NoMatches = false
		return
	}

	search.Matches = buf.FindAllMatches(search.Query)
	search.NoMatches = len(search.Matches) == 0

	// Find first match after CursorZero for incremental search highlighting
	if len(search.Matches) > 0 {
		search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, search.CursorZeroRow, search.CursorZeroCol)
		// Move cursor to show incremental match
		if search.CurrentIndex >= 0 {
			match := search.Matches[search.CurrentIndex]
			pane.CursorRow = match.Row
			pane.CursorCol = match.Col
		}
	} else {
		search.CurrentIndex = -1
	}
}

// confirmSearch confirms the search query and enables n/N navigation
func (e *Editor) confirmSearch() {
	search := e.inputState.Search
	if search == nil || search.Query == "" {
		return
	}

	// Parse for replace syntax
	query, replacement, isReplace := ParseQuery(search.Query)
	if isReplace {
		search.Query = query
		search.IsReplaceMode = true
		search.ReplaceText = replacement
		// Recalculate matches with the actual search query
		e.updateSearchMatches()
	}

	search.Confirmed = true

	// Navigate to first match after CursorZero
	if len(search.Matches) > 0 && search.CurrentIndex >= 0 {
		e.navigateToCurrentMatch()
	}
}

// navigateToCurrentMatch moves cursor to the current match position
func (e *Editor) navigateToCurrentMatch() {
	search := e.inputState.Search
	if search == nil || search.CurrentIndex < 0 || search.CurrentIndex >= len(search.Matches) {
		return
	}

	pane := e.activePane()
	match := search.Matches[search.CurrentIndex]
	pane.CursorRow = match.Row
	pane.CursorCol = match.Col
}

// nextMatch moves to the next match (wrapping at end)
func (e *Editor) nextMatch() {
	search := e.inputState.Search
	if search == nil || len(search.Matches) == 0 {
		return
	}

	search.CurrentIndex = (search.CurrentIndex + 1) % len(search.Matches)
	e.navigateToCurrentMatch()
}

// prevMatch moves to the previous match (wrapping at start)
func (e *Editor) prevMatch() {
	search := e.inputState.Search
	if search == nil || len(search.Matches) == 0 {
		return
	}

	search.CurrentIndex--
	if search.CurrentIndex < 0 {
		search.CurrentIndex = len(search.Matches) - 1
	}
	e.navigateToCurrentMatch()
}

// exitSearchMode exits search mode and positions cursor appropriately
func (e *Editor) exitSearchMode() {
	search := e.inputState.Search
	if search == nil {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer

	// Save query for restoration on F4 re-entry (only if confirmed)
	if search.Confirmed && search.Query != "" {
		e.lastSearchQuery = search.Query
	}

	// If search was confirmed and we have a match, position cursor at end of match
	if search.Confirmed && search.CurrentIndex >= 0 && search.CurrentIndex < len(search.Matches) {
		match := search.Matches[search.CurrentIndex]
		pane.CursorRow = match.Row
		pane.CursorCol = match.Col + match.Length
		// Clamp to line bounds
		if pane.CursorCol > len(buf.Lines[pane.CursorRow]) {
			pane.CursorCol = len(buf.Lines[pane.CursorRow])
		}
	} else {
		// Return to CursorZero (cancelled or no match)
		pane.CursorRow = search.CursorZeroRow
		pane.CursorCol = search.CursorZeroCol
	}

	// Clear search state
	e.inputState.Search = nil
}

// replaceCurrentMatch replaces the current match with replacement text
func (e *Editor) replaceCurrentMatch() {
	search := e.inputState.Search
	if search == nil || !search.IsReplaceMode {
		return
	}
	if search.CurrentIndex < 0 || search.CurrentIndex >= len(search.Matches) {
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	match := search.Matches[search.CurrentIndex]

	// Snapshot for undo - get the text being replaced
	oldLines := copyLines(buf.Lines)

	// Position cursor at match start
	pane.CursorRow = match.Row
	pane.CursorCol = match.Col

	// Delete the matched text
	line := buf.Lines[match.Row]
	endCol := match.Col + match.Length
	if endCol > len(line) {
		endCol = len(line)
	}
	newLine := make([]rune, len(line)-(endCol-match.Col)+len([]rune(search.ReplaceText)))
	copy(newLine[:match.Col], line[:match.Col])
	copy(newLine[match.Col:], []rune(search.ReplaceText))
	copy(newLine[match.Col+len([]rune(search.ReplaceText)):], line[endCol:])
	buf.Lines[match.Row] = newLine
	buf.Modified = true

	// Record in undo history as individual change
	e.history.Push(&Change{
		Type:    ChangeReplace,
		Buffer:  buf,
		Row:     match.Row,
		Col:     match.Col,
		Text:    copyLines(buf.Lines),
		OldText: oldLines,
	})

	e.scheduleAutoSave()

	// Save current cursor position before recalculating matches
	cursorRow := pane.CursorRow
	cursorCol := pane.CursorCol

	// Recalculate matches after replacement
	search.Matches = buf.FindAllMatches(search.Query)
	search.NoMatches = len(search.Matches) == 0

	// Find next match after current cursor position (not CursorZero)
	if len(search.Matches) > 0 {
		search.CurrentIndex = FindFirstMatchAfterCursor(search.Matches, cursorRow, cursorCol)
		if search.CurrentIndex >= 0 {
			e.navigateToCurrentMatch()
		}
	} else {
		search.CurrentIndex = -1
	}
}

// getCurrentWordPrefix returns the word being typed at cursor position
func (e *Editor) getCurrentWordPrefix() (prefix string, startCol int) {
	pane := e.activePane()
	buf := pane.Buffer
	if pane.CursorCol == 0 {
		return "", 0
	}
	line := buf.Lines[pane.CursorRow]
	if len(line) == 0 {
		return "", 0
	}

	// Ensure cursor is within line bounds
	cursorCol := pane.CursorCol
	if cursorCol > len(line) {
		cursorCol = len(line)
	}

	// Scan backwards from cursor to find word start
	startCol = cursorCol
	for startCol > 0 && !isDelimiter(line[startCol-1]) {
		startCol--
	}

	if startCol >= cursorCol {
		return "", startCol
	}

	prefix = string(line[startCol:cursorCol])
	return prefix, startCol
}

// triggerAutocomplete triggers autocomplete suggestions based on current word prefix
func (e *Editor) triggerAutocomplete() {
	prefix, startCol := e.getCurrentWordPrefix()
	if len(prefix) < 2 {
		e.inputState.Autocomplete = nil
		return
	}

	pane := e.activePane()
	buf := pane.Buffer
	words := ExtractWords(buf.Lines, pane.CursorRow, startCol)
	suggestions := FindSuggestions(words, prefix, 10)

	if len(suggestions) == 0 {
		e.inputState.Autocomplete = nil
		return
	}

	e.inputState.Autocomplete = NewAutocompleteState(prefix, startCol, suggestions)
}

// acceptAutocomplete inserts the selected suggestion
func (e *Editor) acceptAutocomplete() {
	ac := e.inputState.Autocomplete
	if ac == nil || len(ac.Suggestions) == 0 {
		return
	}

	selected := ac.Selected()
	if selected == nil {
		return
	}

	buf := e.activeBuffer()
	line := buf.Lines[buf.CursorRow]

	// Calculate the completion (part of word not yet typed)
	completion := []rune(selected.Word[len(ac.Prefix):])
	if len(completion) == 0 {
		e.inputState.Autocomplete = nil
		return
	}

	// Insert completion at cursor position
	newLine := make([]rune, len(line)+len(completion))
	copy(newLine[:buf.CursorCol], line[:buf.CursorCol])
	copy(newLine[buf.CursorCol:], completion)
	copy(newLine[buf.CursorCol+len(completion):], line[buf.CursorCol:])

	buf.Lines[buf.CursorRow] = newLine
	buf.CursorCol += len(completion)
	buf.Modified = true

	e.inputState.Autocomplete = nil
	e.scheduleAutoSave()
}

// handleMarkInput handles input when waiting for a mark identifier
func (e *Editor) handleMarkInput(ev *tcell.EventKey) bool {
	// Cancel on Escape
	if ev.Key() == tcell.KeyEscape {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	// Only accept rune keys
	if ev.Key() != tcell.KeyRune {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	r := ev.Rune()

	// Validate identifier
	if !isValidMarkIdentifier(r) {
		e.inputState.PendingMark = false
		e.inputState.PendingJumpToMark = false
		return false
	}

	if e.inputState.PendingMark {
		e.setMark(r)
		e.inputState.PendingMark = false
	} else if e.inputState.PendingJumpToMark {
		e.jumpToMark(r)
		e.inputState.PendingJumpToMark = false
	}

	return false
}

// setMark saves the current cursor position as a global mark with buffer reference
func (e *Editor) setMark(id rune) {
	pane := e.activePane()
	e.globalMarks[id] = GlobalMark{
		Buffer: pane.Buffer,
		Row:    pane.CursorRow,
		Col:    pane.CursorCol,
	}
}

// jumpToMark moves cursor to a previously set mark, with cross-file pane switching
func (e *Editor) jumpToMark(id rune) {
	mark, exists := e.globalMarks[id]
	if !exists {
		return // Mark doesn't exist, do nothing
	}

	// Find a pane viewing the mark's buffer
	paneIdx := -1
	for i, p := range e.panes {
		if p.Buffer == mark.Buffer {
			paneIdx = i
			break
		}
	}

	// Buffer no longer open - mark is invalid
	if paneIdx == -1 {
		return
	}

	// Switch pane if mark is in different pane
	if paneIdx != e.activePaneIdx {
		e.activePaneIdx = paneIdx
	}

	// Position cursor with clamping on the target pane
	pane := e.panes[paneIdx]
	buf := pane.Buffer

	// Clamp row to valid range
	row := mark.Row
	if row >= len(buf.Lines) {
		row = len(buf.Lines) - 1
	}
	if row < 0 {
		row = 0
	}

	// Clamp column to line length
	col := mark.Col
	if col > len(buf.Lines[row]) {
		col = len(buf.Lines[row])
	}
	if col < 0 {
		col = 0
	}

	pane.CursorRow = row
	pane.CursorCol = col
}