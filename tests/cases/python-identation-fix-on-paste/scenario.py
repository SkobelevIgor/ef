import os
import shutil
import time
import pexpect


def run(ef_path):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    source_file = os.path.join(test_dir, "test.py")
    insert_file = os.path.join(test_dir, "py_insert.txt")
    expected_file = os.path.join(test_dir, "expected.py")

    # Work on a copy so the original seed file is preserved
    work_file = os.path.join(test_dir, "work.py")
    shutil.copy2(source_file, work_file)

    with open(insert_file, "r") as f:
        paste_content = f.read()

    # Python uses spaces (expandtab), no conversion needed
    lines = paste_content.split("\n")

    # First content line: strip leading whitespace (over-indented in source)
    if lines:
        lines[0] = lines[0].lstrip()

    child = pexpect.spawn(ef_path, [work_file], timeout=10,
                          dimensions=(50, 200))
    child.expect("")
    time.sleep(0.5)

    # Navigate to line 2 with 'j'
    child.send("j")
    time.sleep(0.1)

    # Open new line below 'def main(self):' with 'o'
    child.send("o")
    time.sleep(0.3)

    # Send paste content line by line with autocomplete dismissal
    # and auto-indent clearing after each Enter
    for i, line in enumerate(lines):
        child.send(line)
        if i < len(lines) - 1:
            # Dismiss autocomplete with Left+Right arrow keys
            child.send("\x1b[D\x1b[C")
            time.sleep(0.05)
            child.send("\r")
            time.sleep(0.05)
            # Clear auto-indent (count leading spaces)
            indent_len = len(line) - len(line.lstrip()) if line.strip() else 0
            if indent_len > 0:
                child.send("\x08" * indent_len)
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
