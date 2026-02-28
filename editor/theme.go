package editor

import "github.com/gdamore/tcell/v2"

// Theme styles used across the rendering layer

var (
	// Line number gutter
	LineNumStyle     = tcell.StyleDefault.Foreground(tcell.ColorGreen)
	WrapIndicStyle   = tcell.StyleDefault.Foreground(tcell.ColorDarkGray)
	SelectionStyle   = tcell.StyleDefault.Reverse(true)
	SearchMatchStyle = tcell.StyleDefault.Background(tcell.ColorYellow).Foreground(tcell.ColorBlack)
	CurrentMatchStyle = tcell.StyleDefault.Background(tcell.ColorOrange).Foreground(tcell.ColorBlack).Bold(true)

	// Mode-specific current line number styles
	NormalLineNumStyle = tcell.StyleDefault.Background(tcell.ColorBlue).Foreground(tcell.ColorWhite).Bold(true)
	InsertLineNumStyle = tcell.StyleDefault.Background(tcell.ColorGreen).Foreground(tcell.ColorBlack).Bold(true)
	VisualLineNumStyle = tcell.StyleDefault.Background(tcell.ColorPurple).Foreground(tcell.ColorWhite).Bold(true)

	// Search bar
	SearchBarStyle         = tcell.StyleDefault.Background(tcell.ColorDarkBlue).Foreground(tcell.ColorWhite)
	SearchBarNoMatchStyle  = tcell.StyleDefault.Background(tcell.ColorDarkBlue).Foreground(tcell.ColorRed)

	// Autocomplete dropdown
	AutocompleteNormalStyle   = tcell.StyleDefault.Background(tcell.ColorPurple).Foreground(tcell.ColorBlack)
	AutocompleteSelectedStyle = tcell.StyleDefault.Background(tcell.ColorBlack).Foreground(tcell.ColorWhite).Bold(true)

	// Pane separators
	SeparatorStyle = tcell.StyleDefault.Foreground(tcell.ColorGray)
)

// CurrentLineNumStyle returns the line number style for the given editor mode.
func CurrentLineNumStyle(mode Mode) tcell.Style {
	switch mode {
	case ModeInsert:
		return InsertLineNumStyle
	case ModeVisual:
		return VisualLineNumStyle
	default:
		return NormalLineNumStyle
	}
}
