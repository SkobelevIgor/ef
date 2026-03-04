package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

func TestParseColor(t *testing.T) {
	tests := []struct {
		name  string
		input string
		want  tcell.Color
	}{
		// Named colors
		{"black", "black", tcell.ColorBlack},
		{"red", "red", tcell.ColorRed},
		{"green", "green", tcell.ColorGreen},
		{"yellow", "yellow", tcell.ColorYellow},
		{"blue", "blue", tcell.ColorBlue},
		{"magenta", "magenta", tcell.ColorPurple},
		{"purple", "purple", tcell.ColorPurple},
		{"cyan", "cyan", tcell.ColorAqua},
		{"aqua", "aqua", tcell.ColorAqua},
		{"white", "white", tcell.ColorWhite},
		{"gray", "gray", tcell.ColorGray},
		{"grey", "grey", tcell.ColorGray},
		{"teal", "teal", tcell.ColorTeal},
		{"orange", "orange", tcell.ColorOrange},
		{"pink", "pink", tcell.ColorPink},

		// Case insensitive
		{"RED uppercase", "RED", tcell.ColorRed},
		{"Blue mixed", "Blue", tcell.ColorBlue},

		// Hex with 0x prefix
		{"hex 0xFF0000", "0xFF0000", tcell.NewHexColor(0xFF0000)},
		{"hex 0x00FF00", "0x00FF00", tcell.NewHexColor(0x00FF00)},
		{"hex 0X uppercase prefix", "0X0000FF", tcell.NewHexColor(0x0000FF)},

		// Hex with # prefix
		{"hex #FF0000", "#FF0000", tcell.NewHexColor(0xFF0000)},
		{"hex #00FF00", "#00FF00", tcell.NewHexColor(0x00FF00)},

		// Unknown defaults to ColorDefault
		{"unknown color", "fuchsia", tcell.ColorDefault},
		{"empty string", "", tcell.ColorDefault},

		// Whitespace trimming
		{"whitespace trimmed", "  red  ", tcell.ColorRed},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := parseColor(tt.input)
			if got != tt.want {
				t.Errorf("parseColor(%q) = %v, want %v", tt.input, got, tt.want)
			}
		})
	}
}

func TestParseStyle(t *testing.T) {
	tests := []struct {
		name string
		cfg  StyleConfig
		want tcell.Style
	}{
		{
			name: "color and bold",
			cfg:  StyleConfig{Color: "red", Bold: true},
			want: tcell.StyleDefault.Foreground(tcell.ColorRed).Bold(true),
		},
		{
			name: "color only",
			cfg:  StyleConfig{Color: "blue", Bold: false},
			want: tcell.StyleDefault.Foreground(tcell.ColorBlue),
		},
		{
			name: "bold only",
			cfg:  StyleConfig{Color: "", Bold: true},
			want: tcell.StyleDefault.Bold(true),
		},
		{
			name: "empty config",
			cfg:  StyleConfig{},
			want: tcell.StyleDefault,
		},
		{
			name: "hex color with bold",
			cfg:  StyleConfig{Color: "#00FF00", Bold: true},
			want: tcell.StyleDefault.Foreground(tcell.NewHexColor(0x00FF00)).Bold(true),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := parseStyle(tt.cfg)
			if got != tt.want {
				t.Errorf("parseStyle(%+v) = %v, want %v", tt.cfg, got, tt.want)
			}
		})
	}
}

func TestNewSyntaxHighlighter_TokenizerConfig(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxHighlighting: true,
		Tokenizer: &TokenizerConfig{
			Keywords:    []string{"func", "var"},
			LineComment: "//",
			Strings: []StringDelimConfig{
				{Open: "\"", Close: "\"", Escape: "\\"},
			},
			Brackets: "()[]{}",
			Styles: &TokenStyleMap{
				Keyword:      StyleConfig{Color: "yellow"},
				String:       StyleConfig{Color: "green"},
				Comment:      StyleConfig{Color: "gray"},
				FunctionCall: StyleConfig{Color: "yellow"},
				Bracket:      StyleConfig{Color: "purple"},
				Number:       StyleConfig{Color: "blue", Bold: true},
			},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	// Should return TokenizerHighlighter, not BaseSyntaxHighlighter
	if _, ok := h.(*TokenizerHighlighter); !ok {
		t.Errorf("expected *TokenizerHighlighter, got %T", h)
	}

	// Should highlight keywords
	line := []rune("func main()")
	tokens := h.Highlight(line, 0, [][]rune{line})
	if len(tokens) == 0 {
		t.Fatal("expected tokens from TokenizerHighlighter")
	}
}

func TestNewSyntaxHighlighter_FallbackToBase(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\bfunc\b`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	if _, ok := h.(*BaseSyntaxHighlighter); !ok {
		t.Errorf("expected *BaseSyntaxHighlighter, got %T", h)
	}
}

func TestHighlight_SingleRule(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\b(func|var)\b`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	line := []rune("func main()")
	tokens := h.Highlight(line, 0, [][]rune{line})

	if len(tokens) == 0 {
		t.Fatal("expected at least one token, got none")
	}

	// "func" is at positions 0..3
	found := false
	expectedStyle := tcell.StyleDefault.Foreground(tcell.ColorBlue)
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == expectedStyle {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected token for 'func' at [0,4) with blue style, got tokens: %+v", tokens)
	}
}

func TestHighlight_MultipleRulesWithPriority(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "green"}, Priority: 1},
			{Pattern: `\b(func)\b`, Style: StyleConfig{Color: "red"}, Priority: 10},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	line := []rune("func hello")
	tokens := h.Highlight(line, 0, [][]rune{line})

	if len(tokens) == 0 {
		t.Fatal("expected tokens, got none")
	}

	// "func" should have higher priority red style
	redStyle := tcell.StyleDefault.Foreground(tcell.ColorRed)
	greenStyle := tcell.StyleDefault.Foreground(tcell.ColorGreen)

	funcToken := false
	helloToken := false
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == redStyle {
			funcToken = true
		}
		if tok.Start == 5 && tok.End == 10 && tok.Style == greenStyle {
			helloToken = true
		}
	}
	if !funcToken {
		t.Errorf("expected 'func' token with red (high priority) style, tokens: %+v", tokens)
	}
	if !helloToken {
		t.Errorf("expected 'hello' token with green (low priority) style, tokens: %+v", tokens)
	}
}

func TestHighlight_EmptyLine(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	tokens := h.Highlight([]rune{}, 0, [][]rune{{}})
	if tokens != nil {
		t.Errorf("expected nil tokens for empty line, got %+v", tokens)
	}
}

func TestHighlight_NoRules(t *testing.T) {
	ftConfig := FileTypeConfig{}
	h := NewSyntaxHighlighter(ftConfig)

	line := []rune("hello world")
	tokens := h.Highlight(line, 0, [][]rune{line})
	if tokens != nil {
		t.Errorf("expected nil tokens with no rules, got %+v", tokens)
	}
}

func TestHighlight_InvalidPattern(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `[invalid`, Style: StyleConfig{Color: "blue"}, Priority: 1},
			{Pattern: `\w+`, Style: StyleConfig{Color: "green"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)

	// The invalid pattern should be skipped, valid one should work
	line := []rune("hello")
	tokens := h.Highlight(line, 0, [][]rune{line})
	if len(tokens) == 0 {
		t.Error("expected tokens from valid rule after invalid pattern was skipped")
	}
}

func TestHighlight_Reset(t *testing.T) {
	ftConfig := FileTypeConfig{}
	h := NewSyntaxHighlighter(ftConfig)
	// Reset is a no-op but should not panic
	h.Reset()
}

func TestHighlightCache_Miss(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)
	cache := NewHighlightCache(h)

	line := []rune("hello")
	lines := [][]rune{line}

	tokens := cache.GetTokens(0, line, lines)
	if len(tokens) == 0 {
		t.Error("expected tokens on cache miss, got none")
	}
}

func TestHighlightCache_Hit(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)
	cache := NewHighlightCache(h)

	line := []rune("hello")
	lines := [][]rune{line}

	// First call: cache miss
	tokens1 := cache.GetTokens(0, line, lines)
	// Second call: cache hit (same line, same index)
	tokens2 := cache.GetTokens(0, line, lines)

	if len(tokens1) != len(tokens2) {
		t.Errorf("cache hit returned different length: %d vs %d", len(tokens1), len(tokens2))
	}
	for i := range tokens1 {
		if tokens1[i] != tokens2[i] {
			t.Errorf("cache hit returned different token at %d: %+v vs %+v", i, tokens1[i], tokens2[i])
		}
	}
}

func TestHighlightCache_Invalidate(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)
	cache := NewHighlightCache(h)

	line := []rune("hello")
	lines := [][]rune{line}

	// Populate cache
	cache.GetTokens(0, line, lines)

	// Invalidate
	cache.Invalidate(0)

	// Should recompute (cache miss) and still return tokens
	tokens := cache.GetTokens(0, line, lines)
	if len(tokens) == 0 {
		t.Error("expected tokens after invalidation and recompute")
	}
}

func TestHighlightCache_InvalidateAll(t *testing.T) {
	ftConfig := FileTypeConfig{
		SyntaxRules: []SyntaxRuleConfig{
			{Pattern: `\w+`, Style: StyleConfig{Color: "blue"}, Priority: 1},
		},
	}
	h := NewSyntaxHighlighter(ftConfig)
	cache := NewHighlightCache(h)

	line0 := []rune("hello")
	line1 := []rune("world")
	lines := [][]rune{line0, line1}

	// Populate cache for two lines
	cache.GetTokens(0, line0, lines)
	cache.GetTokens(1, line1, lines)

	// Invalidate all
	cache.InvalidateAll()

	// Both should recompute
	tokens0 := cache.GetTokens(0, line0, lines)
	tokens1 := cache.GetTokens(1, line1, lines)
	if len(tokens0) == 0 || len(tokens1) == 0 {
		t.Error("expected tokens after InvalidateAll and recompute")
	}
}

func TestHighlightCache_NilHighlighter(t *testing.T) {
	cache := NewHighlightCache(nil)

	line := []rune("hello")
	tokens := cache.GetTokens(0, line, [][]rune{line})
	if tokens != nil {
		t.Errorf("expected nil tokens with nil highlighter, got %+v", tokens)
	}
}

func TestHighlightCache_Highlighter(t *testing.T) {
	ftConfig := FileTypeConfig{}
	h := NewSyntaxHighlighter(ftConfig)
	cache := NewHighlightCache(h)

	if cache.Highlighter() != h {
		t.Error("Highlighter() did not return the same highlighter")
	}
}

func TestHashLine_SameInput(t *testing.T) {
	line1 := []rune("hello world")
	line2 := []rune("hello world")

	h1 := hashLine(line1)
	h2 := hashLine(line2)

	if h1 != h2 {
		t.Errorf("same input produced different hashes: %d vs %d", h1, h2)
	}
}

func TestHashLine_DifferentInput(t *testing.T) {
	line1 := []rune("hello")
	line2 := []rune("world")

	h1 := hashLine(line1)
	h2 := hashLine(line2)

	if h1 == h2 {
		t.Errorf("different inputs produced same hash: %d", h1)
	}
}

func TestHashLine_EmptyLine(t *testing.T) {
	h := hashLine([]rune{})
	// Should return the initial seed value
	if h != 5381 {
		t.Errorf("empty line hash = %d, want 5381", h)
	}
}

func TestHighlightCache_InvalidateCascade(t *testing.T) {
	cfg := &TokenizerConfig{
		Keywords:     []string{"func"},
		LineComment:  "//",
		BlockComment: []string{"/*", "*/"},
		Brackets:     "(){}",
		Styles: &TokenStyleMap{
			Keyword: StyleConfig{Color: "yellow"},
			Comment: StyleConfig{Color: "gray"},
		},
	}
	h := NewTokenizerHighlighter(cfg)
	cache := NewHighlightCache(h)

	lines := [][]rune{
		[]rune("func main()"),
		[]rune("// comment"),
		[]rune("var x"),
	}

	// Populate cache
	for i, line := range lines {
		cache.GetTokens(i, line, lines)
	}

	// Invalidate from line 1 — should clear lines >= 1 and reset highlighter
	cache.Invalidate(1)

	// Verify line 0 still cached, line 1 cleared
	// (We can't directly check cache internals, but GetTokens should recompute)
	tokens := cache.GetTokens(1, lines[1], lines)
	if len(tokens) == 0 {
		t.Error("expected tokens after invalidate and recompute")
	}
}

func TestGetStyleAt_Found(t *testing.T) {
	style := tcell.StyleDefault.Foreground(tcell.ColorRed)
	tokens := []Token{
		{Start: 0, End: 5, Style: style},
		{Start: 6, End: 11, Style: tcell.StyleDefault.Foreground(tcell.ColorBlue)},
	}

	got, ok := GetStyleAt(tokens, 2)
	if !ok {
		t.Error("expected ok=true for column within token range")
	}
	if got != style {
		t.Errorf("GetStyleAt(tokens, 2) style = %v, want %v", got, style)
	}
}

func TestGetStyleAt_SecondToken(t *testing.T) {
	blueStyle := tcell.StyleDefault.Foreground(tcell.ColorBlue)
	tokens := []Token{
		{Start: 0, End: 5, Style: tcell.StyleDefault.Foreground(tcell.ColorRed)},
		{Start: 6, End: 11, Style: blueStyle},
	}

	got, ok := GetStyleAt(tokens, 8)
	if !ok {
		t.Error("expected ok=true for column within second token")
	}
	if got != blueStyle {
		t.Errorf("GetStyleAt(tokens, 8) style = %v, want %v", got, blueStyle)
	}
}

func TestGetStyleAt_NotFound(t *testing.T) {
	tokens := []Token{
		{Start: 0, End: 5, Style: tcell.StyleDefault.Foreground(tcell.ColorRed)},
	}

	got, ok := GetStyleAt(tokens, 10)
	if ok {
		t.Error("expected ok=false for column outside any token range")
	}
	if got != tcell.StyleDefault {
		t.Errorf("expected StyleDefault when not found, got %v", got)
	}
}

func TestGetStyleAt_EmptyTokens(t *testing.T) {
	_, ok := GetStyleAt(nil, 0)
	if ok {
		t.Error("expected ok=false for nil tokens")
	}
}

func TestGetStyleAt_BoundaryStart(t *testing.T) {
	style := tcell.StyleDefault.Foreground(tcell.ColorGreen)
	tokens := []Token{
		{Start: 3, End: 7, Style: style},
	}

	got, ok := GetStyleAt(tokens, 3)
	if !ok || got != style {
		t.Errorf("expected style at Start boundary, got ok=%v style=%v", ok, got)
	}
}

func TestGetStyleAt_BoundaryEnd(t *testing.T) {
	tokens := []Token{
		{Start: 3, End: 7, Style: tcell.StyleDefault.Foreground(tcell.ColorGreen)},
	}

	// End is exclusive, so col=7 should not match
	_, ok := GetStyleAt(tokens, 7)
	if ok {
		t.Error("expected ok=false at End boundary (exclusive)")
	}
}
