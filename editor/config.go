package editor

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/gdamore/tcell/v2"
)

// Config holds the editor configuration loaded from ~/.efconfig
type Config struct {
	FileTypes map[string]FileTypeConfig `json:"file_types"`
	Maps      map[string]string         `json:"maps"`
}

// StyleConfig defines text styling in config
type StyleConfig struct {
	Color string `json:"color"` // hex color like "0x0000FF" or named color like "blue"
	Bold  bool   `json:"bold"`
}

// SyntaxRuleConfig defines a syntax highlighting rule in config
type SyntaxRuleConfig struct {
	Pattern  string      `json:"pattern"`
	Style    StyleConfig `json:"style"`
	Priority int         `json:"priority"`
}

// FileTypeConfig holds per-filetype configuration
type FileTypeConfig struct {
	Extensions         []string           `json:"extensions"`
	SyntaxHighlighting bool               `json:"syntax_highlighting"`
	TabStop            int                `json:"tabstop"`
	ShiftWidth         int                `json:"shiftwidth"`
	AutoIndentation    bool               `json:"autoindentation"`
	ExpandTab          bool               `json:"expandtab"`
	SyntaxRules        []SyntaxRuleConfig `json:"syntax_rules,omitempty"`
}

// MappingTimeout is the timeout for multi-character key mappings
const MappingTimeout = 500 * time.Millisecond

// LoadConfig loads configuration from ~/.efconfig
// Returns empty config if file doesn't exist
func LoadConfig() *Config {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return DefaultConfig()
	}

	configPath := filepath.Join(homeDir, ".efconfig")
	data, err := os.ReadFile(configPath)
	if err != nil {
		return DefaultConfig()
	}

	config := DefaultConfig()
	if err := json.Unmarshal(data, config); err != nil {
		return DefaultConfig()
	}

	return config
}

// DefaultConfig returns minimal default configuration
// All file type settings should be defined in ~/.efconfig
func DefaultConfig() *Config {
	return &Config{
		FileTypes: map[string]FileTypeConfig{},
		Maps:      map[string]string{},
	}
}

// SpecialKey represents a parsed special key
type SpecialKey struct {
	Key  tcell.Key
	Rune rune
	Mod  tcell.ModMask
}

// ParseSpecialKeys converts a string with special key notation to a slice of SpecialKeys
// Supports: <Esc>, <Enter>, <Tab>, <BS>, <Space>, <Left>, <Right>, <Up>, <Down>
func ParseSpecialKeys(s string) []SpecialKey {
	var keys []SpecialKey
	i := 0

	for i < len(s) {
		if s[i] == '<' {
			// Find closing >
			end := strings.Index(s[i:], ">")
			if end != -1 {
				special := strings.ToLower(s[i+1 : i+end])
				if key, ok := parseSpecialKeyName(special); ok {
					keys = append(keys, key)
					i += end + 1
					continue
				}
			}
		}

		// Regular character
		keys = append(keys, SpecialKey{
			Key:  tcell.KeyRune,
			Rune: rune(s[i]),
			Mod:  tcell.ModNone,
		})
		i++
	}

	return keys
}

// parseSpecialKeyName converts a special key name to a SpecialKey
func parseSpecialKeyName(name string) (SpecialKey, bool) {
	switch name {
	case "esc", "escape":
		return SpecialKey{Key: tcell.KeyEscape}, true
	case "enter", "cr", "return":
		return SpecialKey{Key: tcell.KeyEnter}, true
	case "tab":
		return SpecialKey{Key: tcell.KeyTab}, true
	case "bs", "backspace":
		return SpecialKey{Key: tcell.KeyBackspace2}, true
	case "space":
		return SpecialKey{Key: tcell.KeyRune, Rune: ' '}, true
	case "left":
		return SpecialKey{Key: tcell.KeyLeft}, true
	case "right":
		return SpecialKey{Key: tcell.KeyRight}, true
	case "up":
		return SpecialKey{Key: tcell.KeyUp}, true
	case "down":
		return SpecialKey{Key: tcell.KeyDown}, true
	case "del", "delete":
		return SpecialKey{Key: tcell.KeyDelete}, true
	case "home":
		return SpecialKey{Key: tcell.KeyHome}, true
	case "end":
		return SpecialKey{Key: tcell.KeyEnd}, true
	default:
		return SpecialKey{}, false
	}
}

// GetFileTypeConfig returns the config for a given filetype name
func (c *Config) GetFileTypeConfig(ftName string) FileTypeConfig {
	if cfg, ok := c.FileTypes[ftName]; ok {
		return cfg
	}
	return c.FileTypes["plain"]
}
