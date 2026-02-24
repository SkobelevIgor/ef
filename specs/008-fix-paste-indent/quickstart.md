# Quickstart: Fix Paste Indentation

## What this feature does

When pasting code into ef (via terminal paste or vim `p`/`P`), the editor automatically adjusts indentation of the pasted block to match the insertion context. Mixed tabs/spaces in the source are normalized to the file type's configured style.

## How to test

### Prerequisites
- `~/.efconfig` with at least one file type configured with `"autoindentation": true`
- Example efconfig entry for Go:
  ```json
  {
    "file_types": {
      "go": {
        "extensions": [".go"],
        "autoindentation": true,
        "expandtab": false,
        "tabstop": 4,
        "shiftwidth": 4
      }
    }
  }
  ```

### Test 1: Terminal paste into function body
1. Open a `.go` file with a function: `func foo() {\n\n}`
2. Place cursor on the empty line inside the function (indent level 1)
3. Paste this code (with zero indentation):
   ```
   x := 1
   if x > 0 {
       fmt.Println(x)
   }
   ```
4. Expected: All lines get 1 tab indent, `fmt.Println` gets 2 tabs

### Test 2: Vim `p` command
1. Yank a block of code from one indent level
2. Move cursor to a different indent level
3. Press `p`
4. Expected: Pasted block shifts to match new indent context

### Test 3: Unconfigured file type
1. Open a `.txt` file (not in efconfig)
2. Paste indented code
3. Expected: Content appears exactly as copied — no modification

### Test 4: Undo
1. Perform any paste operation
2. Press `u` (undo)
3. Expected: Single undo restores buffer to pre-paste state

## Build

```bash
go build -o ef .
mv ef ignore/
```
