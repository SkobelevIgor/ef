package editor

import "time"

// Timing constants
const (
	// autoSaveDelay is the delay before auto-saving after edits
	autoSaveDelay = 200 * time.Millisecond

	// DoubleShiftTimeout is how quickly two shifts must be pressed for pane switching
	DoubleShiftTimeout = 400 * time.Millisecond

	// MappingTimeout is the timeout for multi-character key mappings
	MappingTimeout = 500 * time.Millisecond
)

// Layout constants
const (
	// ScrollMargin is the number of lines to keep visible above/below cursor
	ScrollMargin = 5

	// MinPaneHeightHorizontal is the minimum height for horizontal splits
	MinPaneHeightHorizontal = 3

	// MinPaneWidthVertical is the minimum width for vertical splits
	MinPaneWidthVertical = 20
)

// Defaults
const (
	// DefaultTabStop is the default tab width when not configured
	DefaultTabStop = 4

	// DefaultHistorySize is the default undo history capacity
	DefaultHistorySize = 100
)

