# Tasks: Editor Package Test Coverage & Interface Refactoring

**Input**: Design documents from `/specs/012-editor-test-coverage/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Included — the entire feature is about testing. Tests are written after interface refactoring enables testability.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add gomock dependency and install mockgen tool

- [x] T001 Add `go.uber.org/mock/gomock` dependency by running `go get go.uber.org/mock/gomock@latest` and `go install go.uber.org/mock/mockgen@latest`
- [x] T002 Verify mockgen is available by running `mockgen --version` and that `go.sum` is updated

**Checkpoint**: gomock dependency available, mockgen CLI installed

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Extract interfaces, refactor Editor struct to use them, generate mocks, create test helpers. This phase enables ALL test writing in subsequent phases.

**⚠️ CRITICAL**: No user story test work can begin until this phase is complete

- [x] T003 Create `editor/interfaces.go` with all 5 interface definitions: `ScreenRenderer` (7 methods: Render, PollEvent, PostEvent, Size, Close, Suspend, Resume), `EventPoster` (1 method: PostEvent), `FileWatcherService` (3 methods: Watch, Close, UpdateModTime), `ConfigProvider` (2 methods: GetFileTypeConfig, GetKeyMappings), `FileTypeDetector` (4 methods: DetectFileType, GetHighlighter, GetFormatter, GetConfig). Add `//go:generate mockgen -source=interfaces.go -destination=mocks/mock_interfaces.go -package=mocks` directive at top
- [x] T004 Add `GetKeyMappings() map[string]string` method to `Config` struct in `editor/config.go` so it satisfies the `ConfigProvider` interface. This method should return `c.KeyMappings` (the existing field)
- [x] T005 Update `Editor` struct in `editor/editor.go`: change field `screen *Screen` to `screen ScreenRenderer`, change `fileWatcher *FileWatcher` to `fileWatcher FileWatcherService`, change `config *Config` to `config ConfigProvider`, change `fileTypeRegistry *FileTypeRegistry` to `fileTypeRegistry FileTypeDetector`. Add `EditorDeps` struct and `NewEditorWithDeps(deps EditorDeps) *Editor` constructor. Keep existing `New()` function unchanged (it creates concrete types and passes them to `NewEditorWithDeps`)
- [x] T006 Update `NewFileWatcher` in `editor/watcher.go`: change parameter from `screen *Screen` to `screen EventPoster` so FileWatcher depends on the minimal interface instead of concrete Screen
- [x] T007 Update all call sites in `editor/` that access `e.config` or `e.fileTypeRegistry` fields directly (not via interface methods) — replace direct field access with interface method calls where needed. Scan `editor/editor.go`, `editor/editor_normal.go`, `editor/buffer.go`, `editor/editor_autocomplete.go` for `e.config.KeyMappings` (change to `e.config.GetKeyMappings()`), `e.config.GetFileTypeConfig()` (already a method — OK), `e.fileTypeRegistry.DetectFileType()` (already a method — OK)
- [x] T008 Run `go generate ./editor/...` to generate mock implementations into `editor/mocks/mock_interfaces.go`. Verify the file is created and contains `MockScreenRenderer`, `MockEventPoster`, `MockFileWatcherService`, `MockConfigProvider`, `MockFileTypeDetector`
- [x] T009 Run `go build ./...` to verify the entire project compiles with the refactored interfaces. Fix any compilation errors
- [x] T010 Create `editor/testutil_test.go` with test helper functions: `newTestBuffer(lines ...string) *Buffer` (creates Buffer with given lines as `[][]rune`), `newTestPane(lines ...string) *Pane` (creates Pane wrapping a test buffer), `newTestEditor(t *testing.T) (*Editor, *gomock.Controller, *mocks.MockScreenRenderer, *mocks.MockFileWatcherService, *mocks.MockConfigProvider, *mocks.MockFileTypeDetector)` (creates Editor with all mocks wired via `NewEditorWithDeps`, sets up default mock expectations for `Size()` returning 80x24, `GetKeyMappings()` returning empty map, `GetFileTypeConfig()` returning zero value)

**Checkpoint**: All interfaces defined, Editor uses interfaces, mocks generated, project compiles, test helpers ready. User story implementation can now begin.

---

## Phase 3: User Story 2 - Developer Can Test Editor Logic Without a Terminal (Priority: P1) 🎯 MVP

**Goal**: Prove the Editor can be instantiated and operated with mock screen — no real terminal needed

**Independent Test**: `go test ./editor/ -run TestEditor -v` passes without a terminal

### Tests for User Story 2

- [x] T011 [US2] Create `editor/editor_test.go` with tests: `TestNewEditorWithDeps` (verify Editor creates successfully with all mocks), `TestEditorRun_QuitKey` (verify pressing 'q' or Ctrl+Q in normal mode exits the run loop — mock `PollEvent` to return quit key event, mock `Render` to be called), `TestEditorModeTransitions` (verify Esc→Normal, i→Insert, v→Visual mode transitions work with mocked screen)
- [x] T012 [P] [US2] Create `editor/editor_normal_test.go` with tests for normal mode key handlers: `TestNormalMode_CursorMovement` (h/j/k/l move cursor correctly on a test buffer), `TestNormalMode_WordMotions` (w/b/e word navigation), `TestNormalMode_LineMotions` (0/$/^), `TestNormalMode_DeleteChar` (x deletes character), `TestNormalMode_DeleteLine` (dd deletes line), `TestNormalMode_YankLine` (yy copies line to clipboard), `TestNormalMode_PutAfter` (p pastes clipboard). Use `newTestEditor` helper with mock screen expectations
- [x] T013 [P] [US2] Create `editor/editor_insert_test.go` with tests: `TestInsertMode_TypeCharacter` (typing inserts character at cursor), `TestInsertMode_Backspace` (backspace deletes previous char), `TestInsertMode_Enter` (enter splits line), `TestInsertMode_Tab` (tab inserts indentation), `TestInsertMode_EscReturnsToNormal` (Esc transitions back to normal mode)
- [x] T014 [P] [US2] Create `editor/editor_visual_test.go` with tests: `TestVisualMode_SelectionExpand` (movement keys expand selection), `TestVisualMode_YankSelection` (y copies selected text), `TestVisualMode_DeleteSelection` (d/x deletes selected text), `TestVisualMode_EscCancels` (Esc cancels selection and returns to normal)

**Checkpoint**: Editor fully testable without terminal. Core mode handlers have unit tests.

---

## Phase 4: User Story 1 - Developer Can Run Tests with Confidence (Priority: P1)

**Goal**: Comprehensive unit tests covering all editor components to reach 80%+ line coverage

**Independent Test**: `go test ./editor/ -cover` reports >= 80%

### Tests for User Story 1

- [ ] T015 [P] [US1] Expand `editor/buffer_test.go` with comprehensive tests: `TestBufferInsertChar`, `TestBufferDeleteChar`, `TestBufferInsertLine`, `TestBufferDeleteLine`, `TestBufferSplitLine`, `TestBufferJoinLines`, `TestBufferSave` (use temp file), `TestBufferLoad` (use temp file), `TestBuffer_EmptyFile`, `TestBuffer_SingleLine`, `TestBuffer_LargeFile` (1000+ lines), `TestBufferModifiedFlag`, `TestBufferGetLine`, `TestBufferLineCount`
- [ ] T016 [P] [US1] Create `editor/pane_test.go` with tests: `TestNewPane`, `TestPaneCursorMovement` (move cursor within bounds), `TestPaneCursorClamp` (cursor clamped to buffer bounds), `TestPaneScrolling` (scroll offset updates when cursor moves past visible area), `TestPaneSelection` (selection start/end tracking), `TestPaneAtLine` (NewPaneAtLine positions correctly), `TestPaneVisibleLines` (correct range of visible lines based on scroll offset and height)
- [ ] T017 [P] [US1] Expand `editor/history_test.go` with tests: `TestHistoryPushAndUndo` (push change, undo restores), `TestHistoryRedo` (undo then redo restores), `TestHistoryMaxSize` (oldest entries dropped when exceeding max), `TestHistorySession` (begin/end session groups changes), `TestHistorySessionUndo` (undo reverts entire session), `TestHistoryEmpty` (undo/redo on empty stack is no-op), `TestHistoryRedoClearedOnNewChange` (new change after undo clears redo stack)
- [ ] T018 [P] [US1] Expand `editor/search_test.go` with tests: `TestSearchForward`, `TestSearchBackward`, `TestSearchWrapAround`, `TestSearchNoMatch`, `TestSearchReplace`, `TestSearchReplaceAll`, `TestSearchCaseSensitivity`, `TestSearchRegexPatterns`. Also create `editor/editor_search_test.go` with Editor-level search mode tests: `TestSearchMode_EnterAndExit`, `TestSearchMode_TypeQuery`, `TestSearchMode_NextMatch`, `TestSearchMode_PrevMatch`
- [ ] T019 [P] [US1] Expand `editor/autocomplete_test.go` with tests: `TestAutocompleteTrigger` (trigger on typing after word boundary), `TestAutocompleteFilter` (filter suggestions as user types), `TestAutocompleteSelect` (Tab/Enter selects suggestion), `TestAutocompleteCancel` (Esc cancels autocomplete), `TestAutocompleteScrolling` (navigate long suggestion list). Also create `editor/editor_autocomplete_test.go` with Editor-level tests: `TestAutocompleteMode_Trigger`, `TestAutocompleteMode_Accept`, `TestAutocompleteMode_Dismiss`
- [ ] T020 [P] [US1] Create `editor/editor_paste_test.go` with tests: `TestPaste_AfterYankLine` (yy then p pastes line below), `TestPaste_AfterYankChars` (visual yank then p pastes inline), `TestPaste_AfterDelete` (dd then p pastes deleted line), `TestPaste_MultipleLines` (yank multiple lines then paste), `TestPaste_IndentPreservation` (pasted content preserves indentation)
- [ ] T021 [P] [US1] Create `editor/editor_undo_test.go` with tests: `TestUndo_SingleChange`, `TestUndo_MultipleChanges`, `TestRedo_AfterUndo`, `TestUndo_InsertSession` (undo entire insert session as one unit), `TestUndo_DeleteRestore` (undo delete restores text), `TestUndo_AfterSave` (undo still works after save)
- [ ] T022 [P] [US1] Expand `editor/screen_test.go` with tests for testable helper functions: `TestLineNumberWidth` (existing — verify), `TestScreenSize` (mock tcell.Screen, verify Size returns correct values). Test render helper logic where possible without full tcell (e.g., line number width calculation, separator positioning calculations)
- [ ] T023 [P] [US1] Create `editor/editor_marks_test.go` with tests: `TestSetMark` (ma sets mark a at cursor), `TestJumpToMark` ('a jumps to mark a), `TestMarkNotSet` (jumping to unset mark is no-op), `TestGlobalMark` (mA sets global mark), `TestGlobalMarkCrossBuffer` (global mark navigates to different buffer)
- [ ] T024 [P] [US1] Create `editor/config_test.go` with tests: `TestLoadConfigDefault` (missing config file returns sensible defaults), `TestGetFileTypeConfig` (returns correct config for known type), `TestGetFileTypeConfigUnknown` (returns default for unknown type), `TestGetKeyMappings` (returns key mappings from config)
- [ ] T025 [P] [US1] Create `editor/filetype_test.go` with tests: `TestDetectFileType_Go` (.go → "go"), `TestDetectFileType_Python` (.py → "python"), `TestDetectFileType_Unknown` (unknown ext → ""), `TestGetHighlighter` (returns non-nil for known type), `TestGetFormatter` (returns formatter for known type), `TestGetConfig` (returns type-specific config)

**Checkpoint**: Coverage should be at or above 80%. Run `go test ./editor/ -cover` to verify.

---

## Phase 5: User Story 3 - Developer Can Test File Watching Without the Filesystem (Priority: P2)

**Goal**: Prove FileWatcher behavior can be tested with mock EventPoster, no real filesystem monitoring

**Independent Test**: `go test ./editor/ -run TestFileWatch -v` passes without fsnotify

### Tests for User Story 3

- [ ] T026 [US3] Create tests in `editor/editor_test.go` (append to existing): `TestFileWatch_ExternalChange` (use mock FileWatcherService, verify Editor handles file-changed event by reloading buffer), `TestFileWatch_UpdateModTime` (after save, verify UpdateModTime is called on mock), `TestFileWatch_WatchCalledOnOpen` (verify Watch is called for each buffer filename when Editor is created)

**Checkpoint**: FileWatcher fully mockable and tested

---

## Phase 6: User Story 4 - Developer Can Test Configuration Handling Independently (Priority: P2)

**Goal**: Prove configuration-dependent behavior can be tested with mock ConfigProvider

**Independent Test**: `go test ./editor/ -run TestConfig -v` passes without .efconfig file

### Tests for User Story 4

- [ ] T027 [US4] Add tests in `editor/editor_test.go` (append to existing): `TestConfigKeyMappings` (mock ConfigProvider returns custom key mappings, verify Editor executes mapped action), `TestConfigFileTypeSettings` (mock ConfigProvider returns custom file type config, verify Editor applies indentation settings). Also add `TestFileTypeDetector_Mock` (mock FileTypeDetector returns specific highlighter/formatter, verify buffer uses them)

**Checkpoint**: Config fully mockable, config-dependent code paths tested

---

## Phase 7: User Story 5 - Existing Interfaces Preserved and Extended (Priority: P3)

**Goal**: Verify existing SyntaxHighlighter and Formatter interfaces still work, all existing tests pass

**Independent Test**: `go test ./editor/ -run 'TestExtractWords|TestFindSuggestions|TestCopyLines|TestNewHistory|TestFindFirstMatch|TestLineNumberWidth' -v` — all pre-existing tests pass

### Verification for User Story 5

- [ ] T028 [US5] Run all existing tests (`go test ./editor/ -v`) and verify zero failures. Document in a comment at top of `editor/interfaces.go` that `SyntaxHighlighter` and `Formatter` interfaces (in `editor/syntax.go` and `editor/formatter.go`) are preserved and that new interfaces follow the same Go idiom of small, focused interfaces
- [ ] T029 [US5] Verify Editor struct fields: confirm `screen` is `ScreenRenderer` (not `*Screen`), `fileWatcher` is `FileWatcherService` (not `*FileWatcher`), `config` is `ConfigProvider` (not `*Config`), `fileTypeRegistry` is `FileTypeDetector` (not `*FileTypeRegistry`). This is a compile-time verification — if the project builds (T009), this is satisfied

**Checkpoint**: All existing interfaces preserved, new interfaces verified, zero regressions

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final verification, coverage check, binary rebuild

- [ ] T030 Run `go test ./editor/ -cover -v` and verify coverage >= 80%. If below 80%, identify uncovered lines with `go test ./editor/ -coverprofile=coverage.out && go tool cover -func=coverage.out` and add targeted tests for the largest uncovered functions
- [ ] T031 Run `go vet ./editor/...` and fix any warnings
- [ ] T032 Delete old binary and rebuild with `go build -o ef .` and move binary to ignore directory per CLAUDE.md instructions
- [ ] T033 Run `go test ./...` to verify entire project (not just editor/) still passes

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **US2 (Phase 3)**: Depends on Phase 2 — first testability proof
- **US1 (Phase 4)**: Depends on Phase 3 (needs test helpers and mock editor working)
- **US3 (Phase 5)**: Depends on Phase 2 — can run in parallel with Phase 3/4
- **US4 (Phase 6)**: Depends on Phase 2 — can run in parallel with Phase 3/4/5
- **US5 (Phase 7)**: Depends on Phase 2 — can run in parallel with Phase 3/4/5/6
- **Polish (Phase 8)**: Depends on all user story phases being complete

### User Story Dependencies

- **US2 (P1)**: Depends on Foundational — enables all other testing
- **US1 (P1)**: Depends on US2 — needs working mock Editor to write comprehensive tests
- **US3 (P2)**: Depends on Foundational only — independent of US1/US2 tests
- **US4 (P2)**: Depends on Foundational only — independent of US1/US2/US3 tests
- **US5 (P3)**: Depends on Foundational only — verification step

### Within Each User Story

- Test files can be written in parallel (different files, no dependencies) — marked [P]
- All tests use `newTestEditor` helper from T010
- Each story's tests are independently runnable via `-run` flag

### Parallel Opportunities

- T003 and T004 can start simultaneously (different files)
- T012, T013, T014 can run in parallel (different test files)
- T015 through T025 can ALL run in parallel (each is a different test file)
- US3, US4, US5 phases can run in parallel with each other (independent concerns)

---

## Parallel Example: User Story 1 (Phase 4)

```bash
# All these test file tasks can be launched in parallel:
T015: "Expand editor/buffer_test.go"
T016: "Create editor/pane_test.go"
T017: "Expand editor/history_test.go"
T018: "Expand editor/search_test.go + editor/editor_search_test.go"
T019: "Expand editor/autocomplete_test.go + editor/editor_autocomplete_test.go"
T020: "Create editor/editor_paste_test.go"
T021: "Create editor/editor_undo_test.go"
T022: "Expand editor/screen_test.go"
T023: "Create editor/editor_marks_test.go"
T024: "Create editor/config_test.go"
T025: "Create editor/filetype_test.go"
```

---

## Implementation Strategy

### MVP First (US2 Only)

1. Complete Phase 1: Setup (T001-T002)
2. Complete Phase 2: Foundational (T003-T010)
3. Complete Phase 3: US2 (T011-T014)
4. **STOP and VALIDATE**: `go test ./editor/ -v` — Editor works without terminal
5. Check coverage — likely ~30-40% at this point

### Full Coverage (US1)

6. Complete Phase 4: US1 (T015-T025) — all test files in parallel
7. **VALIDATE**: `go test ./editor/ -cover` — should reach 80%+

### Complete Feature

8. Complete Phase 5-7: US3, US4, US5 (T026-T029) — can be parallel
9. Complete Phase 8: Polish (T030-T033) — final verification and binary rebuild

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Commit after each phase completion
- Stop at any checkpoint to validate story independently
- The foundational phase (T003-T010) is the critical path — all test writing depends on it
- T030 is the gatekeeper — if coverage < 80%, iterate with targeted tests before proceeding
