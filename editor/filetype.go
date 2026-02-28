package editor

import (
	"path/filepath"
	"strings"
)

// FileTypeInfo holds information about a file type
type FileTypeInfo struct {
	Name        string
	Extensions  []string
	Highlighter SyntaxHighlighter
	Formatter   Formatter
}

// FileTypeRegistry manages file type detection and associated tools
type FileTypeRegistry struct {
	types     map[string]*FileTypeInfo
	extToType map[string]string
	config    *Config
}

// NewFileTypeRegistry creates a new file type registry
func NewFileTypeRegistry(config *Config) *FileTypeRegistry {
	r := &FileTypeRegistry{
		types:     make(map[string]*FileTypeInfo),
		extToType: make(map[string]string),
		config:    config,
	}

	// Register file types from config
	for ftName, ftConfig := range config.FileTypes {
		r.registerFileTypeFromConfig(ftName, ftConfig)
	}

	return r
}

// registerFileTypeFromConfig registers a file type from config
func (r *FileTypeRegistry) registerFileTypeFromConfig(ftName string, ftConfig FileTypeConfig) {
	extensions := ftConfig.Extensions

	info := &FileTypeInfo{
		Name:       ftName,
		Extensions: extensions,
	}

	// Create highlighter if syntax highlighting is enabled and rules exist
	if ftConfig.SyntaxHighlighting && len(ftConfig.SyntaxRules) > 0 {
		info.Highlighter = NewSyntaxHighlighter(ftConfig)
	}

	// Create formatter
	tabStop := ftConfig.TabStop
	if tabStop <= 0 {
		tabStop = DefaultTabStop
	}
	info.Formatter = NewBaseFormatter(tabStop, ftConfig.ExpandTab)

	r.types[ftName] = info

	// Map extensions to file type
	for _, ext := range extensions {
		r.extToType[ext] = ftName
	}
}

// DetectFileType determines the file type from a filename
// Returns empty string if file type is not recognized
func (r *FileTypeRegistry) DetectFileType(filename string) string {
	ext := strings.ToLower(filepath.Ext(filename))
	if ext == "" {
		return ""
	}

	if ft, ok := r.extToType[ext]; ok {
		return ft
	}

	return ""
}

// GetHighlighter returns the syntax highlighter for a file type
func (r *FileTypeRegistry) GetHighlighter(ft string) SyntaxHighlighter {
	if info, ok := r.types[ft]; ok {
		return info.Highlighter
	}
	return nil
}

// GetFormatter returns the formatter for a file type
func (r *FileTypeRegistry) GetFormatter(ft string) Formatter {
	if info, ok := r.types[ft]; ok && info.Formatter != nil {
		return info.Formatter
	}
	// Return a default formatter
	return NewBaseFormatter(4, false)
}

// GetConfig returns the configuration for a file type
func (r *FileTypeRegistry) GetConfig(ft string) FileTypeConfig {
	return r.config.GetFileTypeConfig(ft)
}
