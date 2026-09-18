#include "unity.h"
#include "input.h"
#include "pane.h"
#include "screen.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static InputState *is;

void setUp(void) { is = input_state_new(); }
void tearDown(void) { input_state_free(is); }

void test_new_input_state(void) {
    TEST_ASSERT_EQUAL_INT(0, is->count);
    TEST_ASSERT_FALSE(is->has_count);
    TEST_ASSERT_FALSE(input_state_has_pending(is));
}

void test_get_count_default(void) {
    TEST_ASSERT_EQUAL_INT(1, input_state_get_count(is));
}

void test_add_digits(void) {
    input_state_add_digit(is, 3);
    TEST_ASSERT_EQUAL_INT(3, input_state_get_count(is));
    input_state_add_digit(is, 5);
    TEST_ASSERT_EQUAL_INT(35, input_state_get_count(is));
}

void test_add_digits_clamps_to_max_count(void) {
    for (int i = 0; i < 30; i++) input_state_add_digit(is, 9);
    TEST_ASSERT_TRUE(is->has_count);
    TEST_ASSERT_EQUAL_INT(MAX_COUNT, input_state_get_count(is));
}

void test_reset_clears_count(void) {
    input_state_add_digit(is, 5);
    input_state_reset(is);
    TEST_ASSERT_FALSE(is->has_count);
    TEST_ASSERT_EQUAL_INT(1, input_state_get_count(is));
}

void test_reset_preserves_last_find(void) {
    input_state_save_last_find(is, L'x', true);
    input_state_reset(is);
    TEST_ASSERT_TRUE(is->has_last_find);
    TEST_ASSERT_EQUAL_INT(L'x', is->last_find_char);
    TEST_ASSERT_TRUE(is->last_find_forward);
}

void test_has_pending_find_forward(void) {
    is->pending_find_forward = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_operator(void) {
    is->pending_operator = L'd';
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_goto_line(void) {
    is->pending_goto_line = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_mark(void) {
    is->pending_mark = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_jump(void) {
    is->pending_jump_to_mark = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_map_buf_push(void) {
    input_state_map_push(is, L'(');
    TEST_ASSERT_EQUAL_INT(1, is->map_buf_len);
    TEST_ASSERT_EQUAL_INT(L'(', is->map_buf[0]);
    input_state_map_push(is, L'(');
    TEST_ASSERT_EQUAL_INT(2, is->map_buf_len);
    TEST_ASSERT_EQUAL_INT(L'(', is->map_buf[1]);
}

void test_map_buf_clear(void) {
    input_state_map_push(is, L'x');
    input_state_map_clear(is);
    TEST_ASSERT_EQUAL_INT(0, is->map_buf_len);
}

void test_map_buf_overflow(void) {
    for (int i = 0; i < MAP_BUF_SIZE + 5; i++)
        input_state_map_push(is, L'a' + (i % 26));
    TEST_ASSERT_EQUAL_INT(MAP_BUF_SIZE, is->map_buf_len);
}

void test_map_buf_reset_clears(void) {
    input_state_map_push(is, L'z');
    input_state_reset(is);
    TEST_ASSERT_EQUAL_INT(0, is->map_buf_len);
}

void test_map_buf_initial_empty(void) {
    TEST_ASSERT_EQUAL_INT(0, is->map_buf_len);
}

/* --- input_handle_find_char tests ---------------------------------------- */

void test_handle_find_char_forward(void) {
    Buffer *buf = buffer_new();
    wchar_t *l = malloc(sizeof(wchar_t) * 7);
    wmemcpy(l, L"abcdef", 6); l[6] = L'\0';
    buffer_set_line(buf, 0, l, 6);
    Pane *pane = pane_new(buf);

    is->pending_find_forward = true;
    EditorEvent ev = {EV_KEY, (int)L'd', L'd', true};
    input_handle_find_char(is, pane, &ev, 1);
    TEST_ASSERT_EQUAL_INT(3, pane->cursor_col);
    TEST_ASSERT_TRUE(is->has_last_find);
    TEST_ASSERT_TRUE(is->last_find_forward);
    TEST_ASSERT_FALSE(is->pending_find_forward);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_find_char_backward(void) {
    Buffer *buf = buffer_new();
    wchar_t *l = malloc(sizeof(wchar_t) * 7);
    wmemcpy(l, L"abcdef", 6); l[6] = L'\0';
    buffer_set_line(buf, 0, l, 6);
    Pane *pane = pane_new(buf);
    pane->cursor_col = 5;

    is->pending_find_backward = true;
    EditorEvent ev = {EV_KEY, (int)L'b', L'b', true};
    input_handle_find_char(is, pane, &ev, 1);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_col);
    TEST_ASSERT_FALSE(is->last_find_forward);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_find_char_escape(void) {
    is->pending_find_forward = true;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    EditorEvent ev = {EV_KEY, 27, 27, true};
    input_handle_find_char(is, pane, &ev, 1);
    TEST_ASSERT_FALSE(is->pending_find_forward);

    pane_free(pane);
    buffer_free(buf);
}

/* --- input_handle_goto_line tests ---------------------------------------- */

void test_handle_goto_line_digit_and_enter(void) {
    Buffer *buf = buffer_new();
    /* Create 5 lines */
    for (int i = 0; i < 4; i++) {
        wchar_t *nl = malloc(sizeof(wchar_t) * 2);
        nl[0] = L'a' + i; nl[1] = L'\0';
        buffer_insert_line_after(buf, buf->line_count - 1, nl, 1);
    }
    Pane *pane = pane_new(buf);
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;

    /* Type '3' */
    EditorEvent ev3 = {EV_KEY, (int)L'3', L'3', true};
    input_handle_goto_line(is, pane, &ev3);
    TEST_ASSERT_EQUAL_INT(1, is->goto_line_buf_len);

    /* Enter */
    EditorEvent enter = {EV_KEY, (int)L'\n', L'\n', true};
    input_handle_goto_line(is, pane, &enter);
    TEST_ASSERT_EQUAL_INT(2, pane->cursor_row); /* line 3 = row 2 */
    TEST_ASSERT_FALSE(is->pending_goto_line);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_escape(void) {
    is->pending_goto_line = true;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    EditorEvent ev = {EV_KEY, 27, 27, true};
    input_handle_goto_line(is, pane, &ev);
    TEST_ASSERT_FALSE(is->pending_goto_line);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_dollar(void) {
    Buffer *buf = buffer_new();
    for (int i = 0; i < 4; i++) {
        wchar_t *nl = malloc(sizeof(wchar_t) * 2);
        nl[0] = L'a' + i; nl[1] = L'\0';
        buffer_insert_line_after(buf, buf->line_count - 1, nl, 1);
    }
    Pane *pane = pane_new(buf);
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;

    EditorEvent ev_dollar = {EV_KEY, (int)L'$', L'$', true};
    input_handle_goto_line(is, pane, &ev_dollar);
    EditorEvent enter = {EV_KEY, (int)L'\n', L'\n', true};
    input_handle_goto_line(is, pane, &enter);
    TEST_ASSERT_EQUAL_INT(buf->line_count - 1, pane->cursor_row);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_backspace(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    EditorEvent ev3 = {EV_KEY, (int)L'3', L'3', true};
    input_handle_goto_line(is, pane, &ev3);
    TEST_ASSERT_EQUAL_INT(1, is->goto_line_buf_len);

    EditorEvent bs = {EV_KEY, 127, 127, true};
    input_handle_goto_line(is, pane, &bs);
    TEST_ASSERT_EQUAL_INT(0, is->goto_line_buf_len);
    TEST_ASSERT_FALSE(is->pending_goto_line); /* auto-cancel on empty */

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_rejects_letters(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    /* Letters should be ignored */
    EditorEvent ev_a = {EV_KEY, (int)L'a', L'a', true};
    input_handle_goto_line(is, pane, &ev_a);
    TEST_ASSERT_EQUAL_INT(0, is->goto_line_buf_len);
    TEST_ASSERT_TRUE(is->pending_goto_line);

    EditorEvent ev_Z = {EV_KEY, (int)L'Z', L'Z', true};
    input_handle_goto_line(is, pane, &ev_Z);
    TEST_ASSERT_EQUAL_INT(0, is->goto_line_buf_len);

    /* Digits should still work after rejected letters */
    EditorEvent ev5 = {EV_KEY, (int)L'5', L'5', true};
    input_handle_goto_line(is, pane, &ev5);
    TEST_ASSERT_EQUAL_INT(1, is->goto_line_buf_len);
    TEST_ASSERT_EQUAL_STRING("5", is->goto_line_buffer);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_auto_cancel_on_empty(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    /* Type a digit */
    EditorEvent ev7 = {EV_KEY, (int)L'7', L'7', true};
    input_handle_goto_line(is, pane, &ev7);
    TEST_ASSERT_EQUAL_INT(1, is->goto_line_buf_len);
    TEST_ASSERT_TRUE(is->pending_goto_line);

    /* Backspace removes digit and auto-cancels */
    EditorEvent bs = {EV_KEY, 127, 127, true};
    input_handle_goto_line(is, pane, &bs);
    TEST_ASSERT_FALSE(is->pending_goto_line);
    TEST_ASSERT_EQUAL_INT(0, is->goto_line_buf_len);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_backspace_on_empty_noop(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    /* Backspace with no digits typed should be a no-op (stay in goto mode) */
    EditorEvent bs = {EV_KEY, 127, 127, true};
    input_handle_goto_line(is, pane, &bs);
    TEST_ASSERT_TRUE(is->pending_goto_line);
    TEST_ASSERT_EQUAL_INT(0, is->goto_line_buf_len);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_buffer_capacity(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    Pane *pane = pane_new(buf);

    /* Fill to capacity (62 chars) */
    for (int i = 0; i < 62; i++) {
        EditorEvent ev = {EV_KEY, (int)L'1', L'1', true};
        input_handle_goto_line(is, pane, &ev);
    }
    TEST_ASSERT_EQUAL_INT(62, is->goto_line_buf_len);

    /* 63rd digit should be silently dropped */
    EditorEvent ev63 = {EV_KEY, (int)L'9', L'9', true};
    input_handle_goto_line(is, pane, &ev63);
    TEST_ASSERT_EQUAL_INT(62, is->goto_line_buf_len);

    pane_free(pane);
    buffer_free(buf);
}

void test_handle_goto_line_large_number(void) {
    is->pending_goto_line = true;
    is->goto_line_buffer[0] = '\0';
    is->goto_line_buf_len = 0;
    Buffer *buf = buffer_new();
    for (int i = 0; i < 4; i++) {
        wchar_t *nl = malloc(sizeof(wchar_t) * 2);
        nl[0] = L'a' + i; nl[1] = L'\0';
        buffer_insert_line_after(buf, buf->line_count - 1, nl, 1);
    }
    Pane *pane = pane_new(buf);

    /* Type 100000 */
    const char *digits = "100000";
    for (int i = 0; digits[i]; i++) {
        EditorEvent ev = {EV_KEY, (int)digits[i], (wchar_t)digits[i], true};
        input_handle_goto_line(is, pane, &ev);
    }
    TEST_ASSERT_EQUAL_INT(6, is->goto_line_buf_len);
    TEST_ASSERT_EQUAL_STRING("100000", is->goto_line_buffer);
    TEST_ASSERT_TRUE(is->pending_goto_line);

    /* Enter goes to last line (clamped) */
    EditorEvent enter = {EV_KEY, (int)L'\n', L'\n', true};
    input_handle_goto_line(is, pane, &enter);
    TEST_ASSERT_EQUAL_INT(buf->line_count - 1, pane->cursor_row);

    pane_free(pane);
    buffer_free(buf);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_new_input_state);
    RUN_TEST(test_get_count_default);
    RUN_TEST(test_add_digits);
    RUN_TEST(test_add_digits_clamps_to_max_count);
    RUN_TEST(test_reset_clears_count);
    RUN_TEST(test_reset_preserves_last_find);
    RUN_TEST(test_has_pending_find_forward);
    RUN_TEST(test_has_pending_operator);
    RUN_TEST(test_has_pending_goto_line);
    RUN_TEST(test_has_pending_mark);
    RUN_TEST(test_has_pending_jump);
    RUN_TEST(test_map_buf_push);
    RUN_TEST(test_map_buf_clear);
    RUN_TEST(test_map_buf_overflow);
    RUN_TEST(test_map_buf_reset_clears);
    RUN_TEST(test_map_buf_initial_empty);
    RUN_TEST(test_handle_find_char_forward);
    RUN_TEST(test_handle_find_char_backward);
    RUN_TEST(test_handle_find_char_escape);
    RUN_TEST(test_handle_goto_line_digit_and_enter);
    RUN_TEST(test_handle_goto_line_escape);
    RUN_TEST(test_handle_goto_line_dollar);
    RUN_TEST(test_handle_goto_line_backspace);
    RUN_TEST(test_handle_goto_line_rejects_letters);
    RUN_TEST(test_handle_goto_line_auto_cancel_on_empty);
    RUN_TEST(test_handle_goto_line_backspace_on_empty_noop);
    RUN_TEST(test_handle_goto_line_buffer_capacity);
    RUN_TEST(test_handle_goto_line_large_number);
    return UNITY_END();
}
