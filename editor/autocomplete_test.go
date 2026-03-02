package editor

import (
	"testing"
)

func TestIsDelimiter(t *testing.T) {
	tests := []struct {
		r    rune
		want bool
	}{
		{'(', true},
		{')', true},
		{'{', true},
		{'}', true},
		{'.', true},
		{',', true},
		{' ', true},
		{'\t', true},
		{'\n', true},
		{'a', false},
		{'Z', false},
		{'0', false},
		{'_', false},
	}

	for _, tt := range tests {
		got := isDelimiter(tt.r)
		if got != tt.want {
			t.Errorf("isDelimiter(%q) = %v, want %v", tt.r, got, tt.want)
		}
	}
}

func TestExtractWords(t *testing.T) {
	tests := []struct {
		name       string
		lines      [][]rune
		excludeRow int
		excludeCol int
		wantLen    int
		wantWords  []string // subset that must be present
		wantAbsent []string // words that must NOT be present
	}{
		{
			name:       "simple words",
			lines:      [][]rune{[]rune("hello world")},
			excludeRow: -1,
			excludeCol: -1,
			wantWords:  []string{"hello", "world"},
		},
		{
			name:       "excludes word at position",
			lines:      [][]rune{[]rune("hello world")},
			excludeRow: 0,
			excludeCol: 6,
			wantWords:  []string{"hello"},
			wantAbsent: []string{"world"},
		},
		{
			name:       "skips short words",
			lines:      [][]rune{[]rune("a bb ccc")},
			excludeRow: -1,
			excludeCol: -1,
			wantWords:  []string{"bb", "ccc"},
			wantAbsent: []string{"a"},
		},
		{
			name:       "deduplicates",
			lines:      [][]rune{[]rune("foo foo bar")},
			excludeRow: -1,
			excludeCol: -1,
			wantLen:    2,
		},
		{
			name:       "multiple lines",
			lines:      [][]rune{[]rune("hello"), []rune("world")},
			excludeRow: -1,
			excludeCol: -1,
			wantWords:  []string{"hello", "world"},
		},
		{
			name:       "empty lines",
			lines:      [][]rune{{}},
			excludeRow: -1,
			excludeCol: -1,
			wantLen:    0,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := ExtractWords(tt.lines, tt.excludeRow, tt.excludeCol)
			if tt.wantLen > 0 && len(got) != tt.wantLen {
				t.Errorf("len = %d, want %d", len(got), tt.wantLen)
			}
			gotSet := make(map[string]bool)
			for _, w := range got {
				gotSet[w] = true
			}
			for _, w := range tt.wantWords {
				if !gotSet[w] {
					t.Errorf("missing word %q in %v", w, got)
				}
			}
			for _, w := range tt.wantAbsent {
				if gotSet[w] {
					t.Errorf("unexpected word %q in %v", w, got)
				}
			}
		})
	}
}

func TestMatchSubsequence(t *testing.T) {
	tests := []struct {
		word, query string
		want        bool
	}{
		{"hello", "hlo", true},
		{"hello", "hello", true},
		{"hello", "h", true},
		{"hello", "xyz", false},
		{"Hello", "hello", true}, // case insensitive
		{"abc", "abcd", false},   // query longer than word
		{"", "a", false},
		{"abc", "", true},
	}

	for _, tt := range tests {
		got := MatchSubsequence(tt.word, tt.query)
		if got != tt.want {
			t.Errorf("MatchSubsequence(%q, %q) = %v, want %v", tt.word, tt.query, got, tt.want)
		}
	}
}

func TestScoreSuggestion(t *testing.T) {
	// Prefix match should score higher than non-prefix
	prefixScore := ScoreSuggestion("hello", "hel")
	subseqScore := ScoreSuggestion("help", "hlp")
	if prefixScore <= subseqScore {
		t.Errorf("prefix match (%d) should score higher than non-prefix (%d)", prefixScore, subseqScore)
	}

	// Shorter word should score higher with same prefix
	shortScore := ScoreSuggestion("foo", "fo")
	longScore := ScoreSuggestion("foobar", "fo")
	if shortScore <= longScore {
		t.Errorf("shorter word (%d) should score higher than longer (%d)", shortScore, longScore)
	}
}

func TestFindSuggestions(t *testing.T) {
	words := []string{"apple", "application", "banana", "apply", "ape"}

	t.Run("prefix match sorting", func(t *testing.T) {
		suggestions := FindSuggestions(words, "app", 10)
		if len(suggestions) != 3 {
			t.Fatalf("got %d suggestions, want 3", len(suggestions))
		}
		// All should be prefix matches, sorted by score (shorter first)
		if suggestions[0].Word != "apply" && suggestions[0].Word != "apple" {
			t.Errorf("first suggestion should be short prefix match, got %q", suggestions[0].Word)
		}
	})

	t.Run("max results limit", func(t *testing.T) {
		suggestions := FindSuggestions(words, "a", 2)
		if len(suggestions) != 2 {
			t.Errorf("got %d suggestions, want 2", len(suggestions))
		}
	})

	t.Run("no matches", func(t *testing.T) {
		suggestions := FindSuggestions(words, "xyz", 10)
		if len(suggestions) != 0 {
			t.Errorf("got %d suggestions, want 0", len(suggestions))
		}
	})

	t.Run("subsequence match", func(t *testing.T) {
		suggestions := FindSuggestions(words, "ae", 10)
		// "apple" matches a...e, "ape" matches a.e
		if len(suggestions) == 0 {
			t.Error("expected subsequence matches")
		}
	})
}

func TestNewAutocompleteState(t *testing.T) {
	suggestions := []Suggestion{{Word: "hello", Score: 100}, {Word: "help", Score: 90}}
	ac := NewAutocompleteState("hel", 5, suggestions)
	if !ac.Active {
		t.Error("should be active")
	}
	if ac.Prefix != "hel" {
		t.Errorf("Prefix = %q", ac.Prefix)
	}
	if ac.PrefixCol != 5 {
		t.Errorf("PrefixCol = %d", ac.PrefixCol)
	}
	if ac.SelectedIdx != 0 {
		t.Errorf("SelectedIdx = %d", ac.SelectedIdx)
	}
}

func TestAutocompleteStateSelected(t *testing.T) {
	t.Run("valid selection", func(t *testing.T) {
		ac := NewAutocompleteState("h", 0, []Suggestion{{Word: "hello", Score: 100}})
		sel := ac.Selected()
		if sel == nil || sel.Word != "hello" {
			t.Error("expected 'hello'")
		}
	})

	t.Run("nil state", func(t *testing.T) {
		var ac *AutocompleteState
		if ac.Selected() != nil {
			t.Error("expected nil")
		}
	})

	t.Run("empty suggestions", func(t *testing.T) {
		ac := &AutocompleteState{Suggestions: nil}
		if ac.Selected() != nil {
			t.Error("expected nil")
		}
	})

	t.Run("out of range", func(t *testing.T) {
		ac := &AutocompleteState{Suggestions: []Suggestion{{Word: "a"}}, SelectedIdx: 5}
		if ac.Selected() != nil {
			t.Error("expected nil for out of range")
		}
	})
}

func TestAutocompleteStateNextPrev(t *testing.T) {
	ac := NewAutocompleteState("h", 0, []Suggestion{
		{Word: "hello"}, {Word: "help"}, {Word: "hero"},
	})

	ac.Next()
	if ac.SelectedIdx != 1 {
		t.Errorf("after Next: idx = %d, want 1", ac.SelectedIdx)
	}
	ac.Next()
	if ac.SelectedIdx != 2 {
		t.Errorf("after 2x Next: idx = %d, want 2", ac.SelectedIdx)
	}
	ac.Next() // wraps
	if ac.SelectedIdx != 0 {
		t.Errorf("after wrap: idx = %d, want 0", ac.SelectedIdx)
	}

	ac.Prev() // wraps to end
	if ac.SelectedIdx != 2 {
		t.Errorf("after Prev from 0: idx = %d, want 2", ac.SelectedIdx)
	}
	ac.Prev()
	if ac.SelectedIdx != 1 {
		t.Errorf("after 2x Prev: idx = %d, want 1", ac.SelectedIdx)
	}

	// nil state should not panic
	var nilAC *AutocompleteState
	nilAC.Next()
	nilAC.Prev()
}

func TestAutocompleteStateUpdateSuggestions(t *testing.T) {
	ac := NewAutocompleteState("h", 0, []Suggestion{{Word: "hello"}})
	ac.SelectedIdx = 0

	newSuggs := []Suggestion{{Word: "help"}, {Word: "hero"}}
	ac.UpdateSuggestions("he", newSuggs)

	if ac.Prefix != "he" {
		t.Errorf("Prefix = %q", ac.Prefix)
	}
	if len(ac.Suggestions) != 2 {
		t.Errorf("suggestions count = %d", len(ac.Suggestions))
	}
	if ac.SelectedIdx != 0 {
		t.Error("SelectedIdx should be reset to 0")
	}
}

func TestAutocompleteStateGetWords(t *testing.T) {
	ac := &AutocompleteState{}
	lines := [][]rune{[]rune("hello world foo")}

	words := ac.GetWords(lines, 1, -1, -1)
	if len(words) < 2 {
		t.Errorf("expected at least 2 words, got %d", len(words))
	}

	// Same modCount should return cached
	words2 := ac.GetWords(lines, 1, -1, -1)
	if len(words2) != len(words) {
		t.Error("cached result should be same")
	}

	// Different modCount should recalculate
	words3 := ac.GetWords(lines, 2, -1, -1)
	if len(words3) != len(words) {
		t.Error("should still have same words")
	}
	if ac.cachedModCount != 2 {
		t.Errorf("cachedModCount = %d, want 2", ac.cachedModCount)
	}
}
