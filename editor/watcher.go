package editor

import (
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/fsnotify/fsnotify"
)

// FileChangedEvent is a custom tcell event for file changes
type FileChangedEvent struct {
	when     time.Time
	Filename string
}

// When returns the time when the event was created
func (e *FileChangedEvent) When() time.Time {
	return e.when
}

// FileWatcher watches files for external changes
type FileWatcher struct {
	watcher    *fsnotify.Watcher
	screen     *Screen
	files      map[string]time.Time // absolute path -> last known mod time
	mu         sync.Mutex
	debounce   map[string]*time.Timer
	debounceMu sync.Mutex
}

// NewFileWatcher creates a new file watcher
func NewFileWatcher(screen *Screen) (*FileWatcher, error) {
	w, err := fsnotify.NewWatcher()
	if err != nil {
		return nil, err
	}

	fw := &FileWatcher{
		watcher:  w,
		screen:   screen,
		files:    make(map[string]time.Time),
		debounce: make(map[string]*time.Timer),
	}

	go fw.watchLoop()

	return fw, nil
}

// Close stops the file watcher
func (fw *FileWatcher) Close() error {
	return fw.watcher.Close()
}

// Watch adds a file to be watched
func (fw *FileWatcher) Watch(filename string) error {
	// Convert to absolute path for consistent matching
	absPath, err := filepath.Abs(filename)
	if err != nil {
		absPath = filename
	}

	fw.mu.Lock()
	// Record current mod time
	if info, err := os.Stat(absPath); err == nil {
		fw.files[absPath] = info.ModTime()
	} else {
		// File might not exist yet, record zero time
		fw.files[absPath] = time.Time{}
	}
	fw.mu.Unlock()

	// Watch the directory containing the file (more reliable on macOS)
	dir := filepath.Dir(absPath)
	return fw.watcher.Add(dir)
}

// UpdateModTime updates the recorded modification time for a file
func (fw *FileWatcher) UpdateModTime(filename string) {
	absPath, err := filepath.Abs(filename)
	if err != nil {
		absPath = filename
	}

	fw.mu.Lock()
	defer fw.mu.Unlock()

	if info, err := os.Stat(absPath); err == nil {
		fw.files[absPath] = info.ModTime()
	}
}

// watchLoop handles fsnotify events
func (fw *FileWatcher) watchLoop() {
	for {
		select {
		case event, ok := <-fw.watcher.Events:
			if !ok {
				return
			}
			// Handle Write, Create, and Rename events (atomic saves use rename)
			if event.Op&(fsnotify.Write|fsnotify.Create|fsnotify.Rename) != 0 {
				fw.handleFileEvent(event.Name)
			}
		case _, ok := <-fw.watcher.Errors:
			if !ok {
				return
			}
		}
	}
}

// handleFileEvent handles a file change event with debouncing
func (fw *FileWatcher) handleFileEvent(eventPath string) {
	// Convert to absolute path
	absPath, err := filepath.Abs(eventPath)
	if err != nil {
		absPath = eventPath
	}

	// Check if this is a file we're watching
	fw.mu.Lock()
	_, watching := fw.files[absPath]
	fw.mu.Unlock()

	if !watching {
		return
	}

	fw.debounceMu.Lock()
	// Cancel existing timer if any
	if timer, exists := fw.debounce[absPath]; exists {
		timer.Stop()
	}

	// Create new debounced timer
	fw.debounce[absPath] = time.AfterFunc(200*time.Millisecond, func() {
		// Check if file was actually modified externally
		info, err := os.Stat(absPath)
		if err != nil {
			return
		}

		// Get the current lastKnown time inside the callback, not before the debounce.
		// This ensures that if UpdateModTime was called (e.g., by auto-save) between
		// when the event fired and when this callback runs, we use the updated time.
		fw.mu.Lock()
		currentLastKnown := fw.files[absPath]
		isExternal := info.ModTime().After(currentLastKnown)
		fw.mu.Unlock()

		if isExternal {
			fw.screen.PostEvent(&FileChangedEvent{
				when:     time.Now(),
				Filename: absPath,
			})
		}
	})
	fw.debounceMu.Unlock()
}
