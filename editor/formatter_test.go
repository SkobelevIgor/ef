package editor

import "testing"

func TestNewBaseFormatter(t *testing.T) {
	f := NewBaseFormatter(8, true)
	if f.TabSize() != 8 {
		t.Errorf("TabSize = %d, want 8", f.TabSize())
	}
	if !f.UseSpaces() {
		t.Error("UseSpaces should be true")
	}

	// Zero tabSize defaults to 4
	f = NewBaseFormatter(0, false)
	if f.TabSize() != 4 {
		t.Errorf("TabSize = %d, want 4 (default)", f.TabSize())
	}
	if f.UseSpaces() {
		t.Error("UseSpaces should be false")
	}
}

func TestBaseFormatterFormat(t *testing.T) {
	f := NewBaseFormatter(4, false)
	lines := [][]rune{[]rune("hello"), []rune("world")}
	result := f.Format(lines)
	if result.Changed {
		t.Error("should not change (no-op formatter)")
	}
	if len(result.Lines) != 2 {
		t.Errorf("lines count = %d", len(result.Lines))
	}
}

func TestBaseFormatterFormatRange(t *testing.T) {
	f := NewBaseFormatter(4, false)
	lines := [][]rune{[]rune("a"), []rune("b")}
	result := f.FormatRange(lines, 0, 1)
	if result.Changed {
		t.Error("should not change")
	}
}

func TestBaseFormatterFormatOnSave(t *testing.T) {
	f := NewBaseFormatter(4, false)
	if f.FormatOnSave() {
		t.Error("should return false")
	}
}

func TestBaseFormatterGetIndent(t *testing.T) {
	f := NewBaseFormatter(4, false)

	t.Run("indented line", func(t *testing.T) {
		indent := f.GetIndent([]rune("  hello"), 0)
		if string(indent) != "  " {
			t.Errorf("got %q, want %q", string(indent), "  ")
		}
	})

	t.Run("tab indented", func(t *testing.T) {
		indent := f.GetIndent([]rune("\thello"), 0)
		if len(indent) != 1 || indent[0] != '\t' {
			t.Errorf("got %q", string(indent))
		}
	})

	t.Run("no indent", func(t *testing.T) {
		indent := f.GetIndent([]rune("hello"), 0)
		if len(indent) != 0 {
			t.Errorf("got %q", string(indent))
		}
	})

	t.Run("empty line", func(t *testing.T) {
		indent := f.GetIndent([]rune{}, 0)
		if indent != nil {
			t.Errorf("got %q, want nil", string(indent))
		}
	})
}
