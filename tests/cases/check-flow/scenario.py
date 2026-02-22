import os
import pexpect


def run(ef_path):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    expected_file = os.path.join(test_dir, "example.txt")
    output_file = os.path.join(test_dir, "output.txt")

    child = pexpect.spawn(ef_path, [output_file], timeout=10)
    child.expect("")

    # Enter insert mode
    child.send("i")

    # Type "hello world"
    child.sendline("hello world")

    # Press Escape to exit insert mode
    child.send("\x1b")

    # Save and quit with F10
    # F10 ANSI escape sequence
    child.send("\x1b[21~")

    child.expect(pexpect.EOF)
    child.wait()

    if not os.path.exists(output_file):
        raise BaseException("Output file was not created")

    with open(output_file, "r") as f:
        actual = f.read()

    with open(expected_file, "r") as f:
        expected = f.read()

    if actual != expected:
        raise BaseException(
            f"File content mismatch.\nExpected:\n{repr(expected)}\nActual:\n{repr(actual)}"
        )
