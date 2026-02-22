# Research: E2E Test Runner

**Feature**: 007-e2e-runner | **Date**: 2026-02-22

## No NEEDS CLARIFICATION Items

The technical context had no unknowns. All decisions are straightforward for a Python CLI script.

## Technology Decisions

### Decision 1: Dynamic Module Loading Approach

**Decision**: Use `importlib.util.spec_from_file_location` + `loader.exec_module`
**Rationale**: Allows loading `scenario.py` from an arbitrary filesystem path (the temp directory) without modifying `sys.path` or risking module name collisions. Each test gets a fresh module namespace.
**Alternatives considered**:
- `exec(open(...).read())` — no proper module namespace, harder to detect `run()` function
- `sys.path.insert` + `import scenario` — pollutes sys.path, module caching issues between tests
- `subprocess` calling a wrapper — unnecessary process overhead, harder error propagation

### Decision 2: Temp Directory Strategy

**Decision**: `tempfile.mkdtemp(prefix=f"{test_name}-")` + `shutil.copytree` contents + `shutil.rmtree` in finally
**Rationale**: Standard Python pattern. Prefix makes temp dirs identifiable during debugging. `copytree` handles recursive file copying. `finally` ensures cleanup on success and failure.
**Alternatives considered**:
- `tempfile.TemporaryDirectory` context manager — doesn't allow custom prefix pattern matching the spec's `<name>-<hash>` format as cleanly
- Symlinks instead of copy — would not isolate tests from modifying originals

### Decision 3: No Argument Parsing Library

**Decision**: Direct `sys.argv` parsing
**Rationale**: Only two commands (`run` and `run-all`), each with exactly one additional argument (ef path). Adding `argparse` for this is over-engineering.
**Alternatives considered**:
- `argparse` — adds complexity for no benefit given the simple interface
- `click` — external dependency, violates minimal-deps principle
