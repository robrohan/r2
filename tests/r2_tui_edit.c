#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"
#define R2_TUI_EDIT_IMPLEMENTATION
#include "../r2_tui_edit.h"
#include "../r2_unit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ED_COLS 60
#define ED_ROWS 20

static uint8_t ed_arena[R2_TUI_ARENA_SIZE(ED_COLS, ED_ROWS)];
static uint32_t ed_now = 1000;

static uint32_t ed_seed = 0xA5A5F00Du;
static uint32_t ed_rand(void)
{
    ed_seed ^= ed_seed << 13;
    ed_seed ^= ed_seed >> 17;
    ed_seed ^= ed_seed << 5;
    return ed_seed;
}

/* the text as a C string (static buffer) */
static const char *ed_text(const tui_edit *e)
{
    static char out[4096];
    tui_edit_get(e, out, sizeof(out));
    return out;
}

/* recount everything the editor tracks incrementally; 0 if it all agrees */
static const char *ed_check(const tui_edit *e)
{
    const char *s = ed_text(e);
    uint32_t len = (uint32_t)strlen(s), nl = 0, before_cursor = 0, before_top = 0, i;
    if (len != tui_edit_len(e))
        return "len disagrees";
    for (i = 0; i < len; i++)
    {
        nl += s[i] == '\n';
        if (i < e->cursor)
            before_cursor += s[i] == '\n';
        if (i < e->top)
            before_top += s[i] == '\n';
    }
    if (e->lines != nl + 1)
        return "line count wrong";
    if (e->cursor > len)
        return "cursor past the end";
    if (e->line != before_cursor)
        return "cursor line wrong";
    if (e->top > len || (e->top > 0 && s[e->top - 1] != '\n'))
        return "top is not a line start";
    if (e->top_line != before_top)
        return "top line number wrong";
    return 0;
}

///////////////////////////////////////////////////////////////
// The gap buffer

static const char *test_edit_basics(void)
{
    char buf[64];
    tui_edit e;
    const char *err;
    tui_edit_init(&e, buf, sizeof(buf));
    r2_assert("starts empty", tui_edit_len(&e) == 0 && e.lines == 1 && ed_text(&e)[0] == 0);
    r2_assert("set", tui_edit_set(&e, S8("one\ntwo\nthree")));
    r2_assert("text", strcmp(ed_text(&e), "one\ntwo\nthree") == 0 && e.lines == 3 && e.cursor == 0 && !e.modified);
    tui_edit_goto(&e, 5);
    r2_assert("goto counts lines", e.line == 1);
    r2_assert("insert", tui_edit_insert(&e, S8("X\nY")) && strcmp(ed_text(&e), "one\ntX\nYwo\nthree") == 0);
    r2_assert("insert moves the cursor and counts lines", e.cursor == 8 && e.line == 2 && e.lines == 4 && e.modified);
    tui_edit_delete(&e, 2, 9);
    r2_assert("delete across lines", strcmp(ed_text(&e), "ono\nthree") == 0 && e.lines == 2);
    r2_assert("cursor inside the deleted part goes to its start", e.cursor == 2 && e.line == 0);
    r2_assert(ed_check(&e) ? ed_check(&e) : "", !(err = ed_check(&e)));
    r2_assert("too big for set", !tui_edit_set(&e, s8_from_bytes(buf, 65)));
    char out[4];
    r2_assert("get cuts to the buffer", tui_edit_get(&e, out, sizeof(out)) == 3 && strcmp(out, "ono") == 0);
    return 0;
}

static const char *test_edit_full(void)
{
    char buf[8];
    tui_edit e;
    tui_edit_init(&e, buf, sizeof(buf));
    r2_assert("fits exactly", tui_edit_insert(&e, S8("abcdefgh")) && tui_edit_len(&e) == 8);
    r2_assert("full: insert refused", !tui_edit_insert(&e, S8("x")));
    r2_assert("unchanged", strcmp(ed_text(&e), "abcdefgh") == 0 && e.cursor == 8);
    tui_edit_delete(&e, 0, 1);
    r2_assert("room again", tui_edit_insert(&e, S8("!")) && strcmp(ed_text(&e), "bcdefgh!") == 0);
    return 0;
}

static const char *test_edit_model(void)
{
    /* random edits against a plain array; everything tracked must match a recount */
    static const char *const pieces[] = {"a", "bc", "\n", "\n\n", "\t", "\xC3\xA9", "\xE4\xB8\xAD", "xyz\nq", " "};
    static char buf[2048];
    char model[2048];
    uint32_t mlen = 0, mcur = 0;
    tui_edit e;
    const char *err;
    tui_edit_init(&e, buf, sizeof(buf));
    for (int iter = 0; iter < 20000; iter++)
    {
        uint32_t r = ed_rand() % 10;
        if (r < 4)
        {
            const char *p = pieces[ed_rand() % (sizeof(pieces) / sizeof(pieces[0]))];
            uint32_t n = (uint32_t)strlen(p);
            int ok = tui_edit_insert(&e, s8_from_cstr(p));
            r2_assert("insert refused with room left", ok || mlen + n > sizeof(buf));
            if (ok)
            {
                memmove(model + mcur + n, model + mcur, mlen - mcur);
                memcpy(model + mcur, p, n);
                mlen += n;
                mcur += n;
            }
        }
        else if (r < 6 && mlen > 0)
        {
            /* delete a range between character starts */
            uint32_t a = ed_rand() % (mlen + 1), b = ed_rand() % (mlen + 1);
            if (a > b)
            {
                uint32_t tmp = a;
                a = b;
                b = tmp;
            }
            while (a > 0 && (model[a] & 0xC0) == 0x80)
                a--;
            while (b < mlen && (model[b] & 0xC0) == 0x80)
                b++;
            tui_edit_delete(&e, a, b);
            memmove(model + a, model + b, mlen - b);
            mlen -= b - a;
            if (mcur >= b)
                mcur -= b - a;
            else if (mcur > a)
                mcur = a;
        }
        else if (r < 8)
        {
            uint32_t to = ed_rand() % (mlen + 1);
            while (to > 0 && to < mlen && (model[to] & 0xC0) == 0x80)
                to--;
            tui_edit_goto(&e, to);
            mcur = to;
        }
        else
        {
            /* scroll somewhere: the top line follows the cursor */
            r2__ed_scroll(&e, 1 + ed_rand() % 6, 1 + ed_rand() % 30);
        }
        model[mlen] = 0;
        r2_assert("text differs from the model", strcmp(ed_text(&e), model) == 0);
        r2_assert("cursor differs from the model", e.cursor == mcur);
        err = ed_check(&e);
        r2_assert(err ? err : "", !err);
    }
    return 0;
}

static const char *test_edit_bad_utf8(void)
{
    /* invalid bytes are characters of one byte, forwards and backwards */
    char buf[32];
    tui_edit e;
    tui_edit_init(&e, buf, sizeof(buf));
    tui_edit_set(&e, S8("a\x80\x80\xC3\xA9\xE4\xB8z"));
    uint32_t stops[16], n = 0, at = 0;
    while (at < tui_edit_len(&e))
    {
        stops[n++] = at;
        at = r2__ed_next(&e, at);
    }
    r2_assert("forward stops", n == 6 && stops[1] == 1 && stops[2] == 2 && stops[3] == 3 && stops[4] == 5 && stops[5] == 7);
    for (uint32_t i = n; i-- > 1;)
        r2_assert("backward matches forward", r2__ed_prev(&e, stops[i]) == stops[i - 1]);
    r2_assert("from the end", r2__ed_prev(&e, tui_edit_len(&e)) == 7);
    tui_edit_goto(&e, 4);
    r2_assert("goto inside a character snaps back", e.cursor == 3);
    return 0;
}

///////////////////////////////////////////////////////////////
// The widget, driven by keys in a window

typedef struct ed_app
{
    tui_edit e;
    char buf[1024];
    int w, h, changes, with_button, presses;
} ed_app;

static void ed_ui(tui *t, tui_window *w, void *user)
{
    ed_app *a = (ed_app *)user;
    (void)w;
    tui_at(t, 0, 0);
    if (a->with_button && tui_button(t, "~B~efore"))
        a->presses++;
    if (tui_editor(t, "##ed", &a->e, a->w, a->h) & TUI_CHANGED)
        a->changes++;
}

static tui ed_t;

static tui_window *ed_open(ed_app *a, const char *text, int w, int h)
{
    tui_init(&ed_t, ed_arena, sizeof(ed_arena), ED_COLS, ED_ROWS);
    tui_edit_init(&a->e, a->buf, sizeof(a->buf));
    tui_edit_set(&a->e, s8_from_cstr(text));
    a->w = w;
    a->h = h;
    /* the window's inside is (1, 2) .. (1 + w, 2 + h) */
    tui_window *win = tui_window_open(&ed_t, "Ed", (tui_rect){0, 1, w + 2, h + 2 + (a->with_button ? 1 : 0)}, 0, ed_ui, a);
    tui_update(&ed_t, ed_now);
    return win;
}

static void ed_keys(const char *bytes)
{
    tui_feed(&ed_t, (const uint8_t *)bytes, strlen(bytes), ed_now);
    ed_now += 100;
    tui_update(&ed_t, ed_now);
}

/* row y of the editor area as text */
static const char *ed_row(int y, int w)
{
    static char out[512];
    size_t n = 0;
    for (int x = 0; x < w; x++)
    {
        rune r = TUI_CELL_RUNE(tui_get_cell(&ed_t, 1 + x, 2 + y));
        if (r != TUI_RUNE_CONT)
            n += utf8_encode(r ? r : ' ', (uint8_t *)out + n);
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
    return out;
}

static const char *test_edit_moving(void)
{
    static ed_app a;
    ed_open(&a, "first line\nab\n\xE4\xB8\xAD\xE6\x96\x87x\na\tb\nlast one here", 40, 10);
    tui_edit *e = &a.e;
    r2_assert("cursor shown at the start", ed_t.cur_visible && ed_t.cur_x == 1 && ed_t.cur_y == 2);
    ed_keys("\x1b[F");
    r2_assert("End", e->cursor == 10 && ed_t.cur_x == 11);
    ed_keys("\x1b[B");
    r2_assert("Down to a shorter line: its end", e->cursor == 13 && e->line == 1);
    ed_keys("\x1b[B");
    r2_assert("Down again keeps aiming for column 10: end of the wide line", e->line == 2 && tui_edit_column(e) == 5);
    ed_keys("\x1b[B\x1b[B");
    r2_assert("and on the last line, column 10", e->line == 4 && tui_edit_column(e) == 10);
    ed_keys("\x1b[H\x1b[A\x1b[A");
    r2_assert("Home, Up, Up: start of the wide line", e->line == 2 && tui_edit_column(e) == 0);
    ed_keys("\x1b[C");
    r2_assert("Right over a wide character: 2 columns", tui_edit_column(e) == 2 && ed_t.cur_x == 3);
    ed_keys("\x1b[B");
    r2_assert("Down from column 2 into a tab: before it", e->line == 3 && tui_edit_column(e) == 1);
    ed_keys("\x1b[C");
    r2_assert("Right over the tab: column 4", tui_edit_column(e) == 4);
    ed_keys("\x1b[A");
    r2_assert("Up from column 4: just after the second wide character", tui_edit_column(e) == 4);
    ed_keys("\x01");
    r2_assert("Ctrl+A", tui_edit_column(e) == 0);
    ed_keys("\x05");
    r2_assert("Ctrl+E", tui_edit_column(e) == 5);
    ed_keys("\x02\x02");
    r2_assert("Ctrl+B twice", tui_edit_column(e) == 2);
    ed_keys("\x06");
    r2_assert("Ctrl+F", tui_edit_column(e) == 4);
    ed_keys("\x10");
    r2_assert("Ctrl+P", e->line == 1);
    ed_keys("\x0e");
    r2_assert("Ctrl+N", e->line == 2);
    ed_keys("\x1b[1;5H");
    r2_assert("Ctrl+Home", e->cursor == 0);
    ed_keys("\x1b[1;5F");
    r2_assert("Ctrl+End", e->cursor == tui_edit_len(e) && e->line == 4);
    ed_keys("\x1b[1;5D");
    r2_assert("Ctrl+Left: word start", strcmp(ed_text(e) + e->cursor, "here") == 0);
    ed_keys("\x1b[1;5D\x1b[1;5D");
    r2_assert("two more words", strcmp(ed_text(e) + e->cursor, "last one here") == 0);
    ed_keys("\x1b[1;5C");
    r2_assert("Ctrl+Right: word end", strcmp(ed_text(e) + e->cursor, " one here") == 0);
    r2_assert(ed_check(e) ? ed_check(e) : "", !ed_check(e));
    return 0;
}

static const char *test_edit_typing(void)
{
    static ed_app a;
    ed_open(&a, "", 40, 10);
    tui_edit *e = &a.e;
    ed_keys("hello");
    r2_assert("typed", strcmp(ed_text(e), "hello") == 0 && a.changes == 5);
    ed_keys("\r\xE4\xB8\xAD!");
    r2_assert("Enter and UTF-8", strcmp(ed_text(e), "hello\n\xE4\xB8\xAD!") == 0 && e->lines == 2);
    ed_keys("\x7f\x7f");
    r2_assert("Backspace takes whole characters", strcmp(ed_text(e), "hello\n") == 0);
    ed_keys("\x7f");
    r2_assert("Backspace at a line start joins", strcmp(ed_text(e), "hello") == 0 && e->lines == 1);
    ed_keys("\x1b[D\x1b[D\x1b[3~");
    r2_assert("Delete", strcmp(ed_text(e), "helo") == 0);
    ed_keys("\x04");
    r2_assert("Ctrl+D", strcmp(ed_text(e), "hel") == 0);
    ed_keys("\x1b[H\x0b");
    r2_assert("Ctrl+K kills to the end", ed_text(e)[0] == 0);
    ed_keys("ab\rcd\x1b[A\x1b[F\x0b");
    r2_assert("Ctrl+K at the end of a line joins", strcmp(ed_text(e), "abcd") == 0);
    ed_keys("\t");
    r2_assert("Tab inserts a tab", strcmp(ed_text(e), "ab\tcd") == 0 && tui_edit_column(e) == 4);
    e->flags |= TUI_EDIT_SPACES;
    ed_keys("\t");
    r2_assert("with TUI_EDIT_SPACES: spaces to the next stop", strcmp(ed_text(e), "ab\t    cd") == 0);
    e->flags |= TUI_EDIT_AUTO_INDENT;
    ed_keys("\x1b[F\r");
    r2_assert("auto indent copies the leading blanks", strcmp(ed_text(e), "ab\t    cd\n") == 0);
    ed_keys("  x\ry");
    r2_assert("auto indent", strcmp(ed_text(e), "ab\t    cd\n  x\n  y") == 0);
    e->flags |= TUI_EDIT_READ_ONLY;
    ed_keys("zz\x7f\r\x0b\x1b[A");
    r2_assert("read only: moves but never changes", strcmp(ed_text(e), "ab\t    cd\n  x\n  y") == 0 && e->line == 1);
    r2_assert(ed_check(e) ? ed_check(e) : "", !ed_check(e));
    return 0;
}

static const char *test_edit_drawing(void)
{
    static ed_app a;
    ed_open(&a, "a\tb\n\xE4\xB8\xAD\xE6\x96\x87\nline three", 20, 4);
    r2_assert("tab drawn as spaces", strcmp(ed_row(0, 20), "a   b") == 0);
    r2_assert("wide characters", strcmp(ed_row(1, 20), "\xE4\xB8\xAD\xE6\x96\x87") == 0);
    r2_assert("third line", strcmp(ed_row(2, 20), "line three") == 0);
    r2_assert("nothing after the text", ed_row(3, 20)[0] == 0);
    a.e.flags |= TUI_EDIT_LINE_NUMBERS;
    tui_window_invalidate(&ed_t, tui_focused_window(&ed_t));
    tui_update(&ed_t, ed_now);
    r2_assert("line numbers", strcmp(ed_row(0, 20), "1 a   b") == 0 && strcmp(ed_row(2, 20), "3 line three") == 0);
    r2_assert("cursor after the gutter", ed_t.cur_x == 1 + 2);
    return 0;
}

static const char *test_edit_scrolling(void)
{
    static ed_app a;
    char text[600];
    size_t n = 0;
    for (int i = 0; i < 30; i++)
        n += (size_t)snprintf(text + n, sizeof(text) - n, "line %02d\n", i);
    snprintf(text + n, sizeof(text) - n, "%s", "a very long last line that goes on and on past the edge");
    ed_open(&a, text, 20, 5);
    tui_edit *e = &a.e;
    r2_assert("first lines", strcmp(ed_row(0, 20), "line 00") == 0 && strcmp(ed_row(4, 20), "line 04") == 0);
    ed_keys("\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B");
    r2_assert("Down past the bottom scrolls one line", e->top_line == 1 && strcmp(ed_row(4, 20), "line 05") == 0);
    r2_assert("cursor on the bottom row", ed_t.cur_y == 2 + 4);
    ed_keys("\x1b[6~");
    r2_assert("PgDn moves 4 lines", e->line == 9 && strcmp(ed_row(4, 20), "line 09") == 0);
    ed_keys("\x1b[1;5F");
    r2_assert("Ctrl+End: last line", e->line == 30 && e->top_line == 26);
    r2_assert("scrolled sideways to the cursor", e->left > 0 && ed_t.cur_x == 1 + 19);
    r2_assert("the end of the line is shown", strcmp(ed_row(4, 20), "on and on past the e") == 0 ||
                                                   strstr(ed_row(4, 20), "edge") != 0);
    ed_keys("\x1b[H");
    r2_assert("Home scrolls back", e->left == 0 && strcmp(ed_row(4, 20), "a very long last lin") == 0);
    ed_keys("\x1b[1;5H");
    r2_assert("Ctrl+Home: top", e->top_line == 0 && strcmp(ed_row(0, 20), "line 00") == 0);
    /* a wide character cut by the left edge shows as blanks */
    /* 6 wide chars (12 columns) in a 6 column editor, cursor at the end:
       the view starts at column 7, in the middle of the 4th character */
    ed_open(&a, "\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD", 6, 2);
    ed_keys("\x1b[F");
    r2_assert("view starts mid character", a.e.left == 7);
    r2_assert("half a wide char at the left edge is a blank",
              TUI_CELL_RUNE(tui_get_cell(&ed_t, 1, 2)) == ' ' && TUI_CELL_RUNE(tui_get_cell(&ed_t, 2, 2)) == 0x4E2D);
    r2_assert("cursor after the last one", ed_t.cur_x == 1 + 5);
    return 0;
}

static const char *test_edit_focus(void)
{
    /* Tab types a tab; Shift+Tab leaves for the widget before */
    static ed_app a;
    a.with_button = 1;
    ed_open(&a, "x", 20, 4);
    r2_assert("button first: it has focus", tui_focused_window(&ed_t)->focus_kind == R2__TUI_W_BUTTON);
    ed_keys("\t");
    r2_assert("Tab moves to the editor", tui_focused_window(&ed_t)->focus_kind == R2__TUI_W_TEXT);
    ed_keys("\t");
    r2_assert("in the editor Tab is a tab", strcmp(ed_text(&a.e), "\tx") == 0 &&
                                               tui_focused_window(&ed_t)->focus_kind == R2__TUI_W_TEXT);
    ed_keys("b");
    r2_assert("letters go into the text, not to the button", a.presses == 0 && strcmp(ed_text(&a.e), "\tbx") == 0);
    ed_keys("\x1b[Z");
    r2_assert("Shift+Tab leaves", tui_focused_window(&ed_t)->focus_kind == R2__TUI_W_BUTTON);
    a.with_button = 0;
    return 0;
}

static const char *test_edit_random_keys(void)
{
    static const char *const keys[] = {"a", "\xC3\xA9", "\xE4\xB8\xAD", "\r", "\t", "\x7f", "\x1b[3~", "\x1b[A",
                                       "\x1b[B", "\x1b[C", "\x1b[D", "\x1b[H", "\x1b[F", "\x1b[5~", "\x1b[6~",
                                       "\x1b[1;5H", "\x1b[1;5F", "\x1b[1;5C", "\x1b[1;5D", "\x01", "\x05",
                                       "\x0b", "\x04", "\x10", "\x0e", " ", "\x1b\x80"};
    static ed_app a;
    ed_open(&a, "start\n\there", 30, 6);
    for (int iter = 0; iter < 20000; iter++)
    {
        const char *err;
        if (ed_rand() % 500 == 0)
            a.e.flags ^= TUI_EDIT_LINE_NUMBERS | (ed_rand() & 1 ? TUI_EDIT_AUTO_INDENT : 0);
        ed_keys(keys[ed_rand() % (sizeof(keys) / sizeof(keys[0]))]);
        err = ed_check(&a.e);
        r2_assert(err ? err : "", !err);
        r2_assert("cursor row on screen", a.e.line >= a.e.top_line && a.e.line < a.e.top_line + 6);
        r2_assert("cursor column on screen", ed_t.cur_x >= 1 && ed_t.cur_x < 1 + 30);
        r2_assert("text stays valid UTF-8", s8_is_valid_utf8(s8_from_cstr(ed_text(&a.e))));
    }
    return 0;
}

////////////////////////////////////////////

static const char *r2_tui_edit_test(void)
{
    r2_run_test(test_edit_basics);
    r2_run_test(test_edit_full);
    r2_run_test(test_edit_model);
    r2_run_test(test_edit_bad_utf8);
    r2_run_test(test_edit_moving);
    r2_run_test(test_edit_typing);
    r2_run_test(test_edit_drawing);
    r2_run_test(test_edit_scrolling);
    r2_run_test(test_edit_focus);
    r2_run_test(test_edit_random_keys);
    return 0;
}
