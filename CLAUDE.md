- If something from user's input is not clear, switch to planning mode and intervew user with questions before implementation.
- Once you finish any changes, delete old binary and rebuild binary with `go build -o ef .` and move binary to ignore directory.

## Active Technologies
- Go 1.x (existing project) + github.com/gdamore/tcell/v2 (existing) (001-search-mode)
- N/A (in-memory buffer) (001-search-mode)
- Go 1.21+ + cell/v2 (terminal rendering) (001-search-mode)
- In-memory (file content in Buffer.Lines) (001-search-mode)
- Go 1.21+ (existing project) + github.com/gdamore/tcell/v2 (existing) (002-autocomplete)
- N/A (in-memory buffer - `Buffer.Lines`) (002-autocomplete)
- Go 1.21+ + github.com/gdamore/tcell/v2 (existing) (003-marks-navigation)
- N/A (in-memory per-buffer marks, non-persistent) (003-marks-navigation)
- N/A (in-memory global registry) (004-global-marks)

## Recent Changes
- 001-search-mode: Added Go 1.x (existing project) + github.com/gdamore/tcell/v2 (existing)
