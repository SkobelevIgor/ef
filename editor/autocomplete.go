package editor

import (
	"sort"
	"strings"
	"unicode"
)

// Suggestion represents a single autocomplete candidate
type Suggestion struct {
	Word  string // The complete word
	Score int    // Relevance score (higher = better match)
}

// AutocompleteState represents an active autocomplete session
type AutocompleteState struct {
	Active      bool         // Whether autocomplete dropdown is visible
	Prefix      string       // The typed characters triggering completion
	PrefixCol   int          // Column position where prefix starts
	Suggestions []Suggestion // Matching words, sorted by relevance
	SelectedIdx int          // Currently highlighted suggestion (0-based)
	ShowAbove   bool         // True if dropdown appears above cursor

	// Word cache: avoids re-scanning the entire buffer on every keystroke
	cachedWords    []string // Cached word list from ExtractWords
	cachedModCount uint64   // Buffer ModCount when cache was built
}

// Tokenizer delimiters for word extraction
const delimiters = "()[]{}.,;:!?<>=+-*/%&|^~\"'` \t\n\r"

// isDelimiter checks if a rune is a word delimiter
func isDelimiter(r rune) bool {
	return strings.ContainsRune(delimiters, r) || unicode.IsSpace(r)
}

// ExtractWords returns all unique words from buffer lines
// excludeRow and excludeCol specify the position of the word being typed (to exclude it)
func ExtractWords(lines [][]rune, excludeRow, excludeCol int) []string {
	seen := make(map[string]struct{})
	var words []string

	for rowIdx, line := range lines {
		wordStart := -1
		for colIdx, r := range line {
			if isDelimiter(r) {
				if wordStart >= 0 {
					word := string(line[wordStart:colIdx])
					// Skip the word being typed (at excludeRow, starting at excludeCol)
					if !(rowIdx == excludeRow && wordStart == excludeCol) {
						if _, exists := seen[word]; !exists && len(word) >= 2 {
							seen[word] = struct{}{}
							words = append(words, word)
						}
					}
					wordStart = -1
				}
			} else {
				if wordStart < 0 {
					wordStart = colIdx
				}
			}
		}
		// Handle word at end of line
		if wordStart >= 0 {
			word := string(line[wordStart:])
			if !(rowIdx == excludeRow && wordStart == excludeCol) {
				if _, exists := seen[word]; !exists && len(word) >= 2 {
					seen[word] = struct{}{}
					words = append(words, word)
				}
			}
		}
	}

	return words
}

// MatchSubsequence checks if query chars appear in order within word (case-insensitive)
func MatchSubsequence(word, query string) bool {
	wordLower := strings.ToLower(word)
	queryLower := strings.ToLower(query)

	wordIdx := 0
	for _, qChar := range queryLower {
		found := false
		for wordIdx < len(wordLower) {
			wChar := rune(wordLower[wordIdx])
			wordIdx++
			if wChar == qChar {
				found = true
				break
			}
		}
		if !found {
			return false
		}
	}
	return true
}

// ScoreSuggestion calculates relevance score for a match
// Higher score = better match
// Prefix matches get +1000, shorter words are preferred
func ScoreSuggestion(word, query string) int {
	score := 0
	wordLower := strings.ToLower(word)
	queryLower := strings.ToLower(query)

	// Prefix match bonus
	if strings.HasPrefix(wordLower, queryLower) {
		score += 1000
	}

	// Length penalty (shorter words preferred)
	score -= len(word)

	return score
}

// FindSuggestions returns sorted, limited suggestions for a prefix
func FindSuggestions(words []string, prefix string, maxResults int) []Suggestion {
	var suggestions []Suggestion

	for _, word := range words {
		if MatchSubsequence(word, prefix) {
			suggestions = append(suggestions, Suggestion{
				Word:  word,
				Score: ScoreSuggestion(word, prefix),
			})
		}
	}

	// Sort by score (descending), then alphabetically
	sort.SliceStable(suggestions, func(i, j int) bool {
		if suggestions[i].Score != suggestions[j].Score {
			return suggestions[i].Score > suggestions[j].Score
		}
		return strings.ToLower(suggestions[i].Word) < strings.ToLower(suggestions[j].Word)
	})

	// Limit results
	if len(suggestions) > maxResults {
		suggestions = suggestions[:maxResults]
	}

	return suggestions
}

// NewAutocompleteState creates state for a new autocomplete session
func NewAutocompleteState(prefix string, prefixCol int, suggestions []Suggestion) *AutocompleteState {
	return &AutocompleteState{
		Active:      true,
		Prefix:      prefix,
		PrefixCol:   prefixCol,
		Suggestions: suggestions,
		SelectedIdx: 0,
	}
}

// Selected returns the currently highlighted suggestion
func (a *AutocompleteState) Selected() *Suggestion {
	if a == nil || len(a.Suggestions) == 0 {
		return nil
	}
	if a.SelectedIdx < 0 || a.SelectedIdx >= len(a.Suggestions) {
		return nil
	}
	return &a.Suggestions[a.SelectedIdx]
}

// Next moves selection to next suggestion (wrapping)
func (a *AutocompleteState) Next() {
	if a == nil || len(a.Suggestions) == 0 {
		return
	}
	a.SelectedIdx = (a.SelectedIdx + 1) % len(a.Suggestions)
}

// Prev moves selection to previous suggestion (wrapping)
func (a *AutocompleteState) Prev() {
	if a == nil || len(a.Suggestions) == 0 {
		return
	}
	a.SelectedIdx--
	if a.SelectedIdx < 0 {
		a.SelectedIdx = len(a.Suggestions) - 1
	}
}

// UpdateSuggestions refreshes suggestions for a new prefix
func (a *AutocompleteState) UpdateSuggestions(prefix string, suggestions []Suggestion) {
	a.Prefix = prefix
	a.Suggestions = suggestions
	a.SelectedIdx = 0
}

// GetWords returns the cached word list, rebuilding it only when the buffer has changed.
func (a *AutocompleteState) GetWords(lines [][]rune, modCount uint64, excludeRow, excludeCol int) []string {
	if a.cachedWords != nil && a.cachedModCount == modCount {
		return a.cachedWords
	}
	a.cachedWords = ExtractWords(lines, excludeRow, excludeCol)
	a.cachedModCount = modCount
	return a.cachedWords
}
