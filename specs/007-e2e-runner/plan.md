# Implementation Plan: E2E Test Runner

**Branch**: `007-e2e-runner` | **Date**: 2026-02-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/007-e2e-runner/spec.md`

## Summary

Implement a Python-based e2e test runner script (`./tests/e2e`) that discovers test cases in `./tests/cases/`, runs each in an isolated temp directory by invoking `scenario.py:run(ef_path)`, and reports pass/fail results. Supports both single-test and run-all modes.

## Technical Context

**Language/Version**: Python 3.x (script, not compiled)
**Primary Dependencies**: Python standard library only (os, sys, shutil, tempfile, importlib, uuid)
**Storage**: N/A (filesystem-only temp directories)
**Testing**: Manual validation with existing `basic-insert-safe-and-exit` test case
**Target Platform**: macOS (developer machine), portable to Linux
**Project Type**: Single script addition to existing Go project
**Performance Goals**: N/A (test runner, not performance-critical)
**Constraints**: No external Python dependencies for the runner itself; test cases may use any deps
**Scale/Scope**: Single file (`tests/e2e`), ~100-150 lines

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Applicable? | Status | Notes |
|-----------|-------------|--------|-------|
| I. Terminal Native | No | N/A | This is a test tool, not editor functionality |
| II. Vim-Style Modal Editing | No | N/A | Not editor functionality |
| III. Data Integrity | No | N/A | No user data involved |
| Code Quality: compile without errors | Yes | Pass | Python script — syntax-checked on execution |
| Code Quality: test manually | Yes | Pass | Will validate with existing test case |
| Code Quality: minimal deps | Yes | Pass | Runner uses only Python stdlib |
| Governance: rebuild binary | No | N/A | Not a Go change; but will rebuild ef binary per CLAUDE.md for testing |
| Governance: no new branch | Yes | Pass | Using existing 007-e2e-runner branch from speckit.specify |

**Gate result**: PASS — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/007-e2e-runner/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
tests/
├── e2e                  # Runner script (MODIFY - currently stub)
├── cases/
│   └── basic-insert-safe-and-exit/
│       └── scenario.py  # Existing test case (NO CHANGE)
└── requirements.txt     # Existing (NO CHANGE)
```

**Structure Decision**: Single file modification — the `tests/e2e` script already exists as a stub (`#!/usr/bin/env python3` only). The runner will be implemented entirely within this file. No new directories or files needed beyond spec artifacts.

## Design

### Runner Architecture

The script follows a simple sequential architecture:

1. **Argument parsing** — `sys.argv` (no argparse needed for 2 commands)
2. **Path resolution** — resolve `cases/` relative to script location via `__file__`
3. **Test discovery** — list subdirectories of `cases/` for `run-all`
4. **Test execution** — for each test:
   - Create temp dir: `tempfile.mkdtemp(prefix=f"{name}-")`
   - Copy case contents: `shutil.copytree` contents
   - Change CWD to temp dir
   - Dynamically import `scenario.py` via `importlib`
   - Call `run(ef_path)`
   - Cleanup temp dir in `finally` block
5. **Reporting** — print results, exit with appropriate code

### Key Implementation Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Module loading | `importlib.util.spec_from_file_location` | Load scenario.py from arbitrary path without polluting sys.modules |
| Temp dir naming | `tempfile.mkdtemp(prefix=f"{name}-")` | OS handles randomness; prefix provides readability |
| Path resolution | `os.path.dirname(os.path.abspath(__file__))` | Works regardless of CWD when script is invoked |
| run() detection | `hasattr(module, 'run')` | Simple, Pythonic check before calling |
| CWD management | `os.chdir(temp_dir)` + restore in finally | Scenario scripts can use relative paths for fixtures |

## Complexity Tracking

No constitution violations — table not needed.
