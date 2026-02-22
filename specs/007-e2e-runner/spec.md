# Feature Specification: E2E Test Runner

**Feature Branch**: `007-e2e-runner`
**Created**: 2026-02-22
**Status**: Draft
**Input**: User description: "E2E runner for ef validation. Python-based test runner that executes scenario-based tests against the ef binary."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Run a Single Test Case (Priority: P1)

A developer wants to run a specific e2e test case against a built ef binary to verify a particular behavior works correctly. They invoke the runner with a test case name and the path to the ef executable, and see whether the test passes or fails.

**Why this priority**: This is the core functionality — running a single test is the atomic operation everything else builds on.

**Independent Test**: Can be fully tested by running `./tests/e2e run basic-insert-safe-and-exit /path/to/ef` and verifying it prints "OK" or an error message.

**Acceptance Scenarios**:

1. **Given** a valid test case directory exists in `./tests/cases/<name>/` with a `scenario.py` containing a `run()` function, **When** the user runs `./tests/e2e run <name> <ef-path>`, **Then** the runner executes the test in an isolated temp directory and prints "OK" if no exception is raised.
2. **Given** a valid test case where `scenario.py:run()` raises an exception, **When** the user runs `./tests/e2e run <name> <ef-path>`, **Then** the runner prints the error message from the exception and exits with a non-zero exit code.
3. **Given** a test case name that does not exist in `./tests/cases/`, **When** the user runs `./tests/e2e run <name> <ef-path>`, **Then** the runner prints an error indicating the test case was not found.
4. **Given** a test case directory that has no `scenario.py` file, **When** the user runs the test, **Then** the runner stops with an error message indicating `scenario.py` is missing.
5. **Given** a test case directory that has `scenario.py` without a `run()` function, **When** the user runs the test, **Then** the runner stops with an error message indicating the `run()` method is missing.

---

### User Story 2 - Run All Test Cases (Priority: P1)

A developer wants to run all available e2e tests at once to validate the entire ef binary before a release or after changes. They use the special name `run-all` and see line-by-line results.

**Why this priority**: Running all tests is essential for CI and pre-release validation; equally important as single-test for practical use.

**Independent Test**: Can be tested by running `./tests/e2e run-all /path/to/ef` with multiple test cases in `./tests/cases/` and verifying each produces a pass/fail line.

**Acceptance Scenarios**:

1. **Given** multiple test case directories exist in `./tests/cases/`, **When** the user runs `./tests/e2e run-all <ef-path>`, **Then** the runner executes each test case and prints a line for each: `<test-name>: OK` or `<test-name>: FAIL - <error message>`.
2. **Given** all tests pass, **When** `run-all` completes, **Then** the runner prints a summary count (e.g., "3/3 passed") and exits with code 0.
3. **Given** one or more tests fail, **When** `run-all` completes, **Then** the runner prints a summary count (e.g., "2/3 passed") and exits with a non-zero exit code.
4. **Given** no test case directories exist in `./tests/cases/`, **When** the user runs `run-all`, **Then** the runner prints a message indicating no tests were found.

---

### User Story 3 - Test Isolation via Temp Directory (Priority: P1)

Each test case runs in a temporary directory so that test files don't pollute the repository or interfere with each other. The test case's directory contents are copied to a temp folder, the test runs there, and the temp folder is cleaned up afterward.

**Why this priority**: Isolation is fundamental to reliable, repeatable test execution and prevents side effects between tests.

**Independent Test**: Can be verified by checking that after a test run, no temp directories remain and the original test case directory is unchanged.

**Acceptance Scenarios**:

1. **Given** a test case directory with `scenario.py` and additional fixture files, **When** the test runs, **Then** all files from the test case directory are copied to a temp directory named `<test-case-name>-<random-hash>` and the test executes within that directory.
2. **Given** a test case has completed (pass or fail), **When** execution finishes, **Then** the temporary directory is deleted.
3. **Given** a test case's `scenario.py` modifies or creates files, **When** the test completes, **Then** the original test case directory in `./tests/cases/` remains unmodified.

---

### Edge Cases

- What happens when the ef executable path is invalid or the file doesn't exist? Runner prints an error before attempting any tests.
- What happens when a test case's `scenario.py` hangs or runs indefinitely? Runner does not handle timeouts — that is the test's responsibility (tests set their own timeouts via pexpect or similar).
- What happens when the temp directory cannot be created (e.g., disk full)? Runner prints the OS-level error and exits.
- What happens when `run-all` encounters a failing test? Runner continues executing remaining tests (does not stop on first failure).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The runner MUST be a Python script located at `./tests/e2e`.
- **FR-002**: The runner MUST support the command `run <test-case-name> <ef-executable-path>` to execute a single test.
- **FR-003**: The runner MUST support the command `run-all <ef-executable-path>` to execute all test cases found in `./tests/cases/`.
- **FR-004**: The runner MUST discover test cases as subdirectories of `./tests/cases/` (relative to the runner's location).
- **FR-005**: For each test execution, the runner MUST create a temporary directory named `<test-case-name>-<random-hash>` and copy all contents of the test case directory into it.
- **FR-006**: The runner MUST execute `scenario.py`'s `run(ef_executable_path)` function from within the temporary directory.
- **FR-007**: The runner MUST delete the temporary directory after test execution, regardless of pass or fail.
- **FR-008**: If `scenario.py` is missing from a test case directory, the runner MUST stop with an error message indicating the file is missing.
- **FR-009**: If `scenario.py` does not contain a `run()` function, the runner MUST stop with an error message indicating the function is missing.
- **FR-010**: If `run()` completes without raising an exception, the runner MUST print "OK" (for single test) or `<test-name>: OK` (for run-all).
- **FR-011**: If `run()` raises an exception, the runner MUST print the error message (for single test) or `<test-name>: FAIL - <error>` (for run-all).
- **FR-012**: The runner MUST exit with code 0 when all tests pass and non-zero when any test fails.
- **FR-013**: For `run-all`, the runner MUST continue executing remaining tests even if one fails.
- **FR-014**: For `run-all`, the runner MUST print a summary line with pass/total count after all tests complete.
- **FR-015**: The runner MUST validate that the ef executable path points to an existing file before running tests.

### Key Entities

- **Test Case**: A subdirectory in `./tests/cases/` containing a `scenario.py` file and optionally additional fixture files (text files, configs, etc.) used during the test.
- **Scenario**: A Python module (`scenario.py`) that exposes a `run(ef_executable_path)` function. The function signals success by returning normally and failure by raising an exception.
- **Runner**: The Python script at `./tests/e2e` that orchestrates test discovery, isolation, execution, and reporting.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can validate any single ef behavior by running one command with the test case name and ef path.
- **SC-002**: A developer can validate all ef behaviors in a single command and immediately see which tests pass and which fail.
- **SC-003**: Tests leave no artifacts behind — the working directory and test case source directories are clean after every run.
- **SC-004**: A failing test provides a clear, actionable error message that identifies what went wrong.
- **SC-005**: The runner correctly discovers and executes all test cases in the `./tests/cases/` directory without manual registration or configuration.

## Assumptions

- Python 3 is available on the developer's machine.
- Test authors are responsible for managing their own dependencies (e.g., pexpect) — the runner does not install packages.
- Test timeouts are the responsibility of individual test scenarios, not the runner.
- The runner script is executable (`chmod +x`) and uses a `#!/usr/bin/env python3` shebang.
- Test case directories use lowercase-with-hyphens naming convention.
