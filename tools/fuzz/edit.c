/*
    The editor widget driven by arbitrary keys: kill / yank, undo, search
    and query replace, with small buffers so the full paths get hit.

    Data: flags, width, height, n, n bytes of starting text (any bytes),
    then the key stream (see fuzz.h).
*/
#define R2_TUI_IMPLEMENTATION
#include "../../r2_tui.h"
#define R2_TUI_EDIT_IMPLEMENTATION
#include "../../r2_tui_edit.h"
#include "fuzz.h"

#include <string.h>

#define COLS 60
#define ROWS 20

static uint8_t arena[R2_TUI_ARENA_SIZE(COLS, ROWS)];

typedef struct app
{
    tui_edit e;
    char buf[256];
    tui_clipboard clip;
    char clipbuf[48];
    char hist[512];
    int w, h;
} app;

static void app_ui(tui *t, tui_window *w, void *user)
{
    app *a = (app *)user;
    (void)w;
    tui_at(t, 0, 0);
    tui_editor(t, "##ed", &a->e, a->w, a->h);
}

/* recount what the editor tracks incrementally (as tests/r2_tui_edit.c does) */
static void check(const app *a, int text_valid)
{
    static char s[sizeof(a->buf) + 1];
    const tui_edit *e = &a->e;
    uint32_t len = tui_edit_get(e, s, sizeof(s)), nl = 0, before_cursor = 0, before_top = 0, i;
    uint32_t from, to;
    FUZZ_CHECK("len", len == tui_edit_len(e) && len <= sizeof(a->buf));
    for (i = 0; i < len; i++)
    {
        nl += s[i] == '\n';
        if (i < e->cursor)
            before_cursor += s[i] == '\n';
        if (i < e->top)
            before_top += s[i] == '\n';
    }
    FUZZ_CHECK("line count", e->lines == nl + 1);
    FUZZ_CHECK("cursor past the end", e->cursor <= len);
    FUZZ_CHECK("cursor line", e->line == before_cursor);
    FUZZ_CHECK("top is a line start", e->top <= len && (e->top == 0 || s[e->top - 1] == '\n'));
    FUZZ_CHECK("top line number", e->top_line == before_top);
    FUZZ_CHECK("mark past the end", e->mark <= len);
    if (tui_edit_selection(e, &from, &to))
        FUZZ_CHECK("selection in range", from <= to && to <= len);
    FUZZ_CHECK("clipboard overflow", a->clip.len <= a->clip.cap);
    if (text_valid)
    {
        FUZZ_CHECK("text stays UTF-8", s8_is_valid_utf8(s8_from_bytes(s, len)));
        FUZZ_CHECK("clipboard holds whole characters", s8_is_valid_utf8(s8_from_bytes(a->clipbuf, a->clip.len)));
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static app a;
    fuzz_stream s;
    uint8_t chunk[FUZZ_CHUNK_MAX], q[64];
    size_t n, text_len;
    uint32_t ms, now = 1000;
    int what, text_valid;
    tui t;
    tui_window *win;
    if (size < 4)
        return 0;
    text_len = data[3] % sizeof(a.buf);
    if (size < 4 + text_len)
        return 0;
    memset(&a, 0, sizeof(a));
    a.w = 1 + data[1] % (COLS - 2);
    a.h = 1 + data[2] % (ROWS - 3);
    tui_init(&t, arena, sizeof(arena), COLS, ROWS);
    tui_edit_init(&a.e, a.buf, sizeof(a.buf));
    a.e.flags = data[0] & (TUI_EDIT_LINE_NUMBERS | TUI_EDIT_AUTO_INDENT);
    FUZZ_CHECK("set", tui_edit_set(&a.e, s8_from_bytes(data + 4, (uint32_t)text_len)));
    text_valid = s8_is_valid_utf8(s8_from_bytes(data + 4, (uint32_t)text_len));
    tui_clipboard_init(&a.clip, a.clipbuf, sizeof(a.clipbuf));
    a.e.clip = &a.clip;
    tui_edit_undo_init(&a.e, a.hist, (data[0] & 0x80) ? 64 : sizeof(a.hist));
    win = tui_window_open(&t, "Ed", (tui_rect){0, 1, a.w + 2, a.h + 2}, 0, app_ui, &a);
    FUZZ_CHECK("open", win);
    tui_update(&t, now);
    check(&a, text_valid);

    s = (fuzz_stream){data + 4 + text_len, size - 4 - text_len, 0};
    while ((what = fuzz_next(&s, chunk, &n, &ms)) != FUZZ_END)
    {
        if (what == FUZZ_QUERY)
            tui_query_size(&t, q, sizeof(q));
        else
        {
            tui_feed(&t, chunk, n, now);
            now += ms;
        }
        tui_update(&t, now);
        fuzz_render(&t);
        check(&a, text_valid);
        if (tui_focused_window(&t) == win && t.cols == COLS && t.rows == ROWS)
            FUZZ_CHECK("cursor row on screen", a.e.line >= a.e.top_line && a.e.line < a.e.top_line + (uint32_t)a.h);
    }
    return 0;
}
