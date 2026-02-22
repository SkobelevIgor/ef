# Tasks: E2E Test Runner

**Input**: Design documents from `/specs/007-e2e-runner/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md

**Tests**: Not requested — no test tasks included.

**Organization**: Tasks are grouped by user story. US3 (test isolation) is foundational since both US1 and US2 depend on it.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Script skeleton with argument parsing and path resolution

- [x] T001 Implement script skeleton with shebang, imports, and main guard in tests/e2e
- [x] T002 Implement argument parsing for `run <name> <ef-path>` and `run-all <ef-path>` commands with usage message in tests/e2e
- [x] T003 Implement ef executable path validation (check file exists) in tests/e2e

---

## Phase 2: Foundational — Test Isolation Engine (US3)

**Purpose**: Core isolation logic that MUST be complete before US1/US2 can work

**Goal**: Each test case runs in an isolated temp directory with full file copy and guaranteed cleanup

**Independent Test**: After running any test, verify no temp directories remain and `tests/cases/` is unchanged

- [x] T004 [US3] Implement temp directory creation with `<test-case-name>-<random>` prefix using tempfile.mkdtemp in tests/e2e
- [x] T005 [US3] Implement test case directory copy to temp dir using shutil.copytree in tests/e2e
- [x] T006 [US3] Implement CWD switch to temp dir before execution and restore original CWD in finally block in tests/e2e
- [x] T007 [US3] Implement temp directory cleanup with shutil.rmtree in finally block (always runs on pass or fail) in tests/e2e
- [x] T008 [US3] Implement dynamic scenario.py loading via importlib.util.spec_from_file_location in tests/e2e
- [x] T009 [US3] Implement run() function detection with hasattr check and error if missing in tests/e2e

**Checkpoint**: Isolation engine complete — a single test case can be loaded, executed in temp dir, and cleaned up

---

## Phase 3: User Story 1 — Run a Single Test Case (Priority: P1) 🎯 MVP

**Goal**: Developer can run `./tests/e2e run <name> <ef-path>` and see OK or error

**Independent Test**: Run `./tests/e2e run basic-insert-safe-and-exit ./ignore/ef` and verify it prints "OK"

### Implementation for User Story 1

- [x] T010 [US1] Implement `run_single_test(name, ef_path)` function that uses isolation engine (T004-T009) to execute one test in tests/e2e
- [x] T011 [US1] Implement single-test output: print "OK" on success, print error message on exception in tests/e2e
- [x] T012 [US1] Implement single-test exit codes: 0 on success, 1 on failure in tests/e2e
- [x] T013 [US1] Implement error handling for missing test case directory (test name not found in tests/cases/) in tests/e2e
- [x] T014 [US1] Implement error handling for missing scenario.py in test case directory in tests/e2e
- [x] T015 [US1] Wire `run` command in argument parser to call run_single_test in tests/e2e

**Checkpoint**: `./tests/e2e run basic-insert-safe-and-exit ./ignore/ef` prints "OK" — MVP complete

---

## Phase 4: User Story 2 — Run All Test Cases (Priority: P1)

**Goal**: Developer can run `./tests/e2e run-all <ef-path>` and see line-by-line results with summary

**Independent Test**: Run `./tests/e2e run-all ./ignore/ef` with multiple test cases and verify each shows pass/fail line plus summary count

### Implementation for User Story 2

- [x] T016 [US2] Implement test case discovery: list sorted subdirectories of tests/cases/ in tests/e2e
- [x] T017 [US2] Implement `run_all_tests(ef_path)` function that iterates discovered test cases and calls isolation engine in tests/e2e
- [x] T018 [US2] Implement run-all output format: `<test-name>: OK` or `<test-name>: FAIL - <error>` per test in tests/e2e
- [x] T019 [US2] Implement continue-on-failure: catch exceptions per test, track pass/fail counts, don't stop on first failure in tests/e2e
- [x] T020 [US2] Implement summary line: print `N/M passed` after all tests complete in tests/e2e
- [x] T021 [US2] Implement run-all exit code: 0 if all passed, 1 if any failed in tests/e2e
- [x] T022 [US2] Implement empty test suite handling: print message when no test case directories found in tests/e2e
- [x] T023 [US2] Wire `run-all` command in argument parser to call run_all_tests in tests/e2e

**Checkpoint**: `./tests/e2e run-all ./ignore/ef` discovers all cases, runs each, prints results and summary

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and edge case handling

- [x] T024 Ensure script is executable (chmod +x) on tests/e2e
- [x] T025 Manual validation: run `./tests/e2e run basic-insert-safe-and-exit ./ignore/ef` and confirm "OK"
- [x] T026 Manual validation: run `./tests/e2e run-all ./ignore/ef` and confirm summary output
- [x] T027 Manual validation: run `./tests/e2e run nonexistent ./ignore/ef` and confirm error message
- [x] T028 Manual validation: run `./tests/e2e` with no args and confirm usage message

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational/US3 (Phase 2)**: Depends on Setup — BLOCKS US1 and US2
- **US1 (Phase 3)**: Depends on Foundational (Phase 2)
- **US2 (Phase 4)**: Depends on Foundational (Phase 2), can run in parallel with US1
- **Polish (Phase 5)**: Depends on US1 and US2 completion

### User Story Dependencies

- **US3 (Test Isolation)**: Foundational — must complete first (used by both US1 and US2)
- **US1 (Single Test)**: Depends on US3 only — no dependency on US2
- **US2 (Run All)**: Depends on US3 only — no dependency on US1 (but reuses same isolation engine)

### Within Each User Story

- Core function before output formatting
- Output formatting before exit code logic
- Error handling after happy path
- Wiring to argument parser last

### Parallel Opportunities

- T004-T009 (US3 isolation tasks) are sequential within the same file but logically buildable as one function
- US1 (Phase 3) and US2 (Phase 4) can theoretically run in parallel since they're independent functions, but both modify the same file
- T025-T028 (manual validations) are independent and can run in parallel

---

## Implementation Strategy

### MVP First (US3 + US1)

1. Complete Phase 1: Setup (T001-T003)
2. Complete Phase 2: US3 Isolation Engine (T004-T009)
3. Complete Phase 3: US1 Single Test (T010-T015)
4. **STOP and VALIDATE**: `./tests/e2e run basic-insert-safe-and-exit ./ignore/ef` prints "OK"
5. This is a usable MVP — developer can already run individual tests

### Full Delivery

1. MVP above
2. Add Phase 4: US2 Run All (T016-T023)
3. Complete Phase 5: Polish (T024-T028)
4. All acceptance scenarios from spec.md satisfied

---

## Notes

- All tasks modify the same file (`tests/e2e`) — true parallelism is limited
- No test tasks included (not requested in spec)
- Total: 28 tasks across 5 phases
- The existing `basic-insert-safe-and-exit` test case serves as the validation fixture
- Build ef binary before validation: `go build -o ef . && mv ef ignore/`
