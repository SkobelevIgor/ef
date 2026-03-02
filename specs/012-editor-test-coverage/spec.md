# Feature Specification: Editor Package Test Coverage & Interface Refactoring

**Feature Branch**: `012-editor-test-coverage`
**Created**: 2026-03-01
**Status**: Draft
**Input**: User description: "Improve testing coverage. Current unit test coverage of editor package is about 5%, which is critically low. We need to improve this value to at least 80%. Plan refactoring and tests adding. Editor package almost not using interfaces, and instead violates Dependency Inversion principle. Plan code base changes to use interfaces as much as possible."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Developer Can Run Tests with Confidence (Priority: P1)

As a developer working on the editor, I want comprehensive unit tests covering at least 80% of the editor package so that I can make changes without fear of regressions.

**Why this priority**: Without sufficient test coverage, every code change risks introducing bugs. This is the foundational goal — all other stories exist to enable this one.

**Independent Test**: Run `go test ./editor/ -cover` and verify coverage is at or above 80%.

**Acceptance Scenarios**:

1. **Given** the editor package codebase, **When** I run `go test ./editor/ -cover`, **Then** the reported coverage is at least 80%
2. **Given** existing editor functionality, **When** I run the full test suite, **Then** all tests pass and no existing behavior is broken
3. **Given** a test failure, **When** I read the test output, **Then** the failure message clearly identifies what behavior broke and where

---

### User Story 2 - Developer Can Test Editor Logic Without a Terminal (Priority: P1)

As a developer, I want the editor's core logic decoupled from the terminal rendering layer so that I can write fast, reliable unit tests that don't require a real terminal screen.

**Why this priority**: The biggest blocker to testability is the direct dependency on concrete `Screen` (which wraps `tcell.Screen`). Without this decoupling, meaningful editor tests are impossible.

**Independent Test**: Instantiate an Editor with a mock screen implementation and execute key-handling logic without any terminal dependency.

**Acceptance Scenarios**:

1. **Given** a mock screen implementation, **When** I create an Editor instance, **Then** the editor initializes and operates without a real terminal
2. **Given** an editor with a mock screen, **When** I simulate key presses, **Then** the editor processes them identically to how it would with a real screen
3. **Given** a mock screen, **When** the editor renders, **Then** I can inspect the rendered output programmatically in tests

---

### User Story 3 - Developer Can Test File Watching Without the Filesystem (Priority: P2)

As a developer, I want the file watching mechanism abstracted behind an interface so that I can test external-change detection without relying on real filesystem events.

**Why this priority**: File watching is an I/O-bound concern that currently couples to both the filesystem and the Screen. Decoupling it enables isolated testing and removes a hard-to-test dependency.

**Independent Test**: Create an Editor with a mock file watcher, simulate a file-changed event, and verify the editor reloads the buffer.

**Acceptance Scenarios**:

1. **Given** a mock file watcher, **When** I simulate a file-changed event, **Then** the editor detects it and reloads the affected buffer
2. **Given** a mock file watcher, **When** no events occur, **Then** the editor continues operating normally

---

### User Story 4 - Developer Can Test Configuration Handling Independently (Priority: P2)

As a developer, I want configuration access abstracted behind an interface so that I can test editor behavior under different configurations without manipulating config files.

**Why this priority**: Configuration affects many code paths (key mappings, file type settings, indentation). Mocking it enables thorough testing of configuration-dependent behavior.

**Independent Test**: Create an Editor with a mock config provider returning custom settings, and verify the editor respects those settings.

**Acceptance Scenarios**:

1. **Given** a mock config returning custom key mappings, **When** the editor processes a mapped key, **Then** it executes the mapped action
2. **Given** a mock config with specific file type settings, **When** a file of that type is opened, **Then** the editor applies the correct settings

---

### User Story 5 - Existing Interfaces Are Preserved and Extended (Priority: P3)

As a developer, I want the existing well-designed interfaces (`SyntaxHighlighter`, `Formatter`) preserved while new interfaces are introduced for components that currently use concrete types.

**Why this priority**: The codebase already has some good abstractions. The refactoring must build on these strengths, not break them.

**Independent Test**: Verify that existing syntax highlighting and formatting tests continue to pass after refactoring.

**Acceptance Scenarios**:

1. **Given** existing `SyntaxHighlighter` and `Formatter` interfaces, **When** the refactoring is complete, **Then** all existing tests using these interfaces still pass
2. **Given** new interfaces introduced during refactoring, **When** I examine the Editor struct, **Then** it depends on interfaces rather than concrete types for Screen, FileWatcher, Config, and FileTypeRegistry

---

### Edge Cases

- What happens when a mock implementation returns unexpected values (nil, empty, errors)?
- How does the system handle tests that run in parallel modifying shared state?
- What happens when Buffer operations are tested with empty files, single-line files, and very large files?
- How does undo/redo testing work when History holds a direct pointer to a Buffer?

## Requirements *(mandatory)*

### Functional Requirements

#### Interface Extraction

- **FR-001**: System MUST define a `ScreenRenderer` interface that abstracts all screen operations the Editor depends on (rendering, event polling, sizing, lifecycle)
- **FR-002**: System MUST define an `EventPoster` interface that decouples the FileWatcher from the concrete Screen for posting events
- **FR-003**: System MUST define a `FileWatcher` interface that abstracts filesystem monitoring so the Editor does not depend on the concrete FileWatcher struct
- **FR-004**: System MUST define a `ConfigProvider` interface that abstracts configuration access so Editor does not depend on the concrete Config struct
- **FR-005**: System MUST define a `FileTypeDetector` interface that abstracts file type detection and registry lookup

#### Refactoring

- **FR-006**: The Editor struct MUST depend on the new interfaces (FR-001 through FR-005) instead of concrete types
- **FR-007**: Existing concrete implementations MUST implement the new interfaces without changing their external behavior
- **FR-008**: The refactoring MUST NOT change any user-facing behavior of the editor
- **FR-009**: The History struct MUST be decoupled from direct Buffer pointer storage where it creates testing difficulties
- **FR-010**: All new interfaces MUST be defined in the `editor` package alongside the types that implement them

#### Testing

- **FR-011**: System MUST use the `gomock` library (`go.uber.org/mock/gomock`) to generate mock implementations of all new interfaces for use in tests
- **FR-011a**: Mock generation MUST use `go generate` with `//go:generate mockgen` directives so mocks stay in sync with interface changes
- **FR-012**: System MUST have unit tests covering at least 80% of the `editor` package code
- **FR-013**: Unit tests MUST cover all mode handlers (Normal, Insert, Visual, Search)
- **FR-014**: Unit tests MUST cover Buffer operations (load, save, modify, undo/redo)
- **FR-015**: Unit tests MUST cover Pane operations (cursor movement, scrolling, selection)
- **FR-016**: Unit tests MUST cover History (undo stack, redo stack, session management)
- **FR-017**: Unit tests MUST cover autocomplete logic
- **FR-018**: Unit tests MUST cover search and replace logic
- **FR-019**: Tests MUST run without requiring a real terminal, filesystem watcher, or config files
- **FR-020**: All existing tests MUST continue to pass after refactoring

### Key Entities

- **Interface Contracts**: The new interfaces (`ScreenRenderer`, `EventPoster`, `FileWatcher`, `ConfigProvider`, `FileTypeDetector`) that define the boundaries between components
- **Mock Implementations**: `gomock`-generated test doubles for each interface, generated via `go generate` and stored in `editor/mocks/`
- **Editor**: The high-level orchestrator that will depend on interfaces instead of concrete types after refactoring

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Editor package unit test coverage reaches at least 80% as reported by `go test -cover`
- **SC-002**: All tests pass without requiring a real terminal or screen
- **SC-003**: All tests pass without requiring real filesystem monitoring
- **SC-004**: The Editor struct has zero direct dependencies on concrete I/O types (Screen, FileWatcher)
- **SC-005**: All existing editor functionality works identically after refactoring (zero behavior regressions)
- **SC-006**: Test suite completes in under 10 seconds on a standard development machine
- **SC-007**: At least 5 new interfaces are introduced to replace concrete dependencies in the Editor struct

## Assumptions

- The refactoring is limited to the `editor` package; other packages are out of scope
- Mock implementations are generated by `gomock` (`go.uber.org/mock/gomock`) and live in a `mocks/` subdirectory within the `editor` package
- The `tcell.Screen` dependency inside the concrete `Screen` struct is acceptable — the abstraction boundary is at the `ScreenRenderer` interface level
- Performance characteristics of the editor remain unchanged; interfaces add negligible overhead in Go
- The existing `SyntaxHighlighter` and `Formatter` interfaces are well-designed and do not need changes
- Buffer and Pane structs are data-centric and do not need to be abstracted behind interfaces themselves
- The 80% coverage target is measured by line coverage (`go test -cover` default)

## Scope & Boundaries

### In Scope

- Adding `go.uber.org/mock/gomock` as a project dependency
- Extracting interfaces from concrete dependencies in the `editor` package
- Refactoring the Editor struct to depend on interfaces
- Writing comprehensive unit tests for all editor package components using `gomock`
- Generating mock implementations via `mockgen` with `//go:generate` directives

### Out of Scope

- Integration tests or end-to-end tests
- Changes to packages outside `editor/`
- Persistent test fixtures or test data files
- Performance optimization
- Adding new editor features
