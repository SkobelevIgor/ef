#ifndef EF_TEST_HELPERS_H
#define EF_TEST_HELPERS_H

#include "editor.h"
#include "screen.h"
#include <wchar.h>

/* Mock screen vtable (shared across all integration tests). */
extern int test_mock_width;
extern int test_mock_height;
extern ScreenVTable test_mock_screen;

/* Create a buffer + pane + editor from literal lines.
   Caller receives pointers via ed_out and buf_out. */
void test_setup_editor(const wchar_t *lines[], int count,
                       Editor **ed_out, Buffer **buf_out);

/* Dispatch a printable character event. */
void test_send_char(Editor *ed, wchar_t ch);

/* Dispatch a special-key event. */
void test_send_key(Editor *ed, int key);

#endif /* EF_TEST_HELPERS_H */
