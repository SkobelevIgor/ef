import os
import shutil
import time
import pexpect


def run(ef_path):
    test_dir = os.getcwd()
    source_file = os.path.join(test_dir, "test.py")
    insert_file = os.path.join(test_dir, "py_insert.txt")
    expected_file = os.path.join(test_dir, "expected.py")

    # Work on a copy so the original seed file is preserved
    work_file = os.path.join(test_dir, "work.py")
    shutil.copy2(source_file, work_file)

    with open(insert_file, "r") as f:
        paste_content = f.read()

    child = pexpect.spawn(ef_path, [work_file], timeout=10,
                          dimensions=(50, 200))
    child.expect("")
    time.sleep(0.5)

    # Navigate to line 2 with '2j'
    child.send("2j")
    time.sleep(0.1)

    # Enter insert mode with 'o' (opens new line below)
    # Auto-indent copies 4 spaces from 'def main(self):'
    child.send("o")
    time.sleep(0.3)

    # Clear auto-indent from 'o' (4 backspaces)
    child.send("\x08" * 4)
    time.sleep(0.1)

    # Split paste content into lines
    lines = paste_content.split("\n")

    # Send lines 1-5 at once (autocomplete will merge them since each line
    # ends with a repeated word like "data_source = data_source").
    # The empty line 6 will produce two \r chars — first consumed by
    # autocomplete, second creates a newline.
    # Lines 1-5 + empty line 6
    first_block = "\r".join(lines[:6]) + "\r"
    child.send(first_block)
    time.sleep(0.3)

    # At this point a newline was created (from the second \r of the empty line).
    # Auto-indent copied 4 spaces from the merged line. Clear it.
    child.send("\x08" * 4)
    time.sleep(0.1)

    # Send remaining lines (7-18) one by one with autocomplete dismissal
    # and auto-indent clearing after each Enter
    for i in range(6, len(lines)):
        line = lines[i]
        child.send(line)
        if i < len(lines) - 1:
            # Dismiss autocomplete with Left+Right arrow keys
            child.send("\x1b[D\x1b[C")
            time.sleep(0.05)
            child.send("\r")
            time.sleep(0.05)
            # Clear auto-indent (equals leading whitespace of line just typed)
            indent = len(line) - len(line.lstrip()) if line.strip() else 0
            if indent > 0:
                child.send("\x08" * indent)
                time.sleep(0.05)

    time.sleep(0.5)

    # Press Escape to exit insert mode
    child.send("\x1b")
    time.sleep(0.2)

    # Save and quit with F10
    child.send("\x1b[21~")

    child.expect(pexpect.EOF)
    child.wait()

    with open(work_file, "r") as f:
        actual = f.read()

    with open(expected_file, "r") as f:
        expected = f.read()

    if actual != expected:
        raise BaseException(
            f"File content mismatch.\nExpected:\n{repr(expected)}\nActual:\n{repr(actual)}"
        )
