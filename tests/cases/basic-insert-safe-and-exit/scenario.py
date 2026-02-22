import os
import tempfile
import pexpect


def run(ef_path):
    # Create a temporary file to edit
    tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False)
    tmp_path = tmp.name
    print(tmp_path)
    tmp.close()


    # Launch ef with the temp file
    child = pexpect.spawn(ef_path, [tmp_path], encoding='utf-8', timeout=5)
    child.expect(pexpect.TIMEOUT, timeout=1)  # wait for editor to start

    # Enter insert mode with 'i'
    child.send('i')

    # Type "hello world"
    child.send('hello world')

    # Press Escape to return to normal mode
    child.send('\x1b')

    # Press F10 to save and exit
    # F10 escape sequence
    child.send('\x1b[21~')

    child.expect(pexpect.EOF, timeout=5)

    # Verify file contains "hello world"
    with open(tmp_path, 'r') as f:
        content = f.read()

    if 'hello world' not in content:
        raise BaseException(
            f'Expected file to contain "hello world", but got: {repr(content)}'
        )

