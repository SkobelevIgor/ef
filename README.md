# EF (Edit Files)

A terminal-based text editor written in C, inspired by Vim.

## Supported Platforms

- **macOS** (primary) - uses Homebrew ncurses and pcre2
- **Linux** (Debian/Ubuntu, Fedora/RHEL, Arch) - uses system packages

## Prerequisites

- **CMake** >= 3.16
- **ncursesw** (wide character support)
- **PCRE2** (regular expressions)

### macOS

```bash
brew install ncurses pcre2
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake libncursesw5-dev libpcre2-dev pkg-config
```

### Linux (Fedora/RHEL)

```bash
sudo dnf install gcc cmake ncurses-devel pcre2-devel
```

### Linux (Arch)

```bash
sudo pacman -S base-devel cmake ncurses pcre2
```

## Build

```bash
cmake -B build && cmake --build build
```

The binary is produced at `build/ef`.

## Install

```bash
sudo cmake --install build
```

Installs `ef` to `/usr/local/bin` by default. To choose a different prefix:

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=~/.local && cmake --build build && cmake --install build
```

## Run Tests

```bash
ctest --test-dir build
```

## Usage

```bash
# Single file
./build/ef filename.go

# Open file at specific line
./build/ef filename.go:42

# Multiple files - vertical splits (side-by-side, default)
./build/ef file1.go file2.go file3.go

# Multiple files - horizontal splits (stacked, with -h flag)
./build/ef -h file1.go file2.go file3.go

# Multiple files with line numbers
./build/ef file1.go:10 file2.go:25 file3.go:100

# Horizontal splits with line numbers
./build/ef -h file1.go:10 file2.go:25
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
- **Syntax Highlighting** - Configurable per file type via PCRE2 regex
- **Auto-save** - Saves 200ms after changes
- **Relative Line Numbers** - With mode-colored indicators

## Configuration

User configuration file: `~/.efconfig` (JSON format).

On first run, if `~/.efconfig` doesn't exist, `ef` creates it with default syntax rules for Go, Python, and JavaScript/TypeScript.

See [.efconfig](.efconfig) for the full default configuration.

Minimal example:

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
				{"pattern": "(?:^|\\s)//.*$", "style": {"color": "gray", "bold": false}, "priority": 30},
				{"pattern": "\"([^\"\\\\]|\\\\.)*\"", "style": {"color": "green", "bold": false}, "priority": 35},
				{"pattern": "`[^`]*`", "style": {"color": "green", "bold": false}, "priority": 35},
				{"pattern": "'([^'\\\\]|\\\\.)'", "style": {"color": "green", "bold": false}, "priority": 35},
				{"pattern": "\\b(break|case|chan|const|continue|default|defer|else|fallthrough|for|func|go|goto|if|import|interface|map|package|range|return|select|struct|switch|type|var)\\b", "style": {"color": "yellow", "bold": false}, "priority": 10},
				{"pattern": "\\w+\\(", "style": {"color": "yellow", "bold": false}, "priority": 8},
				{"pattern": "[\\(\\)\\[\\]\\{\\}]", "style": {"color": "purple", "bold": false}, "priority": 11},
				{"pattern": "\\b\\d+(\\.\\d+)?\\b", "style": {"color": "blue", "bold": true}, "priority": 5}
			]
		}
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

- [ncursesw](https://invisible-island.net/ncurses/) - Terminal handling with wide character support
- [PCRE2](https://github.com/PCRE2Project/pcre2) - Regular expressions for syntax highlighting
- [cJSON](https://github.com/DaveGamble/cJSON) - JSON parsing (embedded)
- [Unity](https://github.com/ThrowTheSwitch/Unity) - Unit test framework (embedded)
