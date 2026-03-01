# Quickstart: Simplify Indentation Logic

## Build & Test

```bash
# Build
go build -o ef . && mv ef ignore/

# Run tests
go test ./editor/

# Manual test: paste preserves indentation
# 1. Open a file: ./ignore/ef somefile.go
# 2. yy to yank a line, p to paste — indentation should be verbatim
# 3. dd a few lines, P to paste — indentation should be verbatim
# 4. Enter insert mode, type multiple lines, Escape — no retroactive reindent
# 5. Press Enter after a line ending with { — new line should be indented (auto-indent preserved)
# 6. Test with AutoIndentation disabled in config — Enter should produce column 0
```

## Key Files

| File | Action |
|------|--------|
| `editor/indent.go` | Remove `ReindentLines`, `reindentPastedRange`, `endInsertSession`, `reindentInsertSession`, `GetPrevNonEmptyLineIndent` |
| `editor/editor.go` | Remove `ReindentEvent` handler, remove reindent posting from `scheduleAutoSave`, remove `endInsertSession` call from F10, remove `insertStartRow`/`insertStartCol` fields |
| `editor/editor_insert.go` | Remove `ReindentEvent` type, remove `startInsertSession`, remove `endInsertSession` call on Escape |
| `editor/editor_normal.go` | Remove `reindentPastedRange` calls from `p`/`P` handlers |
| `editor/editor_visual.go` | Remove `reindentPastedRange` call from visual paste |
