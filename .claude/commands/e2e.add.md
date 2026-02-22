---
description: Creates a new end-to-end test case.
---

## User Input

```text
$ARGUMENTS
```
You **MUST** consider the user input before proceeding (if not empty).

There are required `Name` and `Scenario` attributes. If user didn't add them, re-ask user to add required data.

## Execution steps

### 1. Create test case folder.

In `./tests/cases` directory with name in `test-case-name` format ( space between words, lowercase ). Eg.:
`/e2e.add Name: external changes added to history. Senario: ...` should create folder `./tests/cases/external-changes-added-to-history`.

- If name contains any symbols except digits and letters, re-ask to input new `Name`;
- If such directory already exists, re-ask user to input new `Name`;

### 2. Scenario creation.
Create a file `scenario.py` with entrypoint function `run` and implement steps described in `Scenario` using `pexpect` library.
- `run` function gets only one parameter - full path to executable `ef` build.
For seed data user can mention data files with format: FILE_<name>::<full path to it> - copy them to test folder as well and use reference in your test script.
- any assets should be created in test case folder, but can be used in any way. For example, you can create a file with some content and then check if it was changed after `ef` execution.
If validation fails throw `BaseException` with message, that describes assert failure.
- Script shouldn't remove any files from test case folder, but can create new ones.