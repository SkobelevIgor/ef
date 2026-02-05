package editor

// ChangeType represents the type of change
type ChangeType int

const (
	ChangeInsert ChangeType = iota
	ChangeDelete
	ChangeReplace
)

// Change represents a single undoable change
type Change struct {
	Type         ChangeType
	Row          int
	Col          int
	Text         [][]rune // Text that was inserted (for undo of insert) or deleted (for redo of delete)
	OldText      [][]rune // Previous text (for replacements)
	LineDeletion bool     // True if this is a whole-line deletion (dd command)
}

// History manages undo/redo stacks
type History struct {
	undoStack []*Change
	redoStack []*Change
	maxSize   int
	pending   *Change // Accumulates changes during Insert mode session
	inSession bool    // Whether we're in an Insert mode session

	// Session tracking for insert mode
	sessionStartRow int
	sessionStartCol int
	sessionLines    [][]rune // Snapshot of lines at session start
}

// NewHistory creates a new history manager
func NewHistory(maxSize int) *History {
	if maxSize <= 0 {
		maxSize = 100
	}
	return &History{
		maxSize: maxSize,
	}
}

// Push adds a change to the undo stack
func (h *History) Push(c *Change) {
	if c == nil {
		return
	}

	// Clear redo stack on new changes
	h.redoStack = nil

	// If in session, merge with pending change
	if h.inSession && h.pending != nil {
		// For simplicity, just replace pending with new change
		// A more sophisticated implementation would merge them
		h.pending = c
		return
	}

	h.undoStack = append(h.undoStack, c)

	// Trim to max size
	if len(h.undoStack) > h.maxSize {
		h.undoStack = h.undoStack[len(h.undoStack)-h.maxSize:]
	}
}

// Undo returns the last change to undo (or nil if empty)
func (h *History) Undo() *Change {
	if len(h.undoStack) == 0 {
		return nil
	}

	// Pop from undo stack
	c := h.undoStack[len(h.undoStack)-1]
	h.undoStack = h.undoStack[:len(h.undoStack)-1]

	// Push to redo stack
	h.redoStack = append(h.redoStack, c)

	return c
}

// Redo returns the last undone change to redo (or nil if empty)
func (h *History) Redo() *Change {
	if len(h.redoStack) == 0 {
		return nil
	}

	// Pop from redo stack
	c := h.redoStack[len(h.redoStack)-1]
	h.redoStack = h.redoStack[:len(h.redoStack)-1]

	// Push back to undo stack
	h.undoStack = append(h.undoStack, c)

	return c
}

// StartSession begins a new Insert mode session
// All changes during this session will be grouped as one undo unit
func (h *History) StartSession(row, col int, lines [][]rune) {
	h.inSession = true
	h.pending = nil
	h.sessionStartRow = row
	h.sessionStartCol = col
	// Take a snapshot of the lines
	h.sessionLines = copyLines(lines)
}

// CommitSession ends the Insert mode session and commits pending changes
// currentLines is the current state of the buffer after editing
func (h *History) CommitSession(currentLines [][]rune) {
	if h.inSession && h.sessionLines != nil {
		// Compare snapshots to determine what changed
		// For simplicity, we record this as a replacement of the entire affected region
		oldLineCount := len(h.sessionLines)
		newLineCount := len(currentLines)

		// Check if anything actually changed
		changed := oldLineCount != newLineCount
		if !changed {
			for i := 0; i < oldLineCount; i++ {
				if string(h.sessionLines[i]) != string(currentLines[i]) {
					changed = true
					break
				}
			}
		}

		if changed {
			// Record as: delete old content, insert new content
			// We store the old lines so undo can restore them
			h.redoStack = nil
			h.undoStack = append(h.undoStack, &Change{
				Type:    ChangeReplace,
				Row:     h.sessionStartRow,
				Col:     h.sessionStartCol,
				Text:    copyLines(currentLines), // New state (for redo)
				OldText: h.sessionLines,          // Old state (for undo)
			})
		}
	}
	h.inSession = false
	h.sessionLines = nil
}

// RecordInsert records an insert operation
func (h *History) RecordInsert(row, col int, text [][]rune) {
	h.Push(&Change{
		Type: ChangeInsert,
		Row:  row,
		Col:  col,
		Text: copyLines(text),
	})
}

// RecordDelete records a delete operation
func (h *History) RecordDelete(row, col int, text [][]rune) {
	h.Push(&Change{
		Type: ChangeDelete,
		Row:  row,
		Col:  col,
		Text: copyLines(text),
	})
}

// RecordDeleteLines records a whole-line deletion (dd command)
func (h *History) RecordDeleteLines(row int, lines [][]rune) {
	h.Push(&Change{
		Type:         ChangeDelete,
		Row:          row,
		Col:          0,
		Text:         copyLines(lines),
		LineDeletion: true,
	})
}

// copyLines creates a deep copy of a slice of rune slices
func copyLines(lines [][]rune) [][]rune {
	if lines == nil {
		return nil
	}
	result := make([][]rune, len(lines))
	for i, line := range lines {
		result[i] = make([]rune, len(line))
		copy(result[i], line)
	}
	return result
}

// CanUndo returns true if there are changes to undo
func (h *History) CanUndo() bool {
	return len(h.undoStack) > 0
}

// CanRedo returns true if there are changes to redo
func (h *History) CanRedo() bool {
	return len(h.redoStack) > 0
}
