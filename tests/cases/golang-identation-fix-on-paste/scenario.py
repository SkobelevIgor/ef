import os
import shutil
import pexpect


def run(ef_path):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    source_file = os.path.join(test_dir, "test.go")
    insert_file = os.path.join(test_dir, "go_insert.txt")
    expected_file = os.path.join(test_dir, "go_result.go")

    # Work on a copy so the original seed file is preserved
    work_file = os.path.join(test_dir, "work.go")
    shutil.copy2(source_file, work_file)

    with open(insert_file, "r") as f:
        paste_content = f.read()

    child = pexpect.spawn(ef_path, [work_file], timeout=10)
    child.expect("")

    # Navigate to line 3 with '3j'
    child.send("3j")

    # Enter insert mode with 'i'
    child.send("i")

    # Send paste content as regular input, using \r for newlines
    lines = paste_content.split("\n")
    for i, line in enumerate(lines):
        child.send(line)
        if i < len(lines) - 1:
            child.send("\r")

    # Press Escape to exit insert mode
    child.send("\x1b")

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
