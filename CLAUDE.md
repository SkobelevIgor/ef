# Ways of Working

## Not try to guess, clear understanding of requriements is top priority.
If something from user's input is not clear, switch to planning mode and intervew user with questions before implementation.

## Artifact rebuild.
Once you finish any changes, delete old binary artifact and build new with `go build -o ef .`, then move binary to ignore directory.

## TDD approach:

Coverage of the `editor` package should be not less then 80% (go test -cover ./editor)
For any code changes strictly follow TDD approach:
1. Write Test First (RED). Write a failing test that describes the expected behavior.
2. Run Test -- Verify it FAILS.
3. Write Minimal Implementation (GREEN). Only enough code to make the test pass.
4. Run Test -- Verify it PASSES.
5. Refactor (IMPROVE). Remove duplication, improve names, optimize -- tests must stay green.
6. Verify Coverage.

### Edge Cases You MUST Test:
- Nil input;
- Empty arrays/strings;
- Boundary values (min/max);
- Special characters (Unicode, emojis, etc);

### Test Anti-Patterns to Avoid
- Testing implementation details (internal state) instead of behavior
- Tests depending on each other (shared state)
- Asserting too little (passing tests that don't verify anything)

## Coding guide:
- ALWAYS follow dependency inversion principle, never pass instance or pointer of struct to function, use interface definition instead. Actively use `gomock` in testing of such dependencies. This will simplify writing of unit tests.
- NEVER create mehtods, longer then 20 rows. If you have such situation, decompose the function / method.

