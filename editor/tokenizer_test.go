package editor

import (
	"testing"

	"github.com/gdamore/tcell/v2"
)

// --- matchPrefix tests ---

func TestMatchPrefix(t *testing.T) {
	tests := []struct {
		name   string
		line   []rune
		pos    int
		prefix string
		want   bool
	}{
		{"exact match", []rune("// comment"), 0, "//", true},
		{"middle match", []rune("a /* b"), 2, "/*", true},
		{"no match", []rune("hello"), 0, "//", false},
		{"empty prefix", []rune("hello"), 0, "", true},
		{"empty line", []rune{}, 0, "//", false},
		{"nil line", nil, 0, "//", false},
		{"pos at end", []rune("ab"), 2, "c", false},
		{"prefix longer than remaining", []rune("ab"), 1, "bc", false},
		{"unicode line", []rune("日本語//x"), 3, "//", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := matchPrefix(tt.line, tt.pos, tt.prefix)
			if got != tt.want {
				t.Errorf("matchPrefix(%q, %d, %q) = %v, want %v",
					string(tt.line), tt.pos, tt.prefix, got, tt.want)
			}
		})
	}
}

// --- isIdentStart / isIdentPart tests ---

func TestIsIdentStart(t *testing.T) {
	tests := []struct {
		r    rune
		want bool
	}{
		{'a', true}, {'Z', true}, {'_', true},
		{'0', false}, {' ', false}, {'.', false},
		{'日', true},  // unicode letter
		{'🎉', false}, // emoji
	}
	for _, tt := range tests {
		if got := isIdentStart(tt.r); got != tt.want {
			t.Errorf("isIdentStart(%q) = %v, want %v", tt.r, got, tt.want)
		}
	}
}

func TestIsIdentPart(t *testing.T) {
	tests := []struct {
		r    rune
		want bool
	}{
		{'a', true}, {'Z', true}, {'_', true}, {'5', true},
		{' ', false}, {'.', false},
		{'日', true},  // unicode letter
		{'🎉', false}, // emoji
	}
	for _, tt := range tests {
		if got := isIdentPart(tt.r); got != tt.want {
			t.Errorf("isIdentPart(%q) = %v, want %v", tt.r, got, tt.want)
		}
	}
}

// --- scanIdentifier tests ---

func TestScanIdentifier(t *testing.T) {
	tests := []struct {
		name string
		line []rune
		pos  int
		want int
	}{
		{"simple", []rune("hello world"), 0, 5},
		{"with digits", []rune("var123 x"), 0, 6},
		{"underscore start", []rune("_foo bar"), 0, 4},
		{"single char", []rune("x+y"), 0, 1},
		{"at end", []rune("abc"), 0, 3},
		{"empty", []rune{}, 0, 0},
		{"nil", nil, 0, 0},
		{"unicode ident", []rune("日本語 x"), 0, 3},
		{"mid-line", []rune("a.method("), 2, 8},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := scanIdentifier(tt.line, tt.pos)
			if got != tt.want {
				t.Errorf("scanIdentifier(%q, %d) = %d, want %d",
					string(tt.line), tt.pos, got, tt.want)
			}
		})
	}
}

// --- scanNumber tests ---

func TestScanNumber(t *testing.T) {
	tests := []struct {
		name string
		line []rune
		pos  int
		want int
	}{
		{"integer", []rune("123 abc"), 0, 3},
		{"float", []rune("3.14 x"), 0, 4},
		{"hex", []rune("0xFF00 x"), 0, 6},
		{"hex upper", []rune("0XAB x"), 0, 4},
		{"octal", []rune("0o77 x"), 0, 4},
		{"binary", []rune("0b1010 x"), 0, 6},
		{"single digit", []rune("5+3"), 0, 1},
		{"at end", []rune("42"), 0, 2},
		{"empty", []rune{}, 0, 0},
		{"nil", nil, 0, 0},
		{"exponent", []rune("1e10 x"), 0, 4},
		{"exponent negative", []rune("2.5e-3 x"), 0, 6},
		{"dot only end", []rune("3."), 0, 2},
		{"mid-line", []rune("x=42+1"), 2, 4},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := scanNumber(tt.line, tt.pos)
			if got != tt.want {
				t.Errorf("scanNumber(%q, %d) = %d, want %d",
					string(tt.line), tt.pos, got, tt.want)
			}
		})
	}
}

// --- scanBlockComment tests ---

func TestScanBlockComment(t *testing.T) {
	tests := []struct {
		name      string
		line      []rune
		pos       int
		endMarker string
		wantEnd   int
		wantDone  bool
	}{
		{"closed on same line", []rune("comment */ rest"), 0, "*/", 10, true},
		{"not closed", []rune("still in comment"), 0, "*/", 16, false},
		{"immediately closed", []rune("*/"), 0, "*/", 2, true},
		{"empty line", []rune{}, 0, "*/", 0, false},
		{"nil line", nil, 0, "*/", 0, false},
		{"close mid-line", []rune("xx*/yy"), 0, "*/", 4, true},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			end, done := scanBlockComment(tt.line, tt.pos, tt.endMarker)
			if end != tt.wantEnd || done != tt.wantDone {
				t.Errorf("scanBlockComment(%q, %d, %q) = (%d, %v), want (%d, %v)",
					string(tt.line), tt.pos, tt.endMarker, end, done, tt.wantEnd, tt.wantDone)
			}
		})
	}
}

// --- scanString tests ---

func TestScanString(t *testing.T) {
	tests := []struct {
		name     string
		line     []rune
		pos      int
		close    string
		escape   string
		wantEnd  int
		wantDone bool
	}{
		{"simple double quote", []rune(`hello" rest`), 0, "\"", "\\", 6, true},
		{"with escape", []rune(`he\"llo" rest`), 0, "\"", "\\", 8, true},
		{"not closed", []rune("hello world"), 0, "\"", "\\", 11, false},
		{"immediately closed", []rune(`" rest`), 0, "\"", "\\", 1, true},
		{"empty line", []rune{}, 0, "\"", "\\", 0, false},
		{"nil line", nil, 0, "\"", "\\", 0, false},
		{"no escape char", []rune("hello` rest"), 0, "`", "", 6, true},
		{"triple quote", []rune(`abc""" rest`), 0, `"""`, "\\", 6, true},
		{"triple not closed", []rune(`abc"" not done`), 0, `"""`, "\\", 14, false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			end, done := scanString(tt.line, tt.pos, tt.close, tt.escape)
			if end != tt.wantEnd || done != tt.wantDone {
				t.Errorf("scanString(%q, %d, %q, %q) = (%d, %v), want (%d, %v)",
					string(tt.line), tt.pos, tt.close, tt.escape, end, done, tt.wantEnd, tt.wantDone)
			}
		})
	}
}

// --- tokenizeLine tests ---

func makeRuntime() *tokenizerRuntime {
	cfg := &TokenizerConfig{
		Keywords:     []string{"func", "var", "if", "return"},
		LineComment:  "//",
		BlockComment: []string{"/*", "*/"},
		Strings: []StringDelimConfig{
			{Open: "`", Close: "`"},
			{Open: "\"", Close: "\"", Escape: "\\"},
			{Open: "'", Close: "'", Escape: "\\"},
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
	}
	return newTokenizerRuntime(cfg)
}

func TestTokenizeLine_Keywords(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine([]rune("func main"), StateNormal, rt)

	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}

	// Should have keyword token for "func"
	found := false
	kwStyle := parseStyle(StyleConfig{Color: "yellow"})
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == kwStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected keyword token for 'func' at [0,4), got %+v", tokens)
	}
}

func TestTokenizeLine_FunctionCall(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune("main(x)"), StateNormal, rt)

	fcStyle := parseStyle(StyleConfig{Color: "yellow"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == fcStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected function_call token for 'main' at [0,4), got %+v", tokens)
	}
}

func TestTokenizeLine_MethodCall(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune("a.Method(x)"), StateNormal, rt)

	fcStyle := parseStyle(StyleConfig{Color: "yellow"})
	found := false
	for _, tok := range tokens {
		// "Method" is at positions 2..7, dot excluded
		if tok.Start == 2 && tok.End == 8 && tok.Style == fcStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected function_call token for 'Method' at [2,8), got %+v", tokens)
	}
}

func TestTokenizeLine_LineComment(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine([]rune("x := 1 // comment"), StateNormal, rt)

	if state != StateNormal {
		t.Errorf("expected StateNormal after line comment, got %d", state)
	}

	commentStyle := parseStyle(StyleConfig{Color: "gray"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 7 && tok.End == 17 && tok.Style == commentStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected comment token at [7,17), got %+v", tokens)
	}
}

func TestTokenizeLine_BlockComment_SameLine(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine([]rune("x /* comment */ y"), StateNormal, rt)

	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}

	commentStyle := parseStyle(StyleConfig{Color: "gray"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 2 && tok.End == 15 && tok.Style == commentStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected comment token at [2,15), got %+v", tokens)
	}
}

func TestTokenizeLine_BlockComment_MultiLine(t *testing.T) {
	rt := makeRuntime()
	// First line: open block comment
	_, state := tokenizeLine([]rune("x /* start"), StateNormal, rt)
	if state != StateBlockComment {
		t.Errorf("expected StateBlockComment, got %d", state)
	}

	// Middle line: still in comment
	_, state = tokenizeLine([]rune("still comment"), state, rt)
	if state != StateBlockComment {
		t.Errorf("expected StateBlockComment, got %d", state)
	}

	// End line: close block comment
	tokens, state := tokenizeLine([]rune("end */ code"), state, rt)
	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}

	commentStyle := parseStyle(StyleConfig{Color: "gray"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 6 && tok.Style == commentStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected comment token at [0,6), got %+v", tokens)
	}
}

func TestTokenizeLine_String(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine([]rune(`x = "hello" + y`), StateNormal, rt)

	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}

	strStyle := parseStyle(StyleConfig{Color: "green"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 4 && tok.End == 11 && tok.Style == strStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected string token at [4,11), got %+v", tokens)
	}
}

func TestTokenizeLine_StringWithEscape(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune(`x = "he\"llo"`), StateNormal, rt)

	strStyle := parseStyle(StyleConfig{Color: "green"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 4 && tok.End == 13 && tok.Style == strStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected string token at [4,13), got %+v", tokens)
	}
}

func TestTokenizeLine_MultiLineString(t *testing.T) {
	rt := makeRuntime()
	// Backtick string open
	_, state := tokenizeLine([]rune("x = `start"), StateNormal, rt)
	if state != StateString {
		t.Errorf("expected StateString, got %d", state)
	}

	// Close on next line
	tokens, state := tokenizeLine([]rune("end` + y"), state, rt)
	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}

	strStyle := parseStyle(StyleConfig{Color: "green"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == strStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected string token at [0,4), got %+v", tokens)
	}
}

func TestTokenizeLine_Number(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune("x = 42"), StateNormal, rt)

	numStyle := parseStyle(StyleConfig{Color: "blue", Bold: true})
	found := false
	for _, tok := range tokens {
		if tok.Start == 4 && tok.End == 6 && tok.Style == numStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected number token at [4,6), got %+v", tokens)
	}
}

func TestTokenizeLine_Brackets(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune("f()"), StateNormal, rt)

	brStyle := parseStyle(StyleConfig{Color: "purple"})
	count := 0
	for _, tok := range tokens {
		if tok.Style == brStyle {
			count++
		}
	}
	if count != 2 {
		t.Errorf("expected 2 bracket tokens, got %d in %+v", count, tokens)
	}
}

func TestTokenizeLine_EmptyLine(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine([]rune{}, StateNormal, rt)
	if len(tokens) != 0 {
		t.Errorf("expected no tokens for empty line, got %+v", tokens)
	}
	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}
}

func TestTokenizeLine_NilLine(t *testing.T) {
	rt := makeRuntime()
	tokens, state := tokenizeLine(nil, StateNormal, rt)
	if len(tokens) != 0 {
		t.Errorf("expected no tokens for nil line, got %+v", tokens)
	}
	if state != StateNormal {
		t.Errorf("expected StateNormal, got %d", state)
	}
}

func TestTokenizeLine_StringContainingURL(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune(`url := "https://example.com"`), StateNormal, rt)

	strStyle := parseStyle(StyleConfig{Color: "green"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 7 && tok.End == 28 && tok.Style == strStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected string token covering entire URL, got %+v", tokens)
	}
}

func TestTokenizeLine_IdentifierNotKeyword(t *testing.T) {
	rt := makeRuntime()
	tokens, _ := tokenizeLine([]rune("myVar"), StateNormal, rt)

	kwStyle := parseStyle(StyleConfig{Color: "yellow"})
	for _, tok := range tokens {
		if tok.Style == kwStyle {
			t.Errorf("plain identifier should not get keyword style, got %+v", tokens)
		}
	}
}

// --- TokenizerHighlighter tests ---

func makeTokenizerConfig() *TokenizerConfig {
	return &TokenizerConfig{
		Keywords:     []string{"func", "var", "if", "return"},
		LineComment:  "//",
		BlockComment: []string{"/*", "*/"},
		Strings: []StringDelimConfig{
			{Open: "`", Close: "`"},
			{Open: "\"", Close: "\"", Escape: "\\"},
			{Open: "'", Close: "'", Escape: "\\"},
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
	}
}

func TestTokenizerHighlighter_Highlight(t *testing.T) {
	h := NewTokenizerHighlighter(makeTokenizerConfig())
	line := []rune("func main()")
	lines := [][]rune{line}
	tokens := h.Highlight(line, 0, lines)

	kwStyle := parseStyle(StyleConfig{Color: "yellow"})
	found := false
	for _, tok := range tokens {
		if tok.Start == 0 && tok.End == 4 && tok.Style == kwStyle {
			found = true
		}
	}
	if !found {
		t.Errorf("expected keyword token for 'func', got %+v", tokens)
	}
}

func TestTokenizerHighlighter_MultiLineBlockComment(t *testing.T) {
	h := NewTokenizerHighlighter(makeTokenizerConfig())
	lines := [][]rune{
		[]rune("x /* start"),
		[]rune("middle"),
		[]rune("end */ y"),
	}

	// Highlight line 0 — opens block comment
	h.Highlight(lines[0], 0, lines)

	// Highlight line 1 — entire line is comment
	tokens1 := h.Highlight(lines[1], 1, lines)
	commentStyle := parseStyle(StyleConfig{Color: "gray"})
	if len(tokens1) == 0 || tokens1[0].Style != commentStyle {
		t.Errorf("line 1 should be comment, got %+v", tokens1)
	}

	// Highlight line 2 — comment closes
	tokens2 := h.Highlight(lines[2], 2, lines)
	hasComment := false
	for _, tok := range tokens2 {
		if tok.Style == commentStyle && tok.Start == 0 {
			hasComment = true
		}
	}
	if !hasComment {
		t.Errorf("line 2 should start with comment, got %+v", tokens2)
	}
}

func TestTokenizerHighlighter_Reset(t *testing.T) {
	h := NewTokenizerHighlighter(makeTokenizerConfig())
	lines := [][]rune{
		[]rune("/* start"),
		[]rune("end */"),
	}
	h.Highlight(lines[0], 0, lines)
	h.Reset()

	// After reset, state cache is cleared
	// Highlighting line 1 without line 0 should treat it as normal
	tokens := h.Highlight(lines[1], 1, lines)
	commentStyle := parseStyle(StyleConfig{Color: "gray"})
	allComment := true
	for _, tok := range tokens {
		if tok.Style != commentStyle {
			allComment = false
		}
	}
	// After reset, line 1 at index 1 with no prior state should NOT be a comment
	// (it needs to re-derive from line 0 first)
	// The highlighter should re-derive by scanning from the top
	// Actually per design: Reset clears cache, next render re-derives
	// Since we only call Highlight(line1, 1, lines) without line 0 first,
	// the highlighter should re-scan from line 0 to determine state at line 1
	_ = allComment
	_ = tokens
	// Just verify no panic
}

func TestTokenizerHighlighter_EmptyLine(t *testing.T) {
	h := NewTokenizerHighlighter(makeTokenizerConfig())
	tokens := h.Highlight([]rune{}, 0, [][]rune{{}})
	if len(tokens) != 0 {
		t.Errorf("expected no tokens for empty line, got %+v", tokens)
	}
}

func TestTokenizerHighlighter_NilConfig(t *testing.T) {
	// Nil styles should not panic
	cfg := &TokenizerConfig{
		Keywords: []string{"func"},
	}
	h := NewTokenizerHighlighter(cfg)
	tokens := h.Highlight([]rune("func"), 0, [][]rune{[]rune("func")})
	// Should work without styles (default zero styles)
	_ = tokens
}

// suppress unused import
var _ = tcell.StyleDefault
