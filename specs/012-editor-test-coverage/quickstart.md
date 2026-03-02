# Quickstart: Editor Test Coverage & Interface Refactoring

**Feature**: 012-editor-test-coverage

## Prerequisites

- Go 1.21+
- `mockgen` tool: `go install go.uber.org/mock/mockgen@latest`

## Setup

```bash
# Add gomock dependency
go get go.uber.org/mock/gomock

# Generate mocks (after interfaces.go is created)
go generate ./editor/...
```

## Running Tests

```bash
# Run all editor tests with coverage
go test ./editor/ -cover -v

# Run specific test file
go test ./editor/ -run TestEditorNormalMode -v

# Generate coverage report
go test ./editor/ -coverprofile=coverage.out
go tool cover -html=coverage.out -o coverage.html
```

## Implementation Order

1. **Create `editor/interfaces.go`** — define all 5 interfaces
2. **Update `editor/editor.go`** — change Editor struct fields to interface types, add `NewEditorWithDeps`
3. **Update `editor/watcher.go`** — accept `EventPoster` instead of `*Screen`
4. **Add `GetKeyMappings()` to `editor/config.go`** — implement ConfigProvider interface
5. **Run `go generate`** — create mock implementations
6. **Create `editor/testutil_test.go`** — test helpers (`newTestEditor`, `newTestBuffer`, etc.)
7. **Write tests** — start with normal mode (biggest impact), then buffer, insert, visual, search, etc.
8. **Rebuild binary** — `go build -o ef .` and move to ignore directory

## Verification

```bash
# Check coverage target
go test ./editor/ -cover | grep -oP '\d+\.\d+%'
# Expected: >= 80.0%

# Verify no behavior regression
go build -o ef . && ./ef testfile.txt
# Manually verify: all modes work, undo/redo, search, autocomplete
```
