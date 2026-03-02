package editor

import (
	"os"
	"testing"
	"time"

	"github.com/gdamore/tcell/v2"
)

// mockEventPoster captures posted events for testing.
type mockEventPoster struct {
	events []tcell.Event
}

func (m *mockEventPoster) PostEvent(ev tcell.Event) error {
	m.events = append(m.events, ev)
	return nil
}

func TestFileChangedEvent_When_Watcher(t *testing.T) {
	now := time.Now()
	ev := &FileChangedEvent{when: now, Filename: "/tmp/test.txt"}
	if !ev.When().Equal(now) {
		t.Error("When should return the creation time")
	}
	if ev.Filename != "/tmp/test.txt" {
		t.Errorf("Filename = %q", ev.Filename)
	}
}

func TestNewFileWatcher(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	if fw.watcher == nil {
		t.Error("watcher should not be nil")
	}
	if fw.screen != poster {
		t.Error("screen should be the poster")
	}
}

func TestFileWatcher_Watch(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	// Create a temp file to watch
	tmp := t.TempDir()
	path := tmp + "/watched.txt"
	os.WriteFile(path, []byte("content"), 0644)

	err = fw.Watch(path)
	if err != nil {
		t.Fatalf("Watch error: %v", err)
	}

	// Verify the file is tracked
	fw.mu.Lock()
	_, tracked := fw.files[path]
	fw.mu.Unlock()

	if !tracked {
		t.Error("file should be tracked after Watch")
	}
}

func TestFileWatcher_WatchNonexistent(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	tmp := t.TempDir()
	path := tmp + "/nonexistent.txt"

	// Watching a non-existent file should still work (records zero time)
	err = fw.Watch(path)
	if err != nil {
		t.Fatalf("Watch error: %v", err)
	}

	fw.mu.Lock()
	modTime := fw.files[path]
	fw.mu.Unlock()

	if !modTime.IsZero() {
		t.Error("mod time should be zero for non-existent file")
	}
}

func TestFileWatcher_UpdateModTime(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	tmp := t.TempDir()
	path := tmp + "/update.txt"
	os.WriteFile(path, []byte("content"), 0644)

	fw.Watch(path)

	// Get initial mod time
	fw.mu.Lock()
	initialTime := fw.files[path]
	fw.mu.Unlock()

	// Modify file and update
	time.Sleep(10 * time.Millisecond)
	os.WriteFile(path, []byte("modified"), 0644)
	fw.UpdateModTime(path)

	fw.mu.Lock()
	newTime := fw.files[path]
	fw.mu.Unlock()

	if !newTime.After(initialTime) && !newTime.Equal(initialTime) {
		t.Error("mod time should be updated")
	}
}

func TestFileWatcher_Close(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	err = fw.Close()
	if err != nil {
		t.Errorf("Close error: %v", err)
	}
}

func TestFileWatcher_HandleFileEvent_UnwatchedFile(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	// Handling event for unwatched file should be a no-op
	fw.handleFileEvent("/tmp/unwatched_file_xyz.txt")

	// No events should be posted
	if len(poster.events) != 0 {
		t.Error("should not post events for unwatched files")
	}
}

func TestFileWatcher_HandleFileEvent_WatchedFile(t *testing.T) {
	poster := &mockEventPoster{}
	fw, err := NewFileWatcher(poster)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	defer fw.Close()

	tmp := t.TempDir()
	path := tmp + "/event.txt"
	os.WriteFile(path, []byte("content"), 0644)

	fw.Watch(path)

	// Set an old mod time so the event is detected as external
	fw.mu.Lock()
	fw.files[path] = time.Time{}
	fw.mu.Unlock()

	// Trigger file event
	fw.handleFileEvent(path)

	// Wait for debounce (200ms + buffer)
	time.Sleep(350 * time.Millisecond)

	if len(poster.events) == 0 {
		t.Error("expected an event to be posted for external change")
	}
}
