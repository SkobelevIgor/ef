# Research: Editor Package Test Coverage & Interface Refactoring

**Feature**: 012-editor-test-coverage
**Date**: 2026-03-01

## R1: Interface Extraction Strategy for Go Terminal Editor

**Decision**: Extract 5 interfaces into a single `interfaces.go` file in the `editor` package. Each interface is minimal (Go idiom: small interfaces).

**Rationale**: Go's implicit interface satisfaction means existing concrete types automatically implement the new interfaces without any `implements` keyword. This makes the refactoring safe — define the interface, change the struct field type, and the compiler verifies correctness.

**Alternatives considered**:
- One interface per file: Rejected — adds file proliferation for small interfaces; a single file is easier to navigate
- Interfaces in a separate package: Rejected — creates import cycles; Go convention is to define interfaces where they're consumed

## R2: gomock vs Alternatives for Mock Generation

**Decision**: Use `go.uber.org/mock/gomock` (v0.5.x) with `mockgen` code generation.

**Rationale**:
- `gomock` is the de facto standard for Go mock generation
- `mockgen` supports both source and reflect modes — source mode (`-source`) is simpler for our use case
- Generated mocks auto-update via `go generate`, preventing drift
- Maintained by uber (forked from original Google repo which is archived)

**Alternatives considered**:
- `github.com/stretchr/testify/mock`: Requires hand-writing mock structs; doesn't auto-generate from interfaces
- `github.com/vektra/mockery`: Good but adds another dependency ecosystem; gomock is more widely adopted
- Hand-written mocks: Doesn't scale to 5+ interfaces; no type-safety guarantees on mock method signatures

## R3: Editor Constructor Refactoring Approach

**Decision**: Introduce a `NewEditorWithDeps` constructor that accepts interfaces, keep `New` as the production constructor that creates concrete types and delegates.

**Rationale**:
- `New(fileInfos, splitMode)` remains the public API for `main.go` — zero changes to caller
- `NewEditorWithDeps(deps EditorDeps)` accepts a deps struct containing all interfaces — used by tests
- This avoids breaking the existing API while enabling full testability

**Alternatives considered**:
- Functional options pattern (`WithScreen(s)`, `WithConfig(c)`): More idiomatic but verbose for 5+ dependencies; deps struct is cleaner
- Modify `New` signature directly: Would break `main.go` and violate "no changes outside editor package" spirit
- Builder pattern: Over-engineered for a constructor with fixed dependencies

## R4: History-Buffer Coupling Resolution

**Decision**: Keep `*Buffer` pointer in History but make History accept Buffer via the constructor. Tests create real Buffer instances (they're data-only structs with no I/O in constructors).

**Rationale**: Buffer is a data model, not an I/O boundary. Creating `Buffer{Lines: [][]rune{...}}` in tests is trivial and doesn't require mocking. The coupling issue is not that History references Buffer, but that creating an Editor (which creates History) requires Screen/FileWatcher. Once Editor can be created with mocks, History testing is unblocked.

**Alternatives considered**:
- Abstract Buffer behind interface: Over-engineering — Buffer is a value-like type with 200+ method calls throughout the codebase
- Store buffer path instead of pointer: Would require lookup mechanism, adding complexity for no test benefit

## R5: Coverage Strategy — What to Test

**Decision**: Prioritize testing by code volume and criticality:

| Priority | Component | Files | Est. Coverage Impact |
|----------|-----------|-------|---------------------|
| P1 | Normal mode handlers | editor_normal.go (9.8K) | ~15% |
| P2 | Screen rendering logic | screen.go (17K) | ~12% (partial — render helpers testable, tcell calls mocked) |
| P3 | Buffer operations | buffer.go (15K) | ~10% |
| P4 | Insert mode | editor_insert.go (3.8K) | ~5% |
| P5 | Visual mode | editor_visual.go (4.5K) | ~5% |
| P6 | Search mode | editor_search.go (7.4K) | ~8% |
| P7 | Paste operations | editor_paste.go (3.9K) | ~4% |
| P8 | Undo/redo | editor_undo.go (4.6K) | ~5% |
| P9 | Pane operations | pane.go (10K) | ~8% |
| P10 | Autocomplete, Config, FileType | remaining | ~8% |

**Rationale**: Normal mode is the largest mode handler and most complex. Screen rendering is the largest file but partially requires mocking. Buffer operations are foundational and testable without mocks.

## R6: Test Helper Pattern

**Decision**: Create a `testutil_test.go` file with helper functions for creating test editors, buffers, and panes.

**Rationale**: Most tests need an Editor with mocks wired up. A helper like `newTestEditor(t, opts...)` reduces boilerplate. Buffer and Pane helpers (`newTestBuffer(lines...)`, `newTestPane(lines...)`) simplify data setup.

**Alternatives considered**:
- Test fixtures in external files: Rejected — adds I/O to tests, harder to maintain
- Table-driven tests only: Not sufficient — need editor instance setup helpers alongside table-driven test cases
