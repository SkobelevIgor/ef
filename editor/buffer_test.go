package editor

import (
	"os"
	"path/filepath"
	"testing"
)

func TestIsLineEmpty(t *testing.T) {
	// NOTE: hasContent is misnamed — it returns true when line has non-whitespace content
	tests := []struct {
		name string
		line []rune
		want bool
	}{
		{"empty line", []rune{}, false},
		{"whitespace only", []rune("   \t  "), false},
		{"has content", []rune("  hello  "), true},
		{"only content", []rune("x"), true},
		{"tabs and spaces then content", []rune("\t  a"), true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := hasContent(tt.line)
			if got != tt.want {
				t.Errorf("hasContent(%q) = %v, want %v", string(tt.line), got, tt.want)
			}
		})
	}
}

func TestGetVisualColumn(t *testing.T) {
	tests := []struct {
		name    string
		line    []rune
		charCol int
		tabStop int
		want    int
	}{
		{"no tabs", []rune("hello"), 3, 4, 3},
		{"tab at start", []rune("\thello"), 1, 4, 4},
		{"tab after text", []rune("ab\thello"), 3, 4, 4},
		{"two tabs", []rune("\t\thello"), 2, 8, 16},
		{"empty line", []rune{}, 0, 4, 0},
		{"col beyond line", []rune("hi"), 5, 4, 2},
		{"tab stop 2", []rune("\thello"), 1, 2, 2},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			buf := &Buffer{
				Lines: [][]rune{tt.line},
				Config: FileTypeConfig{
					TabStop: tt.tabStop,
				},
			}
			got := buf.GetVisualColumn(tt.line, tt.charCol)
			if got != tt.want {
				t.Errorf("got %d, want %d", got, tt.want)
			}
		})
	}
}

func TestBufferInsertChar(t *testing.T) {
	buf := newTestBuffer("hello")
	newCol := buf.InsertChar(0, 0, 'X')
	if newCol != 1 {
		t.Errorf("newCol = %d, want 1", newCol)
	}
	if string(buf.Lines[0]) != "Xhello" {
		t.Errorf("line = %q, want %q", string(buf.Lines[0]), "Xhello")
	}
	if !buf.Modified {
		t.Error("Modified should be true")
	}
	if buf.ModCount != 1 {
		t.Errorf("ModCount = %d, want 1", buf.ModCount)
	}
	newCol = buf.InsertChar(0, 6, '!')
	if string(buf.Lines[0]) != "Xhello!" {
		t.Errorf("line = %q", string(buf.Lines[0]))
	}
}

func TestBufferDeleteChar(t *testing.T) {
	t.Run("backspace mid-line", func(t *testing.T) {
		buf := newTestBuffer("hello")
		r, c := buf.DeleteChar(0, 3)
		if r != 0 || c != 2 {
			t.Errorf("got (%d,%d), want (0,2)", r, c)
		}
		if string(buf.Lines[0]) != "helo" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
	t.Run("backspace at start joins lines", func(t *testing.T) {
		buf := newTestBuffer("hello", "world")
		r, c := buf.DeleteChar(1, 0)
		if r != 0 || c != 5 {
			t.Errorf("got (%d,%d), want (0,5)", r, c)
		}
		if len(buf.Lines) != 1 || string(buf.Lines[0]) != "helloworld" {
			t.Errorf("lines = %v", buf.Lines)
		}
	})
	t.Run("backspace at 0,0 no-op", func(t *testing.T) {
		buf := newTestBuffer("hello")
		r, c := buf.DeleteChar(0, 0)
		if r != 0 || c != 0 {
			t.Errorf("got (%d,%d)", r, c)
		}
	})
}

func TestBufferDeleteCharForward(t *testing.T) {
	t.Run("delete mid-line", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.DeleteCharForward(0, 2)
		if string(buf.Lines[0]) != "helo" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
	t.Run("delete at end joins lines", func(t *testing.T) {
		buf := newTestBuffer("hello", "world")
		buf.DeleteCharForward(0, 5)
		if len(buf.Lines) != 1 || string(buf.Lines[0]) != "helloworld" {
			t.Errorf("lines = %v", buf.Lines)
		}
	})
	t.Run("delete at end of last line no-op", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.DeleteCharForward(0, 5)
		if string(buf.Lines[0]) != "hello" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
}

func TestBufferInsertNewline(t *testing.T) {
	t.Run("split at middle", func(t *testing.T) {
		buf := newTestBuffer("hello world")
		r, c := buf.InsertNewline(0, 5)
		if r != 1 || c != 0 {
			t.Errorf("got (%d,%d)", r, c)
		}
		if string(buf.Lines[0]) != "hello" || string(buf.Lines[1]) != " world" {
			t.Errorf("lines = %q, %q", string(buf.Lines[0]), string(buf.Lines[1]))
		}
	})
	t.Run("split at start", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.InsertNewline(0, 0)
		if string(buf.Lines[0]) != "" || string(buf.Lines[1]) != "hello" {
			t.Errorf("lines = %q, %q", string(buf.Lines[0]), string(buf.Lines[1]))
		}
	})
}

func TestBufferInsertNewlineWithIndent(t *testing.T) {
	t.Run("with autoindent", func(t *testing.T) {
		buf := newTestBuffer("  hello world")
		buf.Config.AutoIndentation = true
		r, c := buf.InsertNewlineWithIndent(0, 7)
		if r != 1 || c != 2 {
			t.Errorf("got (%d,%d), want (1,2)", r, c)
		}
	})
	t.Run("without autoindent", func(t *testing.T) {
		buf := newTestBuffer("  hello")
		buf.Config.AutoIndentation = false
		_, c := buf.InsertNewlineWithIndent(0, 7)
		if c != 0 {
			t.Errorf("col = %d, want 0", c)
		}
	})
}

func TestBufferOpenLineBelow(t *testing.T) {
	buf := newTestBuffer("  hello", "world")
	buf.Config.AutoIndentation = true
	r, c := buf.OpenLineBelow(0)
	if r != 1 || c != 2 {
		t.Errorf("got (%d,%d), want (1,2)", r, c)
	}
	if len(buf.Lines) != 3 {
		t.Fatalf("line count = %d", len(buf.Lines))
	}
}

func TestBufferOpenLineAbove(t *testing.T) {
	buf := newTestBuffer("  hello")
	buf.Config.AutoIndentation = true
	r, c := buf.OpenLineAbove(0)
	if r != 0 || c != 2 {
		t.Errorf("got (%d,%d), want (0,2)", r, c)
	}
}

func TestBufferInsertTab(t *testing.T) {
	t.Run("real tab", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.Config.ExpandTab = false
		c := buf.InsertTab(0, 0)
		if c != 1 || buf.Lines[0][0] != '\t' {
			t.Errorf("col=%d, char=%q", c, buf.Lines[0][0])
		}
	})
	t.Run("expand tab", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.Config.ExpandTab = true
		buf.Config.TabStop = 4
		c := buf.InsertTab(0, 0)
		if c != 4 {
			t.Errorf("col = %d, want 4", c)
		}
	})
}

func TestBufferDeleteLine(t *testing.T) {
	t.Run("normal", func(t *testing.T) {
		buf := newTestBuffer("hello", "world", "third")
		deleted := buf.DeleteLine(1)
		if string(deleted) != "world" {
			t.Errorf("deleted = %q", string(deleted))
		}
		if len(buf.Lines) != 2 {
			t.Errorf("count = %d", len(buf.Lines))
		}
	})
	t.Run("single line buffer", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.DeleteLine(0)
		if len(buf.Lines) != 1 || len(buf.Lines[0]) != 0 {
			t.Errorf("expected empty line")
		}
	})
	t.Run("out of bounds", func(t *testing.T) {
		buf := newTestBuffer("hello")
		if buf.DeleteLine(-1) != nil {
			t.Error("expected nil")
		}
		if buf.DeleteLine(5) != nil {
			t.Error("expected nil")
		}
	})
}

func TestBufferDeleteCharAt(t *testing.T) {
	buf := newTestBuffer("hello")
	deleted := buf.DeleteCharAt(0, 1)
	if deleted != 'e' {
		t.Errorf("deleted = %q", deleted)
	}
	if buf.DeleteCharAt(0, 100) != 0 {
		t.Error("expected 0 for out of bounds")
	}
}

func TestBufferCopyLine(t *testing.T) {
	buf := newTestBuffer("hello", "world")
	copied := buf.CopyLine(0)
	if string(copied) != "hello" {
		t.Errorf("got %q", string(copied))
	}
	copied[0] = 'X'
	if buf.Lines[0][0] == 'X' {
		t.Error("not a deep copy")
	}
	if buf.CopyLine(-1) != nil || buf.CopyLine(5) != nil {
		t.Error("expected nil for out of bounds")
	}
}

func TestBufferInsertLineAfterBefore(t *testing.T) {
	buf := newTestBuffer("hello", "world")
	buf.InsertLineAfter(0, []rune("middle"))
	if len(buf.Lines) != 3 || string(buf.Lines[1]) != "middle" {
		t.Errorf("insert after failed")
	}

	buf2 := newTestBuffer("hello", "world")
	buf2.InsertLineBefore(1, []rune("middle"))
	if len(buf2.Lines) != 3 || string(buf2.Lines[1]) != "middle" {
		t.Errorf("insert before failed")
	}
}

func TestBufferDeleteRange(t *testing.T) {
	t.Run("same line", func(t *testing.T) {
		buf := newTestBuffer("hello world")
		deleted := buf.DeleteRange(0, 2, 0, 5)
		if string(deleted[0]) != "llo" {
			t.Errorf("deleted = %q", string(deleted[0]))
		}
		if string(buf.Lines[0]) != "he world" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
	t.Run("multi-line", func(t *testing.T) {
		buf := newTestBuffer("hello", "middle", "world")
		buf.DeleteRange(0, 3, 2, 2)
		if len(buf.Lines) != 1 || string(buf.Lines[0]) != "helrld" {
			t.Errorf("result = %q", string(buf.Lines[0]))
		}
	})
}

func TestBufferGetRange(t *testing.T) {
	t.Run("same line", func(t *testing.T) {
		buf := newTestBuffer("hello world")
		result := buf.GetRange(0, 0, 0, 4)
		if string(result[0]) != "hello" {
			t.Errorf("got %q", string(result[0]))
		}
	})
	t.Run("multi-line", func(t *testing.T) {
		buf := newTestBuffer("hello", "world", "test")
		result := buf.GetRange(0, 3, 1, 2)
		if len(result) != 2 || string(result[0]) != "lo" || string(result[1]) != "wor" {
			t.Errorf("got %v", result)
		}
	})
}

func TestBufferIndentRange(t *testing.T) {
	t.Run("tab indent", func(t *testing.T) {
		buf := newTestBuffer("hello", "world")
		buf.Config.ExpandTab = false
		buf.IndentRange(0, 1)
		if buf.Lines[0][0] != '\t' || buf.Lines[1][0] != '\t' {
			t.Error("not indented with tab")
		}
	})
	t.Run("space indent", func(t *testing.T) {
		buf := newTestBuffer("hello")
		buf.Config.ExpandTab = true
		buf.Config.ShiftWidth = 2
		buf.IndentRange(0, 0)
		if string(buf.Lines[0]) != "  hello" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
}

func TestBufferUnindentRange(t *testing.T) {
	t.Run("remove tab", func(t *testing.T) {
		buf := newTestBuffer("\thello")
		buf.UnindentRange(0, 0)
		if string(buf.Lines[0]) != "hello" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
	t.Run("remove spaces", func(t *testing.T) {
		buf := newTestBuffer("    hello")
		buf.Config.ShiftWidth = 4
		buf.UnindentRange(0, 0)
		if string(buf.Lines[0]) != "hello" {
			t.Errorf("line = %q", string(buf.Lines[0]))
		}
	})
	t.Run("empty line", func(t *testing.T) {
		buf := newTestBuffer("")
		buf.UnindentRange(0, 0)
		if len(buf.Lines[0]) != 0 {
			t.Error("should stay empty")
		}
	})
}

func TestBufferReindentRange(t *testing.T) {
	buf := newTestBuffer("  hello", "world", "  test")
	buf.ReindentRange(1, 2)
	if string(buf.Lines[1]) != "  world" {
		t.Errorf("line 1 = %q", string(buf.Lines[1]))
	}
}

func TestBufferFindAllMatches(t *testing.T) {
	t.Run("normal", func(t *testing.T) {
		buf := newTestBuffer("hello world", "hello again")
		if len(buf.FindAllMatches("hello")) != 2 {
			t.Error("expected 2 matches")
		}
	})
	t.Run("case insensitive", func(t *testing.T) {
		buf := newTestBuffer("Hello HELLO hello")
		if len(buf.FindAllMatches("hello")) != 3 {
			t.Error("expected 3 matches")
		}
	})
	t.Run("empty query", func(t *testing.T) {
		buf := newTestBuffer("hello")
		if buf.FindAllMatches("") != nil {
			t.Error("expected nil")
		}
	})
}

func TestBufferSaveLoad(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "test.txt")
	buf := &Buffer{
		Filename: path,
		Lines:    [][]rune{[]rune("hello"), []rune("world")},
		Config:   FileTypeConfig{TabStop: 4},
	}
	if err := buf.Save(); err != nil {
		t.Fatalf("Save: %v", err)
	}
	if buf.Modified {
		t.Error("Modified should be false after save")
	}
	data, _ := os.ReadFile(path)
	if string(data) != "hello\nworld" {
		t.Errorf("file = %q", string(data))
	}
	buf2 := &Buffer{Filename: path, Lines: [][]rune{{}}}
	if err := buf2.Load(); err != nil {
		t.Fatalf("Load: %v", err)
	}
	if len(buf2.Lines) != 2 || string(buf2.Lines[0]) != "hello" {
		t.Errorf("loaded = %v", buf2.Lines)
	}
}

func TestBufferGetShiftWidth(t *testing.T) {
	buf := newTestBuffer("hello")
	buf.Config.ShiftWidth = 0
	if buf.GetShiftWidth() != DefaultTabStop {
		t.Errorf("got %d", buf.GetShiftWidth())
	}
	buf.Config.ShiftWidth = 8
	if buf.GetShiftWidth() != 8 {
		t.Errorf("got %d", buf.GetShiftWidth())
	}
}

func TestStripLeadingWhitespace(t *testing.T) {
	if string(stripLeadingWhitespace([]rune("  hello"))) != "hello" {
		t.Error("failed")
	}
	if stripLeadingWhitespace([]rune("   ")) != nil {
		t.Error("should return nil for all whitespace")
	}
}

func TestLastNonWhitespace(t *testing.T) {
	if lastNonWhitespace([]rune("hello  ")) != 'o' {
		t.Error("expected 'o'")
	}
	if lastNonWhitespace([]rune("   ")) != 0 {
		t.Error("expected 0")
	}
	if lastNonWhitespace([]rune{}) != 0 {
		t.Error("expected 0")
	}
}

func TestBufferGetVisualLineWidth(t *testing.T) {
	buf := newTestBuffer("hello")
	got := buf.GetVisualLineWidth([]rune("hello"))
	if got != 5 {
		t.Errorf("got %d, want 5", got)
	}

	// With tab
	got = buf.GetVisualLineWidth([]rune("\thello"))
	if got != 9 { // tab expands to 4, then 5 chars
		t.Errorf("got %d, want 9", got)
	}

	// Empty
	got = buf.GetVisualLineWidth([]rune{})
	if got != 0 {
		t.Errorf("got %d, want 0", got)
	}
}

func TestNewBuffer_NewFile(t *testing.T) {
	// Non-existent file: should create buffer with one empty line
	buf, err := NewBuffer("/tmp/ef_test_nonexistent_file_12345.txt")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(buf.Lines) != 1 {
		t.Errorf("Lines count = %d, want 1", len(buf.Lines))
	}
	if len(buf.Lines[0]) != 0 {
		t.Error("first line should be empty")
	}
	if buf.Modified {
		t.Error("new buffer should not be modified")
	}
}

func TestNewBuffer_ExistingFile(t *testing.T) {
	// Create a temp file
	tmp := t.TempDir()
	path := tmp + "/test.txt"
	os.WriteFile(path, []byte("hello\nworld"), 0644)

	buf, err := NewBuffer(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(buf.Lines) != 2 {
		t.Errorf("Lines count = %d, want 2", len(buf.Lines))
	}
	if string(buf.Lines[0]) != "hello" {
		t.Errorf("line 0 = %q, want 'hello'", string(buf.Lines[0]))
	}
	if string(buf.Lines[1]) != "world" {
		t.Errorf("line 1 = %q, want 'world'", string(buf.Lines[1]))
	}
}

func TestNewBufferWithRegistry(t *testing.T) {
	tmp := t.TempDir()
	path := tmp + "/test.go"
	os.WriteFile(path, []byte("package main\n"), 0644)

	registry := NewFileTypeRegistry(DefaultConfig())
	buf, err := NewBufferWithRegistry(path, registry)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// DefaultConfig has no file types, so FileType will be empty
	// but the buffer should still load correctly
	if len(buf.Lines) != 1 {
		t.Errorf("Lines count = %d, want 1", len(buf.Lines))
	}
	if string(buf.Lines[0]) != "package main" {
		t.Errorf("line 0 = %q", string(buf.Lines[0]))
	}
}

func TestNewBufferWithRegistry_NilRegistry(t *testing.T) {
	buf, err := NewBufferWithRegistry("/tmp/ef_test_nil_registry_12345.txt", nil)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if buf.FileType != "" {
		t.Error("FileType should be empty with nil registry")
	}
}
