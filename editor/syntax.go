package editor

import (
	"regexp"
	"strconv"
	"strings"
	"sync"

	"github.com/gdamore/tcell/v2"
)

// Token represents a syntax-highlighted region in a line
type Token struct {
	Start int         // Start column (inclusive)
	End   int         // End column (exclusive)
	Style tcell.Style // Style to apply
}

// SyntaxRule defines a regex pattern for syntax highlighting
type SyntaxRule struct {
	Pattern  *regexp.Regexp
	Style    tcell.Style
	Priority int
}

// SyntaxHighlighter interface for highlighting source code
type SyntaxHighlighter interface {
	// Highlight returns tokens for a single line
	Highlight(line []rune, lineIndex int, lines [][]rune) []Token
	// Reset clears any state (e.g., multi-line comment tracking)
	Reset()
}

// BaseSyntaxHighlighter provides regex-based syntax highlighting
type BaseSyntaxHighlighter struct {
	Rules []SyntaxRule
	mu    sync.RWMutex
}

// NewSyntaxHighlighter creates a syntax highlighter for the given file type config
func NewSyntaxHighlighter(ftConfig FileTypeConfig) SyntaxHighlighter {
	h := &BaseSyntaxHighlighter{}

	// Load rules from config
	for _, rule := range ftConfig.SyntaxRules {
		h.addRule(rule.Pattern, parseStyle(rule.Style), rule.Priority)
	}

	return h
}

// parseStyle converts a StyleConfig to tcell.Style
func parseStyle(cfg StyleConfig) tcell.Style {
	style := tcell.StyleDefault

	if cfg.Color != "" {
		color := parseColor(cfg.Color)
		style = style.Foreground(color)
	}

	if cfg.Bold {
		style = style.Bold(true)
	}

	return style
}

// parseColor converts a color string to tcell.Color
// Supports hex colors (0xFF0000, #FF0000) and named colors (red, blue, etc.)
func parseColor(s string) tcell.Color {
	s = strings.TrimSpace(s)

	// Handle hex colors
	if strings.HasPrefix(s, "0x") || strings.HasPrefix(s, "0X") {
		if val, err := strconv.ParseInt(s[2:], 16, 32); err == nil {
			return tcell.NewHexColor(int32(val))
		}
	}
	if strings.HasPrefix(s, "#") {
		if val, err := strconv.ParseInt(s[1:], 16, 32); err == nil {
			return tcell.NewHexColor(int32(val))
		}
	}

	// Handle named colors
	switch strings.ToLower(s) {
	case "black":
		return tcell.ColorBlack
	case "red":
		return tcell.ColorRed
	case "green":
		return tcell.ColorGreen
	case "yellow":
		return tcell.ColorYellow
	case "blue":
		return tcell.ColorBlue
	case "magenta", "purple":
		return tcell.ColorPurple
	case "cyan", "aqua":
		return tcell.ColorAqua
	case "white":
		return tcell.ColorWhite
	case "gray", "grey":
		return tcell.ColorGray
	case "teal":
		return tcell.ColorTeal
	case "orange":
		return tcell.ColorOrange
	case "pink":
		return tcell.ColorPink
	default:
		return tcell.ColorDefault
	}
}

// addRule adds a syntax rule
func (h *BaseSyntaxHighlighter) addRule(pattern string, style tcell.Style, priority int) {
	re, err := regexp.Compile(pattern)
	if err != nil {
		return // Skip invalid patterns
	}
	h.Rules = append(h.Rules, SyntaxRule{
		Pattern:  re,
		Style:    style,
		Priority: priority,
	})
}

// Highlight returns tokens for a line
func (h *BaseSyntaxHighlighter) Highlight(line []rune, lineIndex int, lines [][]rune) []Token {
	if len(line) == 0 || len(h.Rules) == 0 {
		return nil
	}

	lineStr := string(line)

	// Track which rule applies to each character position
	ruleIndices := make([]int, len(line))  // -1 means no rule
	priorities := make([]int, len(line))

	// Initialize to -1 (no rule)
	for i := range ruleIndices {
		ruleIndices[i] = -1
	}

	for ruleIdx, rule := range h.Rules {
		matches := rule.Pattern.FindAllStringIndex(lineStr, -1)
		for _, match := range matches {
			// Convert byte indices to rune indices
			start := len([]rune(lineStr[:match[0]]))
			end := len([]rune(lineStr[:match[1]]))

			for i := start; i < end && i < len(line); i++ {
				if rule.Priority > priorities[i] {
					ruleIndices[i] = ruleIdx
					priorities[i] = rule.Priority
				}
			}
		}
	}

	// Convert to Token slice, merging adjacent same-rule tokens
	var tokens []Token
	if len(ruleIndices) > 0 {
		currentRule := ruleIndices[0]
		start := 0
		for i := 1; i < len(ruleIndices); i++ {
			if ruleIndices[i] != currentRule {
				if currentRule >= 0 {
					tokens = append(tokens, Token{
						Start: start,
						End:   i,
						Style: h.Rules[currentRule].Style,
					})
				}
				currentRule = ruleIndices[i]
				start = i
			}
		}
		// Don't forget the last token
		if currentRule >= 0 {
			tokens = append(tokens, Token{
				Start: start,
				End:   len(ruleIndices),
				Style: h.Rules[currentRule].Style,
			})
		}
	}

	return tokens
}

// Reset clears any highlighting state
func (h *BaseSyntaxHighlighter) Reset() {
	// No state to reset in the base implementation
}

// HighlightCache caches highlighted tokens per line for performance
type HighlightCache struct {
	cache       map[int][]Token
	lineHashes  map[int]uint64
	highlighter SyntaxHighlighter
	mu          sync.RWMutex
}

// NewHighlightCache creates a new highlight cache
func NewHighlightCache(highlighter SyntaxHighlighter) *HighlightCache {
	return &HighlightCache{
		cache:       make(map[int][]Token),
		lineHashes:  make(map[int]uint64),
		highlighter: highlighter,
	}
}

// GetTokens returns tokens for a line, using cache if available
func (c *HighlightCache) GetTokens(lineIdx int, line []rune, lines [][]rune) []Token {
	if c.highlighter == nil {
		return nil
	}

	hash := hashLine(line)

	c.mu.RLock()
	if cachedHash, ok := c.lineHashes[lineIdx]; ok && cachedHash == hash {
		tokens := c.cache[lineIdx]
		c.mu.RUnlock()
		return tokens
	}
	c.mu.RUnlock()

	// Cache miss - compute tokens
	tokens := c.highlighter.Highlight(line, lineIdx, lines)

	c.mu.Lock()
	c.cache[lineIdx] = tokens
	c.lineHashes[lineIdx] = hash
	c.mu.Unlock()

	return tokens
}

// Invalidate clears the cache for a specific line
func (c *HighlightCache) Invalidate(lineIdx int) {
	c.mu.Lock()
	delete(c.cache, lineIdx)
	delete(c.lineHashes, lineIdx)
	c.mu.Unlock()
}

// InvalidateAll clears the entire cache
func (c *HighlightCache) InvalidateAll() {
	c.mu.Lock()
	c.cache = make(map[int][]Token)
	c.lineHashes = make(map[int]uint64)
	c.mu.Unlock()
}

// Highlighter returns the associated syntax highlighter
func (c *HighlightCache) Highlighter() SyntaxHighlighter {
	return c.highlighter
}

// hashLine computes a simple hash of a line for cache invalidation
func hashLine(line []rune) uint64 {
	var hash uint64 = 5381
	for _, r := range line {
		hash = ((hash << 5) + hash) + uint64(r)
	}
	return hash
}

// GetStyleAt returns the style at a specific column position
func GetStyleAt(tokens []Token, col int) (tcell.Style, bool) {
	for _, tok := range tokens {
		if col >= tok.Start && col < tok.End {
			return tok.Style, true
		}
	}
	return tcell.StyleDefault, false
}
