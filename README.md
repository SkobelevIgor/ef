# EF (Edit Files)

A terminal-based text editor written in Go, inspired by Vim.

## Installation

```bash
go build -o ef .
```

## Usage

```bash
# Single file
./ef filename.go

# Open file at specific line
./ef filename.go:42

# Multiple files - vertical splits (side-by-side, default)
./ef file1.go file2.go file3.go

# Multiple files - horizontal splits (stacked, with -h flag)
./ef -h file1.go file2.go file3.go

# Multiple files with line numbers
./ef file1.go:10 file2.go:25 file3.go:100

# Horizontal splits with line numbers
./ef -h file1.go:10 file2.go:25
```

## Modes

| Mode | Line Number Style | Description |
|------|------------------|-------------|
| **Normal** | Blue | Navigation and commands |
| **Insert** | Green | Text editing |
| **Visual** | Purple | Selection mode |

## Key Bindings

### Global (all modes)
- `F4` - Open search
- `F10` - Save and quit
- `Shift+Tab` - Switch panes (split view)
- `Ctrl+z` - Suspend to background (use `fg` to resume)

### Normal Mode

#### Navigation
- `h/j/k/l` or arrow keys - Movement
- `[count]` prefix - Repeat motion (e.g., `5j`)
- `w` / `b` - Next/previous word
- `0` / `$` - Line start/end
- `g` - Go to first line
- `G` - Go to last line (or `[count]G` to specific line)
- `Ctrl+d` / `Ctrl+u` - Page down/up (half screen)
- `f<char>` / `F<char>` - Find character forward/backward
- `;` / `,` - Repeat last find / reverse
- `:<number>Enter` - Go to line number
- `m<id>` - Set mark (a-z, A-Z, 0-9)
- `` `<id> `` - Jump to mark

#### Editing
- `x` - Delete character under cursor
- `dd` - Delete line
- `dw` / `db` - Delete word forward/backward
- `d$` / `d0` - Delete to end/start of line
- `yy` - Copy (yank) line
- `p` / `P` - Paste after/before cursor
- `u` - Undo
- `Ctrl+r` - Redo

#### Mode Switching
- `i` - Insert at cursor
- `a` - Append after cursor
- `A` - Append at end of line
- `I` - Insert at line start
- `o` / `O` - Open line below/above
- `v` - Enter Visual mode
- `Esc` - Reset pending input

### Insert Mode
- Type normally to insert text
- `Esc` - Return to Normal mode
- Arrow keys - Navigation
- `Ctrl+d` / `Ctrl+u` - Page down/up

#### Autocomplete (appears after 2+ characters)
- `Ctrl+n` / `Down` - Next suggestion
- `Ctrl+p` / `Up` - Previous suggestion
- `Tab` / `Enter` - Accept suggestion
- `Esc` - Dismiss autocomplete

### Search Mode (F4)
- Type search query and press `Enter`
- `n` - Jump to next match (Normal mode)
- `N` - Jump to previous match (Normal mode)
- Toggle "Replace" to enable find-and-replace

### Visual Mode
- `h/j/k/l` or arrow keys - Extend selection
- `w` / `b` - Extend by word
- `0` / `$` - Selection to line start/end
- `g` / `G` - Selection to file start/end
- `Ctrl+d` / `Ctrl+u` - Page down/up
- `f<char>` / `F<char>` - Find character forward/backward
- `;` / `,` - Repeat last find / reverse
- `y` - Copy (yank) selection
- `d` / `x` - Delete selection
- `>` / `<` - Indent/unindent selection
- `p` / `P` - Replace selection with clipboard
- `Esc` or `v` - Exit Visual mode

## Features

- **Modal Editing** - Vim-style Normal/Insert/Visual modes
- **Navigation** - Word, line, page, character find, marks
- **Editing** - Yank, delete, paste, undo/redo
- **Search & Replace** - Case-insensitive search with find-and-replace
- **Autocomplete** - Word suggestions based on buffer content
- **Multi-File Splits** - Edit multiple files side-by-side (vertical, default) or stacked (horizontal with `-h`)
- **Syntax Highlighting** - Configurable per file type
- **Auto-save** - Saves 200ms after changes
- **Relative Line Numbers** - With mode-colored indicators

## Configuration

User configuration file: `~/.efconfig` (JSON format)

```json
{
	"file_types": {
		"go": {
			"extensions": [".go"],
			"syntax_highlighting": true,
			"tabstop": 4,
			"shiftwidth": 4,
			"autoindentation": true,
			"expandtab": false,
			"syntax_rules": [
				{"pattern": "//.*$", "style": {"color": "gray", "bold": false}, "priority": 30},
				{"pattern": "\"([^\"\\\\]|\\\\.)*\"", "style": {"color": "green", "bold": false}, "priority": 20},
				{"pattern": "`[^`]*`", "style": {"color": "green", "bold": false}, "priority": 20},
				{"pattern": "'([^'\\\\]|\\\\.)'", "style": {"color": "green", "bold": false}, "priority": 20},
				{"pattern": "\\b(break|case|chan|const|continue|default|defer|else|fallthrough|for|func|go|goto|if|import|interface|map|package|range|return|select|struct|switch|type|var)\\b", "style": {"color": "yellow", "bold": false}, "priority": 10},
				{"pattern": "\\b(bool|byte|complex64|complex128|error|float32|float64|int|int8|int16|int32|int64|rune|string|uint|uint8|uint16|uint32|uint64|uintptr)\\b", "style": {"color": "teal", "bold": false}, "priority": 10},
				{"pattern": "\\b(append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover)\\b", "style": {"color": "aqua", "bold": false}, "priority": 10},
				{"pattern": "\\.\\w+", "style": {"color": "yellow", "bold": false}, "priority": 10},
				{"pattern": "\\b(true|false|nil|iota)\\b", "style": {"color": "purple", "bold": false}, "priority": 10},
				{"pattern": "[\\(\\)\\[\\]\\{\\}]", "style": {"color": "purple", "bold": false}, "priority": 3},
				{"pattern": "\\b\\d+(\\.\\d+)?\\b", "style": {"color": "blue", "bold": true}, "priority": 5},
				{"pattern": "\\b0x[0-9a-fA-F]+\\b", "style": {"color": "blue", "bold": true}, "priority": 5}
			]
		},
    ...
	},
	"maps": {
		"{{": "{}<Esc>ha<Enter><Esc>ko<Tab>",
		"((": "()<Esc>ha",
		"[[": "[]<Esc>ha",
		"\"\"": "\"\"<Esc>ha",
		"''": "''<Esc>ha",
		"``": "``<Esc>ha"
	}
}
```

## Dependencies

- [tcell](https://github.com/gdamore/tcell) - Terminal handling
- [fsnotify](https://github.com/fsnotify/fsnotify) - File watching
