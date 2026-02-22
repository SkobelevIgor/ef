# Quickstart: E2E Test Runner

## Prerequisites

- Python 3.x installed
- `pexpect` installed (`pip install -r tests/requirements.txt`)
- ef binary built (`go build -o ef .`)

## Run a Single Test

```bash
./tests/e2e run basic-insert-safe-and-exit ./ignore/ef
```

Expected output on success:
```
OK
```

Expected output on failure:
```
Expected file to contain "hello world", but got: ''
```

## Run All Tests

```bash
./tests/e2e run-all ./ignore/ef
```

Expected output:
```
basic-insert-safe-and-exit: OK
1/1 passed
```

## Create a New Test Case

1. Create a directory in `tests/cases/`:
   ```bash
   mkdir tests/cases/my-new-test
   ```

2. Create `scenario.py` with a `run()` function:
   ```python
   def run(ef_path):
       # Your test logic here
       # Raise an exception to signal failure
       # Return normally to signal success
       pass
   ```

3. Optionally add fixture files (text files, configs) to the directory — they'll be copied to a temp dir before the test runs.

4. Run it:
   ```bash
   ./tests/e2e run my-new-test ./ignore/ef
   ```
