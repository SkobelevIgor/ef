# Data Model: E2E Test Runner

**Feature**: 007-e2e-runner | **Date**: 2026-02-22

## Entities

This feature has no persistent data model. All data is filesystem-based and ephemeral.

### Test Case (filesystem entity)

```
tests/cases/<test-case-name>/
├── scenario.py          # Required: must export run(ef_path) function
└── [fixture files...]   # Optional: any files needed by the test
```

**Identity**: Directory name (lowercase-with-hyphens)
**Uniqueness**: Directory name must be unique within `tests/cases/`
**Lifecycle**: Static — created by test authors, never modified by runner

### Temp Execution Directory (ephemeral)

```
$TMPDIR/<test-case-name>-<random>/
├── scenario.py          # Copied from test case
└── [fixture files...]   # Copied from test case
```

**Identity**: OS-assigned path with test name prefix
**Lifecycle**: Created before test execution → deleted after execution (always, via finally block)
**State transitions**: Created → Running (CWD set here) → Deleted

## Relationships

```
Test Case 1──copies to──1 Temp Directory
Temp Directory 1──contains──1 Scenario Module
Scenario Module 1──receives──1 EF Executable Path
```

## No Database / Persistence

The runner maintains no state between invocations. Each run is fully independent.
