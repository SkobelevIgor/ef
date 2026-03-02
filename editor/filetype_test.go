package editor

import "testing"

func TestNewFileTypeRegistry(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go":     {Extensions: []string{".go"}, TabStop: 4},
			"python": {Extensions: []string{".py"}, TabStop: 4, ExpandTab: true},
		},
		Maps: map[string]string{},
	}
	r := NewFileTypeRegistry(cfg)
	if r == nil {
		t.Fatal("registry is nil")
	}
	if len(r.types) != 2 {
		t.Errorf("types count = %d, want 2", len(r.types))
	}
}

func TestDetectFileType(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go":     {Extensions: []string{".go"}, TabStop: 4},
			"python": {Extensions: []string{".py"}, TabStop: 4},
		},
		Maps: map[string]string{},
	}
	r := NewFileTypeRegistry(cfg)

	tests := []struct {
		filename string
		want     string
	}{
		{"main.go", "go"},
		{"script.py", "python"},
		{"file.unknown", ""},
		{"noext", ""},
		{"UPPER.GO", "go"},
	}

	for _, tt := range tests {
		t.Run(tt.filename, func(t *testing.T) {
			got := r.DetectFileType(tt.filename)
			if got != tt.want {
				t.Errorf("DetectFileType(%q) = %q, want %q", tt.filename, got, tt.want)
			}
		})
	}
}

func TestGetHighlighter(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go": {Extensions: []string{".go"}, TabStop: 4},
		},
		Maps: map[string]string{},
	}
	r := NewFileTypeRegistry(cfg)

	// No syntax rules → nil highlighter
	h := r.GetHighlighter("go")
	if h != nil {
		t.Error("expected nil highlighter (no syntax rules)")
	}

	// Unknown type
	h = r.GetHighlighter("unknown")
	if h != nil {
		t.Error("expected nil for unknown type")
	}
}

func TestGetFormatter(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go": {Extensions: []string{".go"}, TabStop: 4},
		},
		Maps: map[string]string{},
	}
	r := NewFileTypeRegistry(cfg)

	f := r.GetFormatter("go")
	if f == nil {
		t.Error("expected non-nil formatter")
	}

	// Unknown returns default
	f = r.GetFormatter("unknown")
	if f == nil {
		t.Error("expected non-nil default formatter")
	}
}

func TestGetConfigFileType(t *testing.T) {
	cfg := &Config{
		FileTypes: map[string]FileTypeConfig{
			"go": {Extensions: []string{".go"}, TabStop: 8},
		},
		Maps: map[string]string{},
	}
	r := NewFileTypeRegistry(cfg)

	got := r.GetConfig("go")
	if got.TabStop != 8 {
		t.Errorf("TabStop = %d, want 8", got.TabStop)
	}
}
