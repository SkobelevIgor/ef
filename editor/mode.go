package editor

// Mode represents the current editing mode
type Mode int

const (
	ModeNormal Mode = iota
	ModeInsert
	ModeVisual
)

// String returns the mode name for display
func (m Mode) String() string {
	switch m {
	case ModeNormal:
		return "NORMAL"
	case ModeInsert:
		return "INSERT"
	case ModeVisual:
		return "VISUAL"
	default:
		return "UNKNOWN"
	}
}
