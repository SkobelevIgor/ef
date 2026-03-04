package editor

import (
	"unicode"

	"github.com/gdamore/tcell/v2"
)

// LineState tracks multi-line tokenizer state
type LineState uint8

const (
	StateNormal       LineState = 0
	StateBlockComment LineState = 1
	// StateString + i for string delimiter index i
	StateString LineState = 2
)

// matchPrefix checks if line[pos:] starts with prefix
func matchPrefix(line []rune, pos int, prefix string) bool {
	pr := []rune(prefix)
	if pos+len(pr) > len(line) {
		return false
	}
	for i, r := range pr {
		if line[pos+i] != r {
			return false
		}
	}
	return true
}

// isIdentStart returns true if r can start an identifier
func isIdentStart(r rune) bool {
	return unicode.IsLetter(r) || r == '_'
}

// isIdentPart returns true if r can continue an identifier
func isIdentPart(r rune) bool {
	return isIdentStart(r) || unicode.IsDigit(r)
}

// scanIdentifier returns the end position of an identifier starting at pos
func scanIdentifier(line []rune, pos int) int {
	i := pos
	for i < len(line) && isIdentPart(line[i]) {
		i++
	}
	return i
}

// scanNumber returns the end position of a number literal starting at pos
func scanNumber(line []rune, pos int) int {
	if pos >= len(line) {
		return pos
	}
	i := pos
	// Check hex/octal/binary prefixes
	if line[i] == '0' && i+1 < len(line) {
		switch line[i+1] {
		case 'x', 'X':
			return scanHexDigits(line, i+2)
		case 'o', 'O':
			return scanOctalDigits(line, i+2)
		case 'b', 'B':
			return scanBinaryDigits(line, i+2)
		}
	}
	return scanDecimal(line, i)
}

// scanDecimal scans a decimal/float number with optional exponent
func scanDecimal(line []rune, pos int) int {
	i := pos
	for i < len(line) && unicode.IsDigit(line[i]) {
		i++
	}
	if i < len(line) && line[i] == '.' {
		i++
		for i < len(line) && unicode.IsDigit(line[i]) {
			i++
		}
	}
	if i < len(line) && (line[i] == 'e' || line[i] == 'E') {
		i++
		if i < len(line) && (line[i] == '+' || line[i] == '-') {
			i++
		}
		for i < len(line) && unicode.IsDigit(line[i]) {
			i++
		}
	}
	return i
}

// scanHexDigits scans hex digits after 0x prefix
func scanHexDigits(line []rune, pos int) int {
	i := pos
	for i < len(line) && isHexDigit(line[i]) {
		i++
	}
	return i
}

func isHexDigit(r rune) bool {
	return unicode.IsDigit(r) ||
		(r >= 'a' && r <= 'f') ||
		(r >= 'A' && r <= 'F')
}

// scanOctalDigits scans octal digits after 0o prefix
func scanOctalDigits(line []rune, pos int) int {
	i := pos
	for i < len(line) && line[i] >= '0' && line[i] <= '7' {
		i++
	}
	return i
}

// scanBinaryDigits scans binary digits after 0b prefix
func scanBinaryDigits(line []rune, pos int) int {
	i := pos
	for i < len(line) && (line[i] == '0' || line[i] == '1') {
		i++
	}
	return i
}

// scanBlockComment scans from pos looking for endMarker.
// Returns (endPos, closed). endPos is past the end marker if closed.
func scanBlockComment(line []rune, pos int, endMarker string) (int, bool) {
	i := pos
	for i < len(line) {
		if matchPrefix(line, i, endMarker) {
			return i + len([]rune(endMarker)), true
		}
		i++
	}
	return i, false
}

// scanString scans from pos looking for close delimiter.
// Returns (endPos, closed). endPos is past the close delimiter if closed.
func scanString(line []rune, pos int, close string, escape string) (int, bool) {
	i := pos
	closeRunes := []rune(close)
	for i < len(line) {
		if escape != "" && matchPrefix(line, i, escape) {
			i += len([]rune(escape)) + 1
			continue
		}
		if matchPrefix(line, i, close) {
			return i + len(closeRunes), true
		}
		i++
	}
	return i, false
}

// tokenType identifies the kind of token
type tokenType int

const (
	tokenKeyword tokenType = iota
	tokenString
	tokenComment
	tokenFunctionCall
	tokenBracket
	tokenNumber
	tokenNone
)

// tokenizerRuntime is the pre-processed config for fast tokenizing
type tokenizerRuntime struct {
	keywordMap   map[string]bool
	lineComment  string
	blockStart   string
	blockEnd     string
	strings      []StringDelimConfig
	bracketSet   map[rune]bool
	styles       [6]tcell.Style // indexed by tokenType
}

// newTokenizerRuntime builds a runtime from config
func newTokenizerRuntime(cfg *TokenizerConfig) *tokenizerRuntime {
	rt := &tokenizerRuntime{
		keywordMap:  make(map[string]bool, len(cfg.Keywords)),
		lineComment: cfg.LineComment,
		bracketSet:  make(map[rune]bool),
		strings:     cfg.Strings,
	}
	for _, kw := range cfg.Keywords {
		rt.keywordMap[kw] = true
	}
	if len(cfg.BlockComment) == 2 {
		rt.blockStart = cfg.BlockComment[0]
		rt.blockEnd = cfg.BlockComment[1]
	}
	for _, r := range cfg.Brackets {
		rt.bracketSet[r] = true
	}
	if cfg.Styles != nil {
		rt.styles = buildStyleArray(cfg.Styles)
	}
	return rt
}

func buildStyleArray(m *TokenStyleMap) [6]tcell.Style {
	return [6]tcell.Style{
		tokenKeyword:      parseStyle(m.Keyword),
		tokenString:       parseStyle(m.String),
		tokenComment:      parseStyle(m.Comment),
		tokenFunctionCall: parseStyle(m.FunctionCall),
		tokenBracket:      parseStyle(m.Bracket),
		tokenNumber:       parseStyle(m.Number),
	}
}

// tokenizeLine performs a single-pass tokenization of a line
func tokenizeLine(
	line []rune, state LineState, cfg *tokenizerRuntime,
) ([]Token, LineState) {
	if len(line) == 0 {
		return nil, state
	}
	var tokens []Token
	pos := 0
	if state != StateNormal {
		pos, state, tokens = resumeMultiLine(line, state, cfg)
	}
	for pos < len(line) {
		pos, state, tokens = stepNormal(
			line, pos, state, cfg, tokens,
		)
	}
	return tokens, state
}

// stepNormal processes one token at line[pos] in normal state
func stepNormal(
	line []rune, pos int, state LineState,
	cfg *tokenizerRuntime, tokens []Token,
) (int, LineState, []Token) {
	tt, end, newState := classifyToken(line, pos, cfg)
	if tt == tokenNone {
		return end, state, tokens
	}
	tokens = append(tokens, Token{
		Start: pos, End: end, Style: cfg.styles[tt],
	})
	return end, newState, tokens
}

// resumeMultiLine handles continuation of block comment or string
func resumeMultiLine(
	line []rune, state LineState, cfg *tokenizerRuntime,
) (int, LineState, []Token) {
	var tokens []Token
	if state == StateBlockComment {
		end, closed := scanBlockComment(line, 0, cfg.blockEnd)
		tokens = append(tokens, Token{
			Start: 0, End: end, Style: cfg.styles[tokenComment],
		})
		if !closed {
			return end, StateBlockComment, tokens
		}
		return end, StateNormal, tokens
	}
	// String continuation
	idx := int(state - StateString)
	if idx >= 0 && idx < len(cfg.strings) {
		sd := cfg.strings[idx]
		end, closed := scanString(line, 0, sd.Close, sd.Escape)
		tokens = append(tokens, Token{
			Start: 0, End: end, Style: cfg.styles[tokenString],
		})
		if !closed {
			return end, state, tokens
		}
		return end, StateNormal, tokens
	}
	return 0, StateNormal, tokens
}

// classifyToken identifies what starts at line[pos].
// Returns (type, endPos, newState). endPos negative = unclosed multiline.
func classifyToken(
	line []rune, pos int, cfg *tokenizerRuntime,
) (tokenType, int, LineState) {
	// Line comment
	if cfg.lineComment != "" && matchPrefix(line, pos, cfg.lineComment) {
		return tokenComment, len(line), StateNormal
	}
	// Block comment start
	if cfg.blockStart != "" && matchPrefix(line, pos, cfg.blockStart) {
		return classifyBlockComment(line, pos, cfg)
	}
	// String delimiters
	if tt, end, st, ok := classifyString(line, pos, cfg); ok {
		return tt, end, st
	}
	// Number
	if unicode.IsDigit(line[pos]) {
		return tokenNumber, scanNumber(line, pos), StateNormal
	}
	// Identifier / keyword / function call
	if isIdentStart(line[pos]) {
		tt, end := classifyIdent(line, pos, cfg)
		return tt, end, StateNormal
	}
	// Bracket
	if cfg.bracketSet[line[pos]] {
		return tokenBracket, pos + 1, StateNormal
	}
	return tokenNone, pos + 1, StateNormal
}

func classifyBlockComment(
	line []rune, pos int, cfg *tokenizerRuntime,
) (tokenType, int, LineState) {
	startLen := len([]rune(cfg.blockStart))
	end, closed := scanBlockComment(line, pos+startLen, cfg.blockEnd)
	if !closed {
		return tokenComment, end, StateBlockComment
	}
	return tokenComment, end, StateNormal
}

func classifyString(
	line []rune, pos int, cfg *tokenizerRuntime,
) (tokenType, int, LineState, bool) {
	for i, sd := range cfg.strings {
		if !matchPrefix(line, pos, sd.Open) {
			continue
		}
		openLen := len([]rune(sd.Open))
		end, closed := scanString(line, pos+openLen, sd.Close, sd.Escape)
		if !closed {
			return tokenString, end, StateString + LineState(i), true
		}
		return tokenString, end, StateNormal, true
	}
	return tokenNone, 0, StateNormal, false
}

func classifyIdent(
	line []rune, pos int, cfg *tokenizerRuntime,
) (tokenType, int) {
	end := scanIdentifier(line, pos)
	word := string(line[pos:end])
	// Check if followed by '(' → function call
	if end < len(line) && line[end] == '(' {
		return tokenFunctionCall, end
	}
	if cfg.keywordMap[word] {
		return tokenKeyword, end
	}
	return tokenNone, end
}

// TokenizerHighlighter implements SyntaxHighlighter using single-pass tokenizer
type TokenizerHighlighter struct {
	config     *tokenizerRuntime
	stateCache []LineState
}

// NewTokenizerHighlighter creates a new tokenizer-based highlighter
func NewTokenizerHighlighter(cfg *TokenizerConfig) *TokenizerHighlighter {
	return &TokenizerHighlighter{
		config: newTokenizerRuntime(cfg),
	}
}

// Highlight returns tokens for a single line
func (h *TokenizerHighlighter) Highlight(
	line []rune, lineIndex int, lines [][]rune,
) []Token {
	state := h.getEntryState(lineIndex, lines)
	tokens, exitState := tokenizeLine(line, state, h.config)
	h.setExitState(lineIndex, exitState)
	return tokens
}

// getEntryState returns the entry state for a line
func (h *TokenizerHighlighter) getEntryState(
	lineIndex int, lines [][]rune,
) LineState {
	if lineIndex < len(h.stateCache) {
		return h.stateCache[lineIndex]
	}
	// Need to derive state by scanning prior lines
	startIdx := len(h.stateCache)
	state := StateNormal
	if startIdx > 0 {
		state = h.stateCache[startIdx-1]
	}
	for i := startIdx; i <= lineIndex && i < len(lines); i++ {
		_, exitState := tokenizeLine(lines[i], state, h.config)
		h.growStateCache(i + 1)
		h.stateCache[i] = state
		state = exitState
	}
	return state
}

// setExitState stores the exit state for a line
func (h *TokenizerHighlighter) setExitState(
	lineIndex int, state LineState,
) {
	nextIdx := lineIndex + 1
	h.growStateCache(nextIdx)
	if nextIdx < len(h.stateCache) {
		h.stateCache[nextIdx] = state
	} else {
		h.stateCache = append(h.stateCache, state)
	}
}

// growStateCache ensures stateCache has at least size entries
func (h *TokenizerHighlighter) growStateCache(size int) {
	for len(h.stateCache) < size {
		h.stateCache = append(h.stateCache, StateNormal)
	}
}

// Reset clears the state cache
func (h *TokenizerHighlighter) Reset() {
	h.stateCache = nil
}

