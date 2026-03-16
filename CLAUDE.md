# ef — Terminal Text Editor (C)

## Build & Run

```sh
cmake -B build && cmake --build build
./build/ef [file]
```

## Tests

```sh
cmake -B build && cmake --build build
ctest --test-dir build          # all tests
ctest --test-dir build -R test_buffer  # single test
```

Test framework: Unity (embedded in `tests/unity/`).
Each test file links against `ef_core` static library + Unity via the `add_ef_test()` macro in `tests/CMakeLists.txt`.

## Project Layout

```
src/          # Core source (.c/.h pairs)
tests/        # One test file per module (test_<module>.c), shared helpers in test_helpers.c/.h
deps/cjson/   # Embedded cJSON dependency
build/        # CMake build output (gitignored)
```

## Dependencies

- **ncursesw** — Homebrew on macOS (`brew install ncurses`)
- **PCRE2** — Homebrew on macOS (`brew install pcre2`)
- **cJSON** — embedded in `deps/cjson/`
- **Unity** — embedded in `tests/unity/`

## Coding Conventions

- C11 standard, compiled with `-Wall -Wextra -Wpedantic`
- Each module is a `.c`/`.h` pair in `src/`
- Static library `ef_core` contains all modules; `main.c` is the entry point
- Functions should be small (<50 lines), files focused (<800 lines)
