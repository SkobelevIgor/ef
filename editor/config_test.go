package editor

import (
	"encoding/json"
	"testing"

	"github.com/gdamore/tcell/v2"
)

func TestDefaultConfig(t *testing.T) {
	cfg := DefaultConfig()
	if cfg.FileTypes == nil {
		t.Error("FileTypes should not be nil")
	}
	if cfg.Maps == nil {
		t.Error("Maps should not be nil")
	}
	if len(cfg.FileTypes) != 0 {
		t.Errorf("FileTypes should be empty, got %d", len(cfg.FileTypes))
	}
}

func TestGetFileTypeConfig(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go":    {TabStop: 4, Extensions: []string{".go"}},
			"plain": {TabStop: 8},
		},
		Maps: map[string]string{},
	}

	t.Run("known type", func(t *testing.T) {
		got := cfg.GetFileTypeConfig("go")
		if got.TabStop != 4 {
			t.Errorf("TabStop = %d, want 4", got.TabStop)
		}
	})

	t.Run("unknown falls back to plain", func(t *testing.T) {
		got := cfg.GetFileTypeConfig("unknown")
		if got.TabStop != 8 {
			t.Errorf("TabStop = %d, want 8 (plain fallback)", got.TabStop)
		}
	})
}

func TestGetKeyMappings(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{},
		Maps:      map[string]string{"jk": "<Esc>"},
	}
	m := cfg.GetKeyMappings()
	if m["jk"] != "<Esc>" {
		t.Errorf("got %q", m["jk"])
	}
}

func TestGetMaps(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{},
		Maps:      map[string]string{"jk": "<Esc>"},
	}
	m := cfg.GetMaps()
	if m["jk"] != "<Esc>" {
		t.Errorf("got %q", m["jk"])
	}
}

func TestParseSpecialKeys(t *testing.T) {
	t.Run("escape", func(t *testing.T) {
		keys := ParseSpecialKeys("<Esc>")
		if len(keys) != 1 || keys[0].Key != tcell.KeyEscape {
			t.Errorf("expected KeyEscape, got %v", keys)
		}
	})

	t.Run("plain chars", func(t *testing.T) {
		keys := ParseSpecialKeys("abc")
		if len(keys) != 3 {
			t.Fatalf("expected 3 keys, got %d", len(keys))
		}
		if keys[0].Rune != 'a' || keys[1].Rune != 'b' || keys[2].Rune != 'c' {
			t.Error("wrong runes")
		}
	})

	t.Run("mixed", func(t *testing.T) {
		keys := ParseSpecialKeys("<Enter>x<Tab>")
		if len(keys) != 3 {
			t.Fatalf("expected 3 keys, got %d", len(keys))
		}
		if keys[0].Key != tcell.KeyEnter {
			t.Error("first should be Enter")
		}
		if keys[1].Rune != 'x' {
			t.Error("second should be 'x'")
		}
		if keys[2].Key != tcell.KeyTab {
			t.Error("third should be Tab")
		}
	})
}

func TestTokenizerConfigUnmarshal(t *testing.T) {
	jsonData := `{
		"file_types": {
			"go": {
				"extensions": [".go"],
				"syntax_highlighting": true,
				"tokenizer": {
					"keywords": ["func", "var", "const"],
					"line_comment": "//",
					"block_comment": ["/*", "*/"],
					"strings": [
						{"open": "\"", "close": "\"", "escape": "\\"},
						{"open": "'", "close": "'", "escape": "\\"},
						{"open": "` + "`" + `", "close": "` + "`" + `"}
					],
					"brackets": "()[]{}",
					"styles": {
						"keyword":       {"color": "yellow"},
						"string":        {"color": "green"},
						"comment":       {"color": "gray"},
						"function_call": {"color": "yellow"},
						"bracket":       {"color": "purple"},
						"number":        {"color": "blue", "bold": true}
					}
				}
			}
		}
	}`

	cfg := DefaultConfig()
	if err := json.Unmarshal([]byte(jsonData), cfg); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}

	ftCfg := cfg.FileTypes["go"]
	if ftCfg.Tokenizer == nil {
		t.Fatal("expected Tokenizer to be non-nil")
	}

	tok := ftCfg.Tokenizer
	if len(tok.Keywords) != 3 {
		t.Errorf("keywords count = %d, want 3", len(tok.Keywords))
	}
	if tok.LineComment != "//" {
		t.Errorf("line_comment = %q, want //", tok.LineComment)
	}
	if len(tok.BlockComment) != 2 || tok.BlockComment[0] != "/*" || tok.BlockComment[1] != "*/" {
		t.Errorf("block_comment = %v, want [/* */]", tok.BlockComment)
	}
	if len(tok.Strings) != 3 {
		t.Errorf("strings count = %d, want 3", len(tok.Strings))
	}
	if tok.Strings[0].Open != "\"" || tok.Strings[0].Close != "\"" || tok.Strings[0].Escape != "\\" {
		t.Errorf("strings[0] = %+v", tok.Strings[0])
	}
	if tok.Strings[2].Escape != "" {
		t.Errorf("strings[2].escape should be empty, got %q", tok.Strings[2].Escape)
	}
	if tok.Brackets != "()[]{}" {
		t.Errorf("brackets = %q, want ()[]{}", tok.Brackets)
	}
	if tok.Styles == nil {
		t.Fatal("styles should not be nil")
	}
	if tok.Styles.Keyword.Color != "yellow" {
		t.Errorf("keyword color = %q, want yellow", tok.Styles.Keyword.Color)
	}
	if !tok.Styles.Number.Bold {
		t.Error("number should be bold")
	}
}

func TestTokenizerConfigBackwardCompat(t *testing.T) {
	jsonData := `{
		"file_types": {
			"go": {
				"extensions": [".go"],
				"syntax_highlighting": true,
				"syntax_rules": [
					{"pattern": "\\bfunc\\b", "style": {"color": "blue"}, "priority": 1}
				]
			}
		}
	}`

	cfg := DefaultConfig()
	if err := json.Unmarshal([]byte(jsonData), cfg); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}

	ftCfg := cfg.FileTypes["go"]
	if ftCfg.Tokenizer != nil {
		t.Error("expected Tokenizer to be nil when not present")
	}
	if len(ftCfg.SyntaxRules) != 1 {
		t.Errorf("syntax_rules count = %d, want 1", len(ftCfg.SyntaxRules))
	}
}

func TestParseSpecialKeyName(t *testing.T) {
	tests := []struct {
		name string
		want tcell.Key
		ok   bool
	}{
		{"esc", tcell.KeyEscape, true},
		{"escape", tcell.KeyEscape, true},
		{"enter", tcell.KeyEnter, true},
		{"cr", tcell.KeyEnter, true},
		{"tab", tcell.KeyTab, true},
		{"bs", tcell.KeyBackspace2, true},
		{"left", tcell.KeyLeft, true},
		{"right", tcell.KeyRight, true},
		{"up", tcell.KeyUp, true},
		{"down", tcell.KeyDown, true},
		{"del", tcell.KeyDelete, true},
		{"home", tcell.KeyHome, true},
		{"end", tcell.KeyEnd, true},
		{"unknown", 0, false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			key, ok := parseSpecialKeyName(tt.name)
			if ok != tt.ok {
				t.Errorf("ok = %v, want %v", ok, tt.ok)
			}
			if ok && key.Key != tt.want {
				t.Errorf("key = %v, want %v", key.Key, tt.want)
			}
		})
	}
}
