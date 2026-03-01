# Quickstart: Code Simplification

## Prerequisites

- Go 1.21+ installed
- Repository cloned and on `009-code-simplification` branch

## Verify Current State

```bash
# Build and confirm it works before any changes
go build -o ef . && mv ef ignore/

# Run existing tests
cd tests/e2e && python3 run_tests.py
```

## Implementation Order

Each step should compile and pass tests before moving to the next:

1. Create `editor/utils.go` with shared utilities
2. Update callers to use consolidated visual column functions
3. Adopt `NormalizeRange` in GetSelection/DeleteRange/GetRange
4. Refactor whitespace helpers to use `isWhitespace` predicate
5. Adopt `ClampPosition` in marks and undo
6. Remove dead code (`firstNonWhitespace`)
7. Extract indentation logic to `editor/indent.go`
8. Split `performPaste` into sub-functions
9. Extract search navigation helper
10. Implement inputState auto-reset wrapper
11. Complete key mapping timeout
12. Add undo/redo mirror documentation

## Verify After Each Step

```bash
go build -o ef . && mv ef ignore/
cd tests/e2e && python3 run_tests.py
```
