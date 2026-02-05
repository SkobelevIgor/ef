# Project Transition: FE → EF

**Date**: 2026-02-05
**From**: FE (File Edit)
**To**: EF (Edit Files)

## Summary

This document captures all information needed to transition the project from "fe" to "ef" and migrate to a new repository.

## Files Changed for Rename

### Core Files

| File | Change |
|------|--------|
| `go.mod` | `module fe` → `module ef` |
| `main.go` | `fe/editor` → `ef/editor`, usage message |
| `README.md` | FE references → EF |
| `.feconfig` | Rename to `.efconfig` |
| `editor/config.go` | `.feconfig` → `.efconfig` references |
| `.specify/memory/constitution.md` | FE → EF, build command |
| `CLAUDE.md` | Build command updated |

### Build Commands

All occurrences of `go build -o fe .` should become `go build -o ef .`

Files with build commands:
- `.specify/memory/constitution.md`
- `specs/001-search-mode/tasks.md`
- `specs/002-autocomplete/tasks.md`
- `specs/003-marks-navigation/tasks.md`
- `specs/*/quickstart.md`

## Project Structure

```
ef/
├── main.go                 # Entry point
├── go.mod                  # Module: ef
├── go.sum                  # Dependencies
├── README.md               # Documentation
├── CLAUDE.md               # AI agent context
├── .efconfig               # User configuration (was .feconfig)
├── editor/                 # Core editor package
│   ├── editor.go           # Main editor logic
│   ├── buffer.go           # Text buffer management
│   ├── screen.go           # Terminal rendering
│   ├── input.go            # Input state management
│   ├── mode.go             # Mode definitions
│   ├── history.go          # Undo/redo
│   ├── commands.go         # Command handling
│   ├── config.go           # Configuration loading
│   ├── filetype.go         # File type detection
│   ├── syntax.go           # Syntax highlighting
│   ├── formatter.go        # Text formatting
│   ├── watcher.go          # File change watcher
│   ├── search.go           # Search functionality
│   ├── autocomplete.go     # Autocomplete feature
│   └── marks.go            # Marks navigation
├── specs/                  # Feature specifications
│   ├── 001-search-mode/
│   ├── 002-autocomplete/
│   └── 003-marks-navigation/
└── .specify/               # Speckit templates and memory
    ├── memory/
    │   └── constitution.md
    ├── templates/
    └── scripts/
```

## Dependencies

```
github.com/gdamore/tcell/v2 v2.7.4   # Terminal handling
github.com/fsnotify/fsnotify v1.7.0  # File watching
```

## Features Implemented

1. **Modal Editing** (Vim-style)
   - Normal, Insert, Visual modes
   - Standard vim keybindings

2. **Navigation**
   - h/j/k/l, arrow keys
   - Word navigation (w/b)
   - Line navigation (0/$)
   - Page navigation (Ctrl+d/u)
   - Go to line (:n, G, g)
   - Character find (f/F, ;/,)
   - **Marks** (m<id>, `<id>)

3. **Editing**
   - Insert/append modes
   - Delete operations (x, dd, dw, db, d$, d0)
   - Yank/paste (yy, y, p, P)
   - Undo/redo (u, Ctrl+r)
   - Auto-save (200ms delay)

4. **Visual Mode**
   - Character/line selection
   - Selection operations (y, d, x, >, <)

5. **Search & Replace**
   - F4 to open search
   - Case-insensitive search
   - Find and replace mode
   - n/N for next/previous match

6. **Autocomplete**
   - Triggered after 2+ chars
   - Shallow/subsequence matching
   - Ctrl+n/p or Up/Down navigation
   - Tab/Enter to accept

7. **Split View**
   - Two files side by side
   - Shift+Tab to switch panes

8. **Syntax Highlighting**
   - Configurable per file type
   - Keywords, strings, comments, numbers

## Configuration

User config file: `~/.efconfig` (JSON format)

```json
{
  "filetypes": {
    "go": {
      "tabstop": 4,
      "shiftwidth": 4,
      "autoIndentation": true,
      "expandTab": false
    }
  },
  "maps": {
    "jk": "<Esc>"
  }
}
```

## Git History Summary

Key commits to preserve:
- Initial editor implementation
- Search mode with find-and-replace
- Autocomplete feature
- Marks navigation feature

## Migration Steps

1. Create new repository named `ef`
2. Copy all files (excluding .git)
3. Run the rename script (see below)
4. Initialize new git repo
5. Make initial commit
6. Push to new remote

## Automated Rename Script

```bash
#!/bin/bash
# Run from project root after copying to new location

# Rename config file
mv .feconfig .efconfig 2>/dev/null || true

# Update go.mod
sed -i '' 's/module fe/module ef/' go.mod

# Update config.go
sed -i '' 's/\.feconfig/.efconfig/g' editor/config.go

# Update README.md
sed -i '' 's/# FE/# EF/g' README.md
sed -i '' 's/(File Edit)/(Edit Files)/g' README.md
sed -i '' 's/\.\/fe/.\/ef/g' README.md

# Update constitution
sed -i '' 's/# FE/# EF/g' .specify/memory/constitution.md
sed -i '' 's/(File Edit)/(Edit Files)/g' .specify/memory/constitution.md
sed -i '' 's/go build -o fe/go build -o ef/g' .specify/memory/constitution.md
sed -i '' 's/for FE/for EF/g' .specify/memory/constitution.md

# Update all tasks.md files
find specs -name "tasks.md" -exec sed -i '' 's/go build -o fe/go build -o ef/g' {} \;

# Update all quickstart.md files
find specs -name "quickstart.md" -exec sed -i '' 's/go build -o fe/go build -o ef/g' {} \;
find specs -name "quickstart.md" -exec sed -i '' 's/rm -f fe/rm -f ef/g' {} \;

# Build new binary
rm -f fe ef
go build -o ef .

echo "Rename complete. Binary: ./ef"
```

## Key Bindings Reference

### Global
| Key | Action |
|-----|--------|
| F4 | Open search |
| F10 | Save and quit |
| Ctrl+z | Suspend |
| Shift+Tab | Switch panes |

### Normal Mode
| Key | Action |
|-----|--------|
| h/j/k/l | Movement |
| w/b | Word navigation |
| 0/$ | Line start/end |
| g/G | File start/end |
| f/F | Find char |
| m<id> | Set mark |
| `<id> | Jump to mark |
| i/a/A/I/o/O | Enter insert |
| v | Enter visual |
| dd/dw/db/d$/d0 | Delete |
| yy | Yank line |
| p/P | Paste |
| u | Undo |
| Ctrl+r | Redo |
| x | Delete char |
| :<n> | Go to line |

### Insert Mode
| Key | Action |
|-----|--------|
| Esc | Return to normal |
| Arrows | Navigation |
| Tab/Enter | Accept autocomplete |
| Ctrl+n/p | Navigate autocomplete |

### Visual Mode
| Key | Action |
|-----|--------|
| h/j/k/l | Extend selection |
| y | Yank selection |
| d/x | Delete selection |
| >/< | Indent/unindent |
| Esc/v | Exit visual |

## Constitution Principles

1. **Terminal Native** - All via terminal I/O
2. **Vim-Style Modal Editing** - Normal/Insert/Visual modes
3. **Data Integrity** - Auto-save, undo/redo, no data loss
