#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"
#include "../r2_unit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VT_COLS 60
#define VT_ROWS 20

/* feed a string literal (length from sizeof, so embedded NULs are fine) */
#define TUI_FEED(t, lit, now) tui_feed((t), (const uint8_t *)(lit), sizeof(lit) - 1, (now))

static uint8_t tui_test_arena[R2_TUI_ARENA_SIZE(VT_COLS, VT_ROWS)];

static uint32_t tui_seed = 0x1234567u;
static uint32_t tui_rand(void)
{
    tui_seed ^= tui_seed << 13;
    tui_seed ^= tui_seed >> 17;
    tui_seed ^= tui_seed << 5;
    return tui_seed;
}

///////////////////////////////////////////////////////////////
// A small model terminal: applies the renderer's output to a grid,
// the way xterm would, and complains about anything unexpected.

typedef struct vt_cell
{
    rune r; /* TUI_RUNE_CONT for the right half of a wide char */
    tui_style style;
    rune mark;
} vt_cell;

static vt_cell vt[VT_COLS * VT_ROWS];
static int vt_cols, vt_rows, vt_x, vt_y, vt_cursor_on, vt_sync;
static tui_style vt_style;
static const char *vt_err;

static void vt_reset(int cols, int rows)
{
    vt_cols = cols;
    vt_rows = rows;
    for (int i = 0; i < cols * rows; i++)
    {
        vt[i].r = 0xFFFF; /* garbage that must be painted over */
        vt[i].style = 0;
        vt[i].mark = 0;
    }
    vt_x = vt_y = 0;
    vt_cursor_on = 1;
    vt_sync = 0;
    vt_style = 0;
    vt_err = 0;
}

static void vt_print(rune r)
{
    int w = tui_rune_width(r);
    if (w == 0)
    {
        int px = vt_x - 1;
        if (px >= 0 && vt[vt_y * vt_cols + px].r == TUI_RUNE_CONT)
            px--;
        if (px < 0)
            vt_err = "vt: mark with nothing before it";
        else if (vt[vt_y * vt_cols + px].mark)
            vt_err = "vt: second mark on a cell";
        else
            vt[vt_y * vt_cols + px].mark = r;
        return;
    }
    if (vt_x < 0 || vt_y < 0 || vt_y >= vt_rows || vt_x + w > vt_cols)
    {
        vt_err = "vt: printed past the right edge (relied on wrap)";
        return;
    }
    vt_cell *c = &vt[vt_y * vt_cols + vt_x];
    /* breaking a wide char blanks its other half */
    if (c->r == TUI_RUNE_CONT && vt_x > 0)
        c[-1].r = ' ', c[-1].mark = 0;
    if (c->r != TUI_RUNE_CONT && vt_x + 1 < vt_cols && c[1].r == TUI_RUNE_CONT)
        c[1].r = ' ';
    if (w == 2 && vt_x + 2 < vt_cols && c[1].r != TUI_RUNE_CONT && c[2].r == TUI_RUNE_CONT)
        c[2].r = ' ';
    c->r = r;
    c->style = vt_style;
    c->mark = 0;
    if (w == 2)
    {
        c[1].r = TUI_RUNE_CONT;
        c[1].style = vt_style;
        c[1].mark = 0;
    }
    vt_x += w;
}

static void vt_sgr(const uint32_t *p, int np)
{
    uint32_t fg = 7, bg = 0, attr = 0;
    if (np == 0 || p[0] != 0)
    {
        vt_err = "vt: SGR without a leading reset";
        return;
    }
    for (int i = 1; i < np; i++)
    {
        uint32_t v = p[i];
        if (v == 1)
            attr |= TUI_BOLD;
        else if (v == 4)
            attr |= TUI_UNDERLINE;
        else if (v == 7)
            attr |= TUI_REVERSE;
        else if (v >= 30 && v <= 37)
            fg = v - 30;
        else if (v >= 90 && v <= 97)
            fg = v - 90 + 8;
        else if (v >= 40 && v <= 47)
            bg = v - 40;
        else if (v >= 100 && v <= 107)
            bg = v - 100 + 8;
        else
            vt_err = "vt: unknown SGR";
    }
    vt_style = TUI_STYLE(fg, bg, attr);
}

static void vt_feed(const uint8_t *b, size_t n)
{
    size_t i = 0;
    while (i < n && !vt_err)
    {
        if (b[i] == 0x1B)
        {
            uint32_t p[8] = {0};
            int np = 0, have = 0, priv = 0;
            size_t j = i + 1;
            if (j >= n || b[j] != '[')
            {
                vt_err = "vt: ESC not followed by [";
                return;
            }
            j++;
            if (j < n && b[j] == '?')
                priv = 1, j++;
            for (; j < n && ((b[j] >= '0' && b[j] <= '9') || b[j] == ';'); j++)
            {
                if (b[j] == ';')
                    np++;
                else if (np < 8)
                    p[np] = p[np] * 10 + (uint32_t)(b[j] - '0'), have = 1;
            }
            if (have || np)
                np++;
            if (j >= n)
            {
                vt_err = "vt: sequence split across chunks";
                return;
            }
            switch (b[j])
            {
            case 'H':
                vt_y = (int)(np > 0 && p[0] ? p[0] : 1) - 1;
                vt_x = (int)(np > 1 && p[1] ? p[1] : 1) - 1;
                break;
            case 'C':
                vt_x += (int)(np > 0 && p[0] ? p[0] : 1);
                break;
            case 'm':
                vt_sgr(p, np);
                break;
            case 'h':
            case 'l':
                if (!priv)
                    vt_err = "vt: unexpected mode";
                else if (p[0] == 2026)
                    vt_sync = b[j] == 'h';
                else if (p[0] == 25)
                    vt_cursor_on = b[j] == 'h';
                else
                    vt_err = "vt: unexpected private mode";
                break;
            default:
                vt_err = "vt: unexpected CSI";
            }
            i = j + 1;
        }
        else if (b[i] < 0x20 || b[i] == 0x7F)
        {
            vt_err = "vt: raw control byte in output";
            return;
        }
        else
        {
            rune r;
            uint32_t used = utf8_decode(b + i, (uint32_t)(n - i), &r);
            if (r == R2_RUNE_REPLACEMENT && !(used == 3 && b[i] == 0xEF))
            {
                vt_err = "vt: invalid UTF-8 in output";
                return;
            }
            vt_print(r);
            i += used;
        }
    }
}

/* Render the whole frame in chunks of random size, feeding the model */
static size_t vt_flush(tui *t, int random_chunks)
{
    uint8_t buf[512];
    size_t total = 0, n;
    for (;;)
    {
        size_t cap = random_chunks ? R2_TUI_RENDER_MIN + tui_rand() % 200 : sizeof(buf);
        n = tui_render(t, buf, cap);
        if (n == 0)
            break;
        vt_feed(buf, n);
        total += n;
    }
    return total;
}

/* 1 if the model shows exactly the back buffer */
static int vt_matches(const tui *t)
{
    for (int y = 0; y < t->rows; y++)
    {
        for (int x = 0; x < t->cols; x++)
        {
            tui_cell c = tui_get_cell(t, x, y);
            const vt_cell *v = &vt[y * vt_cols + x];
            rune r = TUI_CELL_RUNE(c);
            if (v->r != (r ? r : ' '))
                return 0;
            if (r != TUI_RUNE_CONT && v->style != TUI_CELL_STYLE(c))
                return 0;
            if (v->mark != tui_get_mark(t, x, y))
                return 0;
        }
    }
    return 1;
}

static int tui_test_init(tui *t, int cols, int rows)
{
    vt_reset(cols, rows);
    return tui_init(t, tui_test_arena, sizeof(tui_test_arena), cols, rows);
}

///////////////////////////////////////////////////////////////
// Setup

static const char *test_tui_init_sizes(void)
{
    tui t;
    static uint8_t small[64];
    /* 16KB of cells plus the combining marks, windows, commands and menus */
    r2_assert("arena for 80x25 should be under 32KB",
              R2_TUI_ARENA_SIZE(80, 25) > 16000 && R2_TUI_ARENA_SIZE(80, 25) < 32 * 1024);
    r2_assert("init in a tiny arena should fail", tui_init(&t, small, sizeof(small), 10, 10) == -1);
    r2_assert("init too big for the arena should fail",
              tui_init(&t, tui_test_arena, sizeof(tui_test_arena), VT_COLS + 1, VT_ROWS) == -1);
    r2_assert("init should succeed", tui_test_init(&t, VT_COLS, VT_ROWS) == 0);
    r2_assert("resize smaller should succeed", tui_resize(&t, 20, 5) == 0 && t.cols == 20 && t.rows == 5);
    r2_assert("resize past the arena should fail", tui_resize(&t, VT_COLS, VT_ROWS + 1) == -1);
    r2_assert("resize to zero should fail", tui_resize(&t, 0, 5) == -1);
    return 0;
}

///////////////////////////////////////////////////////////////
// Width

static const char *test_tui_width_table_matches_generator(void)
{
    /* FNV-1a over the width of every codepoint, as the generator does */
    uint32_t h = 0x811C9DC5u;
    for (rune r = 0; r < 0x110000u; r++)
        h = (h ^ (uint32_t)tui_rune_width(r)) * 0x01000193u;
    r2_assert("width table disagrees with tools/gen_tui_width.py", h == R2_TUI_WIDTH_CHECKSUM);
    return 0;
}

static const char *test_tui_width_spot(void)
{
    r2_assert("a", tui_rune_width('a') == 1);
    r2_assert("e acute", tui_rune_width(0xE9) == 1);
    r2_assert("combining acute", tui_rune_width(0x301) == 0);
    r2_assert("CJK", tui_rune_width(0x4E2D) == 2);
    r2_assert("emoji", tui_rune_width(0x1F44B) == 2);
    r2_assert("fullwidth !", tui_rune_width(0xFF01) == 2);
    r2_assert("soft hyphen", tui_rune_width(0xAD) == 1);
    r2_assert("box drawing", tui_rune_width(0x2500) == 1);
    r2_assert("ZWJ", tui_rune_width(0x200D) == 0);
    r2_assert("text width", tui_text_width(S8("A\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x91\x8B")) == 6);
    r2_assert("text width combining", tui_text_width(S8("e\xCC\x81")) == 1);
    return 0;
}

///////////////////////////////////////////////////////////////
// Drawing

static const char *test_tui_draw_text_cells(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_YELLOW, TUI_BLUE, TUI_BOLD);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    /* A é 中 👋 */
    r2_assert("draw_text width", tui_draw_text(&t, 0, 0, S8("A\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x91\x8B"), s) == 6);
    r2_assert("cell 0", tui_get_cell(&t, 0, 0) == TUI_CELL('A', s));
    r2_assert("cell 1", tui_get_cell(&t, 1, 0) == TUI_CELL(0xE9, s));
    r2_assert("cell 2", tui_get_cell(&t, 2, 0) == TUI_CELL(0x4E2D, s));
    r2_assert("cell 3 cont", TUI_CELL_RUNE(tui_get_cell(&t, 3, 0)) == TUI_RUNE_CONT);
    r2_assert("cell 4", tui_get_cell(&t, 4, 0) == TUI_CELL(0x1F44B, s));
    r2_assert("cell 5 cont", TUI_CELL_RUNE(tui_get_cell(&t, 5, 0)) == TUI_RUNE_CONT);
    r2_assert("cell 6 untouched", TUI_CELL_RUNE(tui_get_cell(&t, 6, 0)) == ' ');
    return 0;
}

static const char *test_tui_wide_at_clip_edge(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_WHITE, TUI_BLACK, 0);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_clip(&t, (tui_rect){0, 0, 5, 1});
    tui_draw_text(&t, 0, 0, S8("abcd\xE4\xB8\xAD"), s);
    r2_assert("wide char cut by the clip becomes a space", tui_get_cell(&t, 4, 0) == TUI_CELL(' ', s));
    r2_assert("nothing drawn past the clip", TUI_CELL_RUNE(tui_get_cell(&t, 5, 0)) == ' ');
    tui_clip_reset(&t);
    tui_draw_text(&t, VT_COLS - 1, 1, S8("\xE4\xB8\xAD"), s);
    r2_assert("wide char at the screen edge becomes a space", tui_get_cell(&t, VT_COLS - 1, 1) == TUI_CELL(' ', s));
    tui_clip(&t, (tui_rect){10, 2, 5, 1});
    tui_draw_text(&t, 9, 2, S8("\xE4\xB8\xAD"), s);
    r2_assert("right half inside the clip becomes a space", tui_get_cell(&t, 10, 2) == TUI_CELL(' ', s));
    r2_assert("left half outside the clip untouched", TUI_CELL_RUNE(tui_get_cell(&t, 9, 2)) == ' ');
    return 0;
}

static const char *test_tui_overwrite_half_of_wide(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_WHITE, TUI_BLACK, 0);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_text(&t, 0, 0, S8("\xE4\xB8\xAD\xE4\xB8\xAD"), s); /* 中中 in cells 0-3 */
    tui_draw_rune(&t, 1, 0, 'x', s);
    r2_assert("left half blanked", TUI_CELL_RUNE(tui_get_cell(&t, 0, 0)) == ' ');
    r2_assert("x written", TUI_CELL_RUNE(tui_get_cell(&t, 1, 0)) == 'x');
    tui_draw_rune(&t, 2, 0, 'y', s);
    r2_assert("right half blanked", TUI_CELL_RUNE(tui_get_cell(&t, 3, 0)) == ' ');
    /* a wide char over the boundary between two wide chars */
    tui_draw_text(&t, 0, 1, S8("\xE4\xB8\xAD\xE4\xB8\xAD"), s);
    tui_draw_rune(&t, 1, 1, 0x1F44B, s);
    r2_assert("boundary: left blanked", TUI_CELL_RUNE(tui_get_cell(&t, 0, 1)) == ' ');
    r2_assert("boundary: new lead", TUI_CELL_RUNE(tui_get_cell(&t, 1, 1)) == 0x1F44B);
    r2_assert("boundary: new cont", TUI_CELL_RUNE(tui_get_cell(&t, 2, 1)) == TUI_RUNE_CONT);
    r2_assert("boundary: right blanked", TUI_CELL_RUNE(tui_get_cell(&t, 3, 1)) == ' ');
    return 0;
}

static const char *test_tui_combining_and_dropped(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_WHITE, TUI_BLACK, 0);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    r2_assert("e + acute is one column", tui_draw_text(&t, 0, 0, S8("e\xCC\x81"), s) == 1);
    r2_assert("mark attached", tui_get_mark(&t, 0, 0) == 0x301);
    tui_draw_rune(&t, 0, 0, 'z', s);
    r2_assert("overwrite clears the mark", tui_get_mark(&t, 0, 0) == 0);
    /* mark after a wide char goes on the lead */
    tui_draw_text(&t, 0, 1, S8("\xE4\xB8\xAD\xCC\x81"), s);
    r2_assert("mark on wide lead", tui_get_mark(&t, 0, 1) == 0x301);
    /* man ZWJ woman: drawn as the two parts, no joiner */
    r2_assert("ZWJ dropped",
              tui_draw_text(&t, 0, 2, S8("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9"), s) == 4);
    r2_assert("ZWJ not a mark", tui_get_mark(&t, 0, 2) == 0 && tui_get_mark(&t, 1, 2) == 0);
    r2_assert("second emoji", TUI_CELL_RUNE(tui_get_cell(&t, 2, 2)) == 0x1F469);
    /* a mark with nothing before it is dropped */
    tui_draw_text(&t, 0, 3, S8("\xCC\x81"), s);
    r2_assert("lone mark dropped", tui_get_mark(&t, 0, 3) == 0);
    return 0;
}

static const char *test_tui_controls_sanitized(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_WHITE, TUI_BLACK, 0);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_text(&t, 0, 0, S8("\x1b[31m\x07\xC2\x9B"), s);
    r2_assert("ESC drawn as ?", TUI_CELL_RUNE(tui_get_cell(&t, 0, 0)) == '?');
    r2_assert("[ kept", TUI_CELL_RUNE(tui_get_cell(&t, 1, 0)) == '[');
    r2_assert("BEL drawn as ?", TUI_CELL_RUNE(tui_get_cell(&t, 5, 0)) == '?');
    r2_assert("C1 CSI drawn as ?", TUI_CELL_RUNE(tui_get_cell(&t, 6, 0)) == '?');
    return 0;
}

static const char *test_tui_box_fill_shadow(void)
{
    tui t;
    tui_style s = TUI_STYLE(TUI_WHITE, TUI_BLUE, 0);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_box(&t, (tui_rect){1, 1, 5, 4}, s, TUI_BOX_DOUBLE);
    r2_assert("top left", TUI_CELL_RUNE(tui_get_cell(&t, 1, 1)) == 0x2554);
    r2_assert("top right", TUI_CELL_RUNE(tui_get_cell(&t, 5, 1)) == 0x2557);
    r2_assert("bottom left", TUI_CELL_RUNE(tui_get_cell(&t, 1, 4)) == 0x255A);
    r2_assert("bottom right", TUI_CELL_RUNE(tui_get_cell(&t, 5, 4)) == 0x255D);
    r2_assert("top edge", TUI_CELL_RUNE(tui_get_cell(&t, 3, 1)) == 0x2550);
    r2_assert("side edge", TUI_CELL_RUNE(tui_get_cell(&t, 1, 2)) == 0x2551);
    r2_assert("inside untouched", TUI_CELL_RUNE(tui_get_cell(&t, 3, 2)) == ' ');
    tui_draw_fill(&t, (tui_rect){10, 0, 3, 2}, 0x2591, s);
    r2_assert("fill", tui_get_cell(&t, 11, 1) == TUI_CELL(0x2591, s));
    tui_draw_fill(&t, (tui_rect){20, 0, 2, 1}, 0x4E2D, s);
    r2_assert("wide fill rune becomes space", TUI_CELL_RUNE(tui_get_cell(&t, 20, 0)) == ' ');
    tui_draw_shadow(&t, (tui_rect){10, 0, 1, 1});
    r2_assert("shadow keeps the rune", TUI_CELL_RUNE(tui_get_cell(&t, 10, 0)) == 0x2591);
    r2_assert("shadow restyles",
              TUI_CELL_STYLE(tui_get_cell(&t, 10, 0)) == TUI_STYLE(TUI_DARK_GRAY, TUI_BLACK, 0));
    tui_clip(&t, (tui_rect){50, 50, 5, 5});
    tui_draw_text(&t, 0, 0, S8("nope"), s);
    r2_assert("clip off screen draws nothing", TUI_CELL_RUNE(tui_get_cell(&t, 0, 0)) == ' ');
    return 0;
}

///////////////////////////////////////////////////////////////
// Renderer

static const char *test_tui_render_first_frame(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_text(&t, 2, 3, S8("Hello \xE4\xB8\xAD e\xCC\x81"), TUI_STYLE(TUI_YELLOW, TUI_BLUE, TUI_BOLD));
    tui_set_cursor(&t, 5, 6, 1);
    vt_flush(&t, 0);
    r2_assert(vt_err ? vt_err : "first frame", !vt_err);
    r2_assert("first frame does not match the back buffer", vt_matches(&t));
    r2_assert("sync left on", !vt_sync);
    r2_assert("cursor not shown", vt_cursor_on);
    r2_assert("cursor position wrong", vt_x == 5 && vt_y == 6);
    r2_assert("nothing left to render", !tui_render_pending(&t));
    return 0;
}

static const char *test_tui_render_minimal_diff(void)
{
    tui t;
    uint8_t buf[256];
    size_t n;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    vt_flush(&t, 0);
    r2_assert("idle render should send nothing", tui_render(&t, buf, sizeof(buf)) == 0);
    tui_draw_rune(&t, 10, 5, 'x', TUI_STYLE(TUI_RED, TUI_BLACK, 0));
    n = vt_flush(&t, 0);
    r2_assert(vt_err ? vt_err : "diff", !vt_err);
    r2_assert("one changed cell should cost under 50 bytes", n < 50);
    r2_assert("diff frame does not match", vt_matches(&t));
    /* redrawing the same thing sends nothing at all */
    tui_draw_rune(&t, 10, 5, 'x', TUI_STYLE(TUI_RED, TUI_BLACK, 0));
    tui_draw_shadow(&t, (tui_rect){0, 0, 1, 1});
    tui_draw_shadow(&t, (tui_rect){0, 0, 1, 1});
    vt_flush(&t, 0);
    tui_draw_shadow(&t, (tui_rect){0, 0, 1, 1});
    tui_draw_rune(&t, 10, 5, 'x', TUI_STYLE(TUI_RED, TUI_BLACK, 0));
    r2_assert("identical redraw should not start a frame", !tui_render_pending(&t));
    n = vt_flush(&t, 0);
    r2_assert("identical redraw should send nothing", n == 0);
    /* text with a combining mark is rewritten internally (mark cleared and
       set again): still nothing to send */
    tui_draw_text(&t, 0, 7, S8("e\xCC\x81"), TUI_STYLE(TUI_RED, TUI_BLACK, 0));
    vt_flush(&t, 0);
    tui_draw_text(&t, 0, 7, S8("e\xCC\x81"), TUI_STYLE(TUI_RED, TUI_BLACK, 0));
    n = vt_flush(&t, 0);
    r2_assert("identical redraw with a mark should send nothing", n == 0);
    /* only the cursor moved: a short frame with no cells */
    tui_set_cursor(&t, 3, 3, 1);
    n = vt_flush(&t, 0);
    r2_assert(vt_err ? vt_err : "cursor only", !vt_err);
    r2_assert("cursor-only frame", n > 0 && n < 40 && vt_x == 3 && vt_y == 3 && vt_cursor_on);
    return 0;
}

static const char *test_tui_render_small_cap(void)
{
    tui t;
    uint8_t buf[R2_TUI_RENDER_MIN];
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_text(&t, 0, 0, S8("x"), TUI_STYLE(TUI_WHITE, TUI_BLACK, 0));
    r2_assert("cap below the minimum renders nothing", tui_render(&t, buf, R2_TUI_RENDER_MIN - 1) == 0);
    r2_assert("still pending", tui_render_pending(&t));
    return 0;
}

static const char *test_tui_render_random(void)
{
    /* Random drawing, rendered in random-sized chunks, sometimes drawing
       in the middle of a frame. After every full flush the model terminal
       must show exactly the back buffer. */
    static const char *const words[] = {"hello",
                                        "\xC3\xA9t\xC3\xA9",
                                        "\xE4\xB8\xAD\xE6\x96\x87",
                                        "\xF0\x9F\x91\x8B!",
                                        "e\xCC\x81\xCC\x88",
                                        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9",
                                        "\x1b[2J",
                                        "\xE2\x94\x80\xE2\x94\x80"};
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (int iter = 0; iter < 3000; iter++)
    {
        int ops = 1 + (int)(tui_rand() % 8);
        for (int k = 0; k < ops; k++)
        {
            uint32_t r = tui_rand();
            int x = (int)(tui_rand() % (VT_COLS + 4)) - 2;
            int y = (int)(tui_rand() % (VT_ROWS + 2)) - 1;
            tui_style s = (tui_style)(tui_rand() & 0x7FF);
            switch (r % 6)
            {
            case 0:
            case 1:
                tui_draw_text(&t, x, y, s8_from_cstr(words[tui_rand() % 8]), s);
                break;
            case 2:
                tui_draw_fill(&t, (tui_rect){x, y, (int)(tui_rand() % 10), (int)(tui_rand() % 4)}, '.', s);
                break;
            case 3:
                tui_draw_box(&t, (tui_rect){x, y, (int)(tui_rand() % 12), (int)(tui_rand() % 6)}, s,
                             (int)(tui_rand() & 1));
                break;
            case 4:
                tui_draw_shadow(&t, (tui_rect){x, y, (int)(tui_rand() % 10), (int)(tui_rand() % 4)});
                break;
            case 5:
                if (tui_rand() & 1)
                    tui_clip(&t, (tui_rect){x, y, (int)(tui_rand() % 20), (int)(tui_rand() % 8)});
                else
                    tui_clip_reset(&t);
                break;
            }
        }
        tui_set_cursor(&t, (int)(tui_rand() % VT_COLS), (int)(tui_rand() % VT_ROWS), (int)(tui_rand() & 1));

        /* a partial frame, then more drawing, then finish */
        if (tui_rand() % 3 == 0)
        {
            uint8_t buf[R2_TUI_RENDER_MIN + 40];
            size_t n = tui_render(&t, buf, sizeof(buf));
            vt_feed(buf, n);
            tui_clip_reset(&t);
            tui_draw_text(&t, (int)(tui_rand() % VT_COLS), (int)(tui_rand() % VT_ROWS),
                          s8_from_cstr(words[tui_rand() % 8]), (tui_style)(tui_rand() & 0x7FF));
        }
        vt_flush(&t, 1);
        r2_assert(vt_err ? vt_err : "random", !vt_err);
        r2_assert("random: terminal does not match the back buffer", vt_matches(&t));
        r2_assert("random: sync left on", !vt_sync);
        r2_assert("random: cursor visibility wrong", vt_cursor_on == t.cur_visible);
        if (t.cur_visible)
            r2_assert("random: cursor position wrong", vt_x == t.cur_x && vt_y == t.cur_y);
    }
    return 0;
}

static const char *test_tui_render_resize_repaints(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_draw_text(&t, 0, 0, S8("before"), TUI_STYLE(TUI_WHITE, TUI_BLACK, 0));
    vt_flush(&t, 0);
    tui_resize(&t, 20, 6);
    vt_reset(20, 6);
    tui_draw_text(&t, 0, 0, S8("after"), TUI_STYLE(TUI_WHITE, TUI_BLACK, 0));
    vt_flush(&t, 0);
    r2_assert(vt_err ? vt_err : "resize", !vt_err);
    r2_assert("resize did not repaint everything", vt_matches(&t));
    return 0;
}

///////////////////////////////////////////////////////////////
// Input

typedef struct key_case
{
    const char *bytes;
    size_t len; /* 0: strlen */
    uint16_t key;
    rune ch;
    uint8_t mods;
} key_case;

static const key_case key_cases[] = {
    {"\x1b[A", 0, TUI_KEY_UP, 0, 0},
    {"\x1b[B", 0, TUI_KEY_DOWN, 0, 0},
    {"\x1b[C", 0, TUI_KEY_RIGHT, 0, 0},
    {"\x1b[D", 0, TUI_KEY_LEFT, 0, 0},
    {"\x1bOA", 0, TUI_KEY_UP, 0, 0},
    {"\x1b[1;5A", 0, TUI_KEY_UP, 0, TUI_MOD_CTRL},
    {"\x1b[1;2B", 0, TUI_KEY_DOWN, 0, TUI_MOD_SHIFT},
    {"\x1b[1;3C", 0, TUI_KEY_RIGHT, 0, TUI_MOD_ALT},
    {"\x1b[1;6D", 0, TUI_KEY_LEFT, 0, TUI_MOD_SHIFT | TUI_MOD_CTRL},
    {"\x1b[H", 0, TUI_KEY_HOME, 0, 0},
    {"\x1b[F", 0, TUI_KEY_END, 0, 0},
    {"\x1bOH", 0, TUI_KEY_HOME, 0, 0},
    {"\x1b[1~", 0, TUI_KEY_HOME, 0, 0},
    {"\x1b[4~", 0, TUI_KEY_END, 0, 0},
    {"\x1b[7~", 0, TUI_KEY_HOME, 0, 0},
    {"\x1b[2~", 0, TUI_KEY_INSERT, 0, 0},
    {"\x1b[3~", 0, TUI_KEY_DELETE, 0, 0},
    {"\x1b[3;5~", 0, TUI_KEY_DELETE, 0, TUI_MOD_CTRL},
    {"\x1b[2;2~", 0, TUI_KEY_INSERT, 0, TUI_MOD_SHIFT},
    {"\x1b[5~", 0, TUI_KEY_PGUP, 0, 0},
    {"\x1b[6~", 0, TUI_KEY_PGDN, 0, 0},
    {"\x1bOP", 0, TUI_KEY_F1, 0, 0},
    {"\x1bOQ", 0, TUI_KEY_F1 + 1, 0, 0},
    {"\x1bOR", 0, TUI_KEY_F1 + 2, 0, 0},
    {"\x1bOS", 0, TUI_KEY_F1 + 3, 0, 0},
    {"\x1b[[A", 0, TUI_KEY_F1, 0, 0},
    {"\x1b[[E", 0, TUI_KEY_F1 + 4, 0, 0},
    {"\x1b[11~", 0, TUI_KEY_F1, 0, 0},
    {"\x1b[15~", 0, TUI_KEY_F1 + 4, 0, 0},
    {"\x1b[17~", 0, TUI_KEY_F1 + 5, 0, 0},
    {"\x1b[21~", 0, TUI_KEY_F1 + 9, 0, 0},
    {"\x1b[23~", 0, TUI_KEY_F1 + 10, 0, 0},
    {"\x1b[24~", 0, TUI_KEY_F12, 0, 0},
    {"\x1b[1;5P", 0, TUI_KEY_F1, 0, TUI_MOD_CTRL},
    {"\x1b[1;2R", 0, TUI_KEY_F1 + 2, 0, TUI_MOD_SHIFT},
    {"\x1b[15;5~", 0, TUI_KEY_F1 + 4, 0, TUI_MOD_CTRL},
    {"\x1bO2P", 0, TUI_KEY_F1, 0, TUI_MOD_SHIFT},
    {"\x1b[Z", 0, TUI_KEY_TAB, 0, TUI_MOD_SHIFT},
    {"\x1bOM", 0, TUI_KEY_ENTER, 0, 0},
    {"\r", 0, TUI_KEY_ENTER, 0, 0},
    {"\n", 0, TUI_KEY_ENTER, 0, 0},
    {"\t", 0, TUI_KEY_TAB, 0, 0},
    {"\x7f", 0, TUI_KEY_BACKSPACE, 0, 0},
    {"\x08", 0, TUI_KEY_BACKSPACE, 0, 0},
    {"\x01", 0, TUI_KEY_RUNE, 'a', TUI_MOD_CTRL},
    {"\x11", 0, TUI_KEY_RUNE, 'q', TUI_MOD_CTRL},
    {"\x1a", 0, TUI_KEY_RUNE, 'z', TUI_MOD_CTRL},
    {"\x1c", 0, TUI_KEY_RUNE, '\\', TUI_MOD_CTRL},
    {"\0", 1, TUI_KEY_RUNE, ' ', TUI_MOD_CTRL},
    {"a", 0, TUI_KEY_RUNE, 'a', 0},
    {"Z", 0, TUI_KEY_RUNE, 'Z', 0},
    {" ", 0, TUI_KEY_RUNE, ' ', 0},
    {"\x1bx", 0, TUI_KEY_RUNE, 'x', TUI_MOD_ALT},
    {"\033f", 0, TUI_KEY_RUNE, 'f', TUI_MOD_ALT},
    {"\x1b\x7f", 0, TUI_KEY_BACKSPACE, 0, TUI_MOD_ALT},
    {"\x1b\r", 0, TUI_KEY_ENTER, 0, TUI_MOD_ALT},
    {"\x1b\xC3\xA9", 0, TUI_KEY_RUNE, 0xE9, TUI_MOD_ALT},
    {"\xC3\xA9", 0, TUI_KEY_RUNE, 0xE9, 0},
    {"\xE4\xB8\xAD", 0, TUI_KEY_RUNE, 0x4E2D, 0},
    {"\xF0\x9F\x91\x8B", 0, TUI_KEY_RUNE, 0x1F44B, 0},
};

static const char *check_one_key(tui *t, const key_case *c)
{
    tui_event ev;
    r2_assert("key: no event", tui_poll(t, &ev));
    r2_assert("key: wrong type", ev.type == TUI_EV_KEY);
    r2_assert("key: wrong key", ev.key == c->key);
    r2_assert("key: wrong char", c->key != TUI_KEY_RUNE || ev.ch == c->ch);
    r2_assert("key: wrong mods", ev.mods == c->mods);
    r2_assert("key: extra event", !tui_poll(t, &ev));
    return 0;
}

static const char *test_tui_keys_table(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (size_t i = 0; i < sizeof(key_cases) / sizeof(key_cases[0]); i++)
    {
        const key_case *c = &key_cases[i];
        size_t len = c->len ? c->len : strlen(c->bytes);
        const char *err;
        /* all at once */
        tui_feed(&t, (const uint8_t *)c->bytes, len, 1000);
        r2_assert("key: should not wait for more", !tui_input_pending(&t) || c->bytes[0] == 0x1b);
        tui_step(&t, 1000 + R2_TUI_ESC_MS);
        if ((err = check_one_key(&t, c)))
            return err;
        /* one byte at a time, quickly */
        for (size_t k = 0; k < len; k++)
            tui_feed(&t, (const uint8_t *)c->bytes + k, 1, 2000 + (uint32_t)k);
        tui_step(&t, 2000 + R2_TUI_ESC_MS + 10);
        if ((err = check_one_key(&t, c)))
            return err;
    }
    return 0;
}

static const char *test_tui_keys_back_to_back(void)
{
    /* every key followed directly by the next one, in one feed */
    tui t;
    tui_event ev;
    uint8_t buf[1024];
    size_t n = 0;
    size_t count = sizeof(key_cases) / sizeof(key_cases[0]);
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (size_t i = 0; i < count; i++)
    {
        size_t len = key_cases[i].len ? key_cases[i].len : strlen(key_cases[i].bytes);
        /* a lone-ESC-prefixed key directly after another can't be told
           apart from Alt+key, so separate them with 'a' */
        memcpy(buf + n, key_cases[i].bytes, len);
        n += len;
        buf[n++] = 'a';
    }
    /* feed in pieces so the queue never overflows */
    size_t at = 0, i = 0;
    while (at < n)
    {
        size_t piece = (n - at) < 7 ? n - at : 7;
        tui_feed(&t, buf + at, piece, 5000);
        at += piece;
        while (tui_poll(&t, &ev))
        {
            const key_case *c = &key_cases[i / 2];
            if (i % 2 == 0)
            {
                r2_assert("stream: wrong key", ev.key == c->key && ev.mods == c->mods);
                r2_assert("stream: wrong char", c->key != TUI_KEY_RUNE || ev.ch == c->ch);
            }
            else
                r2_assert("stream: separator lost", ev.key == TUI_KEY_RUNE && ev.ch == 'a' && ev.mods == 0);
            i++;
        }
    }
    r2_assert("stream: wrong number of events", i == 2 * count);
    return 0;
}

static const char *test_tui_esc_timeout(void)
{
    tui t;
    tui_event ev;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    TUI_FEED(&t, "\x1b", 100);
    r2_assert("ESC should wait", !tui_poll(&t, &ev) && tui_input_pending(&t));
    tui_step(&t, 100 + R2_TUI_ESC_MS - 1);
    r2_assert("ESC should still wait", !tui_poll(&t, &ev));
    tui_step(&t, 100 + R2_TUI_ESC_MS);
    r2_assert("ESC after timeout", tui_poll(&t, &ev) && ev.key == TUI_KEY_ESC && ev.mods == 0);
    /* ESC, then x much later: Esc and x, not Alt+x */
    TUI_FEED(&t, "\x1b", 200);
    TUI_FEED(&t, "x", 200 + R2_TUI_ESC_MS + 5);
    r2_assert("late x: ESC first", tui_poll(&t, &ev) && ev.key == TUI_KEY_ESC);
    r2_assert("late x: then x", tui_poll(&t, &ev) && ev.key == TUI_KEY_RUNE && ev.ch == 'x' && ev.mods == 0);
    /* ESC ESC: an Esc, then a pending ESC */
    TUI_FEED(&t, "\x1b\x1b", 300);
    r2_assert("ESC ESC: first", tui_poll(&t, &ev) && ev.key == TUI_KEY_ESC);
    tui_step(&t, 300 + R2_TUI_ESC_MS);
    r2_assert("ESC ESC: second", tui_poll(&t, &ev) && ev.key == TUI_KEY_ESC);
    /* timed out half sequences */
    TUI_FEED(&t, "\x1b[", 400);
    tui_step(&t, 400 + R2_TUI_ESC_MS);
    r2_assert("ESC [ alone is Alt+[", tui_poll(&t, &ev) && ev.key == TUI_KEY_RUNE && ev.ch == '[' &&
                                          ev.mods == TUI_MOD_ALT);
    TUI_FEED(&t, "\x1b[1;5", 500);
    tui_step(&t, 500 + R2_TUI_ESC_MS);
    r2_assert("cut off CSI is dropped", !tui_poll(&t, &ev) && !tui_input_pending(&t));
    TUI_FEED(&t, "\xE4\xB8", 600);
    tui_step(&t, 600 + R2_TUI_ESC_MS);
    r2_assert("cut off UTF-8 is U+FFFD", tui_poll(&t, &ev) && ev.ch == R2_RUNE_REPLACEMENT);
    return 0;
}

static const char *test_tui_bad_input(void)
{
    tui t;
    tui_event ev;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    TUI_FEED(&t, "\xFF", 0);
    r2_assert("0xFF is U+FFFD", tui_poll(&t, &ev) && ev.ch == R2_RUNE_REPLACEMENT);
    TUI_FEED(&t, "\xE4\xB8" "a", 0);
    r2_assert("broken UTF-8: FFFD", tui_poll(&t, &ev) && ev.ch == R2_RUNE_REPLACEMENT);
    r2_assert("broken UTF-8: then a", tui_poll(&t, &ev) && ev.ch == 'a');
    /* a control byte inside a CSI ends it; the control is still a key */
    TUI_FEED(&t, "\x1b[1\x01", 0);
    r2_assert("CSI broken by ^A", tui_poll(&t, &ev) && ev.ch == 'a' && ev.mods == TUI_MOD_CTRL);
    /* private / unknown sequences make no key */
    TUI_FEED(&t, "\x1b[?1;2c\x1b[200~\x1b[99~z", 0);
    r2_assert("unknown sequences ignored", tui_poll(&t, &ev) && ev.ch == 'z' && !tui_poll(&t, &ev));
    /* too long to be anything */
    TUI_FEED(&t, "\x1b[11111111111111111111111z", 0);
    while (tui_poll(&t, &ev))
        ;
    r2_assert("long garbage left input pending", !tui_input_pending(&t));
    /* the queue fills up rather than overflowing */
    for (int i = 0; i < 100; i++)
        TUI_FEED(&t, "q", 0);
    int count = 0;
    while (tui_poll(&t, &ev))
        count++;
    r2_assert("queue should hold R2_TUI_EVENT_QUEUE events", count == R2_TUI_EVENT_QUEUE);
    return 0;
}

static const char *test_tui_size_query(void)
{
    tui t;
    tui_event ev;
    uint8_t out[64];
    size_t n;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    /* without a query, ESC[r;cR is Shift/Ctrl+F3 */
    TUI_FEED(&t, "\x1b[1;5R", 0);
    r2_assert("CPR without a query is F3", tui_poll(&t, &ev) && ev.type == TUI_EV_KEY && ev.key == TUI_KEY_F1 + 2);
    n = tui_query_size(&t, out, sizeof(out));
    r2_assert("query bytes", n > 0 && memcmp(out, "\x1b" "7\x1b[999;999H\x1b[6n\x1b" "8", n) == 0);
    TUI_FEED(&t, "\x1b[40;120R", 0);
    r2_assert("CPR answers the query", tui_poll(&t, &ev) && ev.type == TUI_EV_RESIZE && ev.x == 120 && ev.y == 40);
    return 0;
}

static const char *test_tui_input_fuzz(void)
{
    /* random bytes biased towards escape sequences: never crashes, never
       produces an out of range key, and never leaves bytes stuck */
    static const uint8_t pool[] = {0x1b, '[', 'O', ';', '1', '5', '~', 'A', 'P', 0xC3, 0xA9, 0xE4, 0xB8, 'a', 0x7f, 0x00};
    tui t;
    tui_event ev;
    uint32_t now = 0;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (int i = 0; i < 200000; i++)
    {
        uint8_t b = (tui_rand() & 1) ? pool[tui_rand() % sizeof(pool)] : (uint8_t)tui_rand();
        now += tui_rand() % 30;
        tui_feed(&t, &b, 1, now);
        while (tui_poll(&t, &ev))
        {
            r2_assert("fuzz: bad type", ev.type == TUI_EV_KEY || ev.type == TUI_EV_RESIZE);
            r2_assert("fuzz: bad key", ev.key <= TUI_KEY_F12);
            r2_assert("fuzz: bad rune", ev.ch <= R2_RUNE_MAX);
        }
    }
    tui_step(&t, now + 1000);
    while (tui_poll(&t, &ev))
        ;
    r2_assert("fuzz: input stuck", !tui_input_pending(&t));
    return 0;
}

static const char *test_tui_key_names(void)
{
    char buf[32];
    tui_event ev = {TUI_EV_KEY, TUI_MOD_CTRL | TUI_MOD_ALT, TUI_KEY_F1 + 4, 0, 0, 0};
    tui_key_name(&ev, buf, sizeof(buf));
    r2_assert("Ctrl+Alt+F5", strcmp(buf, "Ctrl+Alt+F5") == 0);
    ev = (tui_event){TUI_EV_KEY, TUI_MOD_SHIFT, TUI_KEY_TAB, 0, 0, 0};
    tui_key_name(&ev, buf, sizeof(buf));
    r2_assert("Shift+Tab", strcmp(buf, "Shift+Tab") == 0);
    ev = (tui_event){TUI_EV_KEY, 0, TUI_KEY_RUNE, 0x4E2D, 0, 0};
    tui_key_name(&ev, buf, sizeof(buf));
    r2_assert("CJK name", strcmp(buf, "\xE4\xB8\xAD") == 0);
    tui_key_name(&ev, buf, 2);
    r2_assert("name truncated safely", strlen(buf) == 1);
    return 0;
}

///////////////////////////////////////////////////////////////
// Window manager. Everything goes in as key bytes and is checked on the
// composed screen (the back buffer), like a user would see it.

static uint32_t wm_now = 1000;

/* type keys and let the window manager handle them */
static void wm_keys(tui *t, const char *bytes)
{
    tui_feed(t, (const uint8_t *)bytes, strlen(bytes), wm_now);
    wm_now += 100; /* lets a lone ESC time out */
    tui_update(t, wm_now);
}

static const char *wm_row(const tui *t, int y)
{
    static char buf[VT_COLS * 4 + 1];
    size_t n = 0;
    for (int x = 0; x < t->cols; x++)
    {
        rune r = TUI_CELL_RUNE(tui_get_cell(t, x, y));
        if (r != TUI_RUNE_CONT)
            n += utf8_encode(r ? r : ' ', (uint8_t *)buf + n);
    }
    buf[n] = '\0';
    return buf;
}

static int wm_has(const tui *t, const char *text)
{
    for (int y = 0; y < t->rows; y++)
        if (strstr(wm_row(t, y), text))
            return 1;
    return 0;
}

static rune wm_at(const tui *t, int x, int y)
{
    return TUI_CELL_RUNE(tui_get_cell(t, x, y));
}

typedef struct wm_app
{
    const char *text;
    const char *menu;
    const char *title;
    int saved, checks, bad, keys, closing_seen, allow_close, ask, open_child, spam;
    unsigned answer;
    rune last_ctrl;
} wm_app;

static wm_app wm_child_app;

static void wm_app_ui(tui *t, tui_window *w, void *user)
{
    wm_app *a = (wm_app *)user;
    const tui_event *ev;
    if (a->menu)
    {
        tui_menu_begin(t, a->menu);
        if (tui_menu_item(t, "~S~ave", "Ctrl+S"))
            a->saved++;
        if (tui_menu_item_flags(t, "~Q~uit", "Ctrl+Q", TUI_ITEM_DISABLED))
            a->bad++;
        tui_menu_separator(t);
        if (tui_menu_item_flags(t, "C~h~eck", 0, TUI_ITEM_CHECKED))
            a->checks++;
        if (tui_menu_item(t, "~N~ew window", "F2"))
            a->open_child = 1;
        tui_menu_end(t);
    }
    tui_text(t, 0, 0, s8_from_cstr(a->text ? a->text : ""), TUI_DEFAULT);
    for (int i = 0; i < a->spam; i++)
        tui_text(t, 0, 1, S8("0123456789012345678901234567890123456789012345678901234567890123456789"), TUI_DEFAULT);
    ev = tui_key(t);
    if (ev && ev->key == TUI_KEY_RUNE && ev->ch == 'k' && !ev->mods)
    {
        a->keys++;
        tui_use_key(t);
    }
    if (ev && ev->key == TUI_KEY_RUNE && ev->mods == TUI_MOD_CTRL)
        a->last_ctrl = ev->ch;
    if (tui_closing(t))
    {
        a->closing_seen++;
        if (a->allow_close)
            tui_window_close(t, w);
    }
    if (a->ask)
    {
        unsigned r = tui_message(t, a->title ? a->title : "Quit?", "Really quit the demo? Unsaved work is lost.",
                                 TUI_MB_YES | TUI_MB_NO | TUI_MB_CANCEL);
        if (r)
        {
            a->answer = r;
            a->ask = 0;
        }
    }
    if (a->open_child)
    {
        a->open_child = 0;
        tui_window_open(t, "Child", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &wm_child_app);
    }
}

static const char *test_wm_open_and_draw(void)
{
    tui t;
    wm_app a = {.text = "hello", .menu = "~F~ile"};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *w = tui_window_open(&t, "Alpha", (tui_rect){5, 3, 30, 8}, 0, wm_app_ui, &a);
    r2_assert("open failed", w != 0);
    tui_update(&t, wm_now);
    r2_assert("text not drawn inside the frame", wm_at(&t, 6, 4) == 'h' && wm_at(&t, 10, 4) == 'o');
    r2_assert("focused frame should be double", wm_at(&t, 5, 3) == 0x2554 && wm_at(&t, 34, 10) == 0x255D);
    r2_assert("title not centred", wm_at(&t, 17, 3) == 'A');
    r2_assert("window number", wm_at(&t, 31, 3) == '1');
    r2_assert("close marker", wm_at(&t, 8, 3) == 0x25A0);
    r2_assert("shadow on the right",
              TUI_CELL_STYLE(tui_get_cell(&t, 35, 5)) == TUI_STYLE(TUI_DARK_GRAY, TUI_BLACK, 0));
    r2_assert("shadow underneath", TUI_CELL_STYLE(tui_get_cell(&t, 20, 11)) == TUI_STYLE(TUI_DARK_GRAY, TUI_BLACK, 0));
    r2_assert("no shadow left of the window", TUI_CELL_STYLE(tui_get_cell(&t, 6, 11)) == t.theme->desktop);
    r2_assert("desktop pattern", wm_at(&t, 0, 1) == 0x2591);
    r2_assert("menu bar shows the window's menu", strstr(wm_row(&t, 0), "\xE2\x89\xA1  File") != 0);
    r2_assert("status line", strstr(wm_row(&t, VT_ROWS - 1), "F10 Menu") != 0);
    r2_assert("controller ran once", w->runs == 1);
    /* through the renderer too */
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

static const char *test_wm_overlap_and_focus(void)
{
    tui t;
    wm_app a = {.text = "AAAAAAAAAA", .menu = "~F~ile"}, b = {.text = "BBBBBBBBBB", .menu = "~V~iew"};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){8, 3, 20, 6}, 0, wm_app_ui, &b);
    tui_update(&t, wm_now);
    r2_assert("newest window has focus", tui_focused_window(&t) == wb);
    r2_assert("B on top at the overlap", wm_at(&t, 9, 4) == 'B');
    r2_assert("A unfocused: single frame", wm_at(&t, 2, 2) == 0x250C);
    r2_assert("bar shows B's menus", strstr(wm_row(&t, 0), "View") && !strstr(wm_row(&t, 0), "File"));
    wm_keys(&t, "\x1b[17~"); /* F6 */
    r2_assert("F6 focuses A", tui_focused_window(&t) == wa);
    r2_assert("A raised over B", wm_at(&t, 9, 3) == 'A');
    r2_assert("bar follows focus", strstr(wm_row(&t, 0), "File") && !strstr(wm_row(&t, 0), "View"));
    wm_keys(&t, "\x1b[17;2~"); /* Shift+F6 */
    r2_assert("Shift+F6 goes back", tui_focused_window(&t) == wb);
    wm_keys(&t, "\0331"); /* Alt+1 */
    r2_assert("Alt+1 jumps to window 1", tui_focused_window(&t) == wa);
    wm_keys(&t, "k");
    r2_assert("keys go to the focused window", a.keys == 1 && b.keys == 0);
    return 0;
}

static const char *test_wm_menus(void)
{
    tui t;
    wm_app a = {.text = "x", .menu = "~F~ile"};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "A", (tui_rect){10, 6, 30, 8}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b[21~"); /* F10 */
    r2_assert("F10 opens the first menu", t.menu_open && t.menu_idx == 1);
    r2_assert("dropdown shows items and shortcuts", wm_has(&t, "Save") && wm_has(&t, "Ctrl+S"));
    r2_assert("separator drawn", wm_has(&t, "\xE2\x94\x9C\xE2\x94\x80"));
    r2_assert("checked mark", wm_has(&t, "\xE2\x88\x9A"));
    r2_assert("no cursor while the menu is open", !t.cur_visible);
    wm_keys(&t, "\r");
    r2_assert("Enter chose the first item", a.saved == 1 && !t.menu_open);
    wm_keys(&t, "\033f"); /* Alt+F */
    r2_assert("Alt+F opens File", t.menu_open && t.menu_idx == 1);
    wm_keys(&t, "\x1b[B"); /* Down: skips the disabled Quit and the separator */
    wm_keys(&t, "\r");
    r2_assert("Down skipped disabled items", a.checks == 1 && a.bad == 0);
    wm_keys(&t, "\033f");
    wm_keys(&t, "q"); /* hot letter of a disabled item does nothing */
    r2_assert("disabled item by hot letter", a.bad == 0 && t.menu_open);
    wm_keys(&t, "s");
    r2_assert("hot letter chooses", a.saved == 2 && !t.menu_open);
    wm_keys(&t, "\x13"); /* Ctrl+S */
    r2_assert("shortcut chooses", a.saved == 3);
    wm_keys(&t, "\x11"); /* Ctrl+Q on a disabled item */
    r2_assert("disabled shortcut ignored", a.bad == 0);
    r2_assert("disabled shortcut's key goes to the window", a.last_ctrl == 'q');
    wm_keys(&t, "\x1b[21~");
    wm_keys(&t, "\x1b");
    r2_assert("Esc closes the menu", !t.menu_open);
    wm_keys(&t, "\x1b[21~");
    wm_keys(&t, "\x1b[D"); /* Left: to the system menu */
    r2_assert("Left goes to the system menu", t.menu_open && t.menu_idx == 0 && wm_has(&t, "Move/Size"));
    wm_keys(&t, "\x1b");
    wm_keys(&t, "\x1b[12~"); /* F2: New window */
    r2_assert("window opened from a menu", tui_focused_window(&t) && strcmp(tui_focused_window(&t)->title, "Child") == 0);
    r2_assert("child drawn", wm_has(&t, " Child "));
    return 0;
}

static const char *test_wm_system_menu_and_docking(void)
{
    tui t;
    wm_app a = {.text = "left", .menu = 0}, b = {.text = "right", .menu = 0}, c = {.text = "bottom", .menu = 0}, d = {.text = "float", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &a);
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &b);
    tui_window *wc = tui_window_open(&t, "C", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &c);
    tui_window *wd = tui_window_open(&t, "D", (tui_rect){0, 0, 100, 100}, 0, wm_app_ui, &d);
    tui_window_dock(&t, wa, TUI_DOCK_LEFT, 15);
    tui_window_dock(&t, wb, TUI_DOCK_RIGHT, 12);
    tui_window_dock(&t, wc, TUI_DOCK_BOTTOM, 5);
    tui_update(&t, wm_now);
    tui_rect work = tui_work_area(&t);
    r2_assert("left dock", wa->frame.x == 0 && wa->frame.y == 1 && wa->frame.w == 15 && wa->frame.h == VT_ROWS - 2);
    r2_assert("right dock", wb->frame.x == VT_COLS - 12 && wb->frame.w == 12 && wb->frame.h == VT_ROWS - 2);
    r2_assert("bottom dock between them",
              wc->frame.x == 15 && wc->frame.w == VT_COLS - 27 && wc->frame.h == 5 && wc->frame.y == VT_ROWS - 1 - 5);
    r2_assert("work area", work.x == 15 && work.y == 1 && work.w == VT_COLS - 27 && work.h == VT_ROWS - 2 - 5);
    r2_assert("floating window clamped into the work area",
              wd->frame.x == work.x && wd->frame.y == work.y && wd->frame.w == work.w && wd->frame.h == work.h);
    r2_assert("docked text drawn", wm_at(&t, 1, 2) == 'l');
    r2_assert("docked frame drawn", wm_at(&t, 0, 1) == 0x250C && wm_at(&t, 14, VT_ROWS - 2) == 0x2518);
    r2_assert("docked title drawn", strstr(wm_row(&t, 1), " A ") != 0);
    r2_assert("docked windows have no shadow",
              TUI_CELL_STYLE(tui_get_cell(&t, 15, 2)) != TUI_STYLE(TUI_DARK_GRAY, TUI_BLACK, 0));
    /* undock C from the system menu (Alt+Space, then F for Float) */
    tui_window_focus(&t, wc);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b ");
    r2_assert("Alt+Space opens the system menu", t.menu_open && t.menu_idx == 0);
    wm_keys(&t, "f");
    r2_assert("Float undocked", wc->dock == TUI_DOCK_NONE && tui_work_area(&t).h == VT_ROWS - 2);
    /* and dock D to the bottom the same way */
    tui_window_focus(&t, wd);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b ");
    wm_keys(&t, "b");
    r2_assert("Dock Bottom", wd->dock == TUI_DOCK_BOTTOM);
    /* resize a dock with move mode */
    wm_keys(&t, "\x1b[15;5~"); /* Ctrl+F5 */
    int h = wd->frame.h;
    wm_keys(&t, "\x1b[A");
    wm_keys(&t, "\r");
    r2_assert("Up grows a bottom dock", wd->frame.h == h + 1);
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

static const char *test_wm_move_zoom_close(void)
{
    tui t;
    wm_app a = {.text = "a", .menu = 0}, b = {.text = "b", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){10, 4, 20, 6}, 0, wm_app_ui, &b);
    tui_update(&t, wm_now);
    uint32_t runs = wb->runs;
    wm_keys(&t, "\x1b[15;5~"); /* Ctrl+F5 */
    r2_assert("move mode", t.moving && strstr(wm_row(&t, VT_ROWS - 1), "Move") != 0);
    r2_assert("move mode frame colour", TUI_STYLE_FG(TUI_CELL_STYLE(tui_get_cell(&t, 10, 4))) == TUI_BRIGHT_GREEN);
    wm_keys(&t, "\x1b[C\x1b[C\x1b[C\x1b[B");
    r2_assert("arrows move", wb->rect.x == 13 && wb->rect.y == 5);
    r2_assert("moving does not re-run the controller", wb->runs == runs);
    r2_assert("content moved with the window", wm_at(&t, 14, 6) == 'b');
    wm_keys(&t, "\x1b[1;2C\x1b[1;2C");
    r2_assert("Shift+Right widens", wb->rect.w == 22);
    r2_assert("resizing re-runs the controller", wb->runs > runs);
    wm_keys(&t, "\r");
    r2_assert("Enter ends move mode", !t.moving);
    wm_keys(&t, "\x1b[15;5~");
    for (int i = 0; i < 100; i++)
        wm_keys(&t, "\x1b[D");
    r2_assert("cannot move past the work area", wb->rect.x == 0);
    wm_keys(&t, "\x1b");
    r2_assert("Esc restores", !t.moving && wb->rect.x == 13 && wb->rect.w == 22);
    wm_keys(&t, "\x1b[15~"); /* F5 */
    r2_assert("F5 zooms", wb->frame.x == 0 && wb->frame.w == VT_COLS && wb->frame.h == VT_ROWS - 2);
    wm_keys(&t, "\x1b[15~");
    r2_assert("F5 again restores", wb->frame.x == 13 && wb->frame.w == 22);
    wm_keys(&t, "\x1b[1;3R"); /* Alt+F3 */
    r2_assert("Alt+F3 closes", !wb->used && tui_focused_window(&t) == wa);
    r2_assert("closed window gone from the screen", !wm_has(&t, " B "));
    wm_keys(&t, "\x1b[1;3R");
    r2_assert("last window closed", !wa->used && tui_focused_window(&t) == 0);
    r2_assert("empty desktop", wm_at(&t, 5, 5) == 0x2591);
    return 0;
}

static const char *test_wm_prefix(void)
{
    tui t;
    wm_app a = {.text = "aaa", .menu = "~F~ile"}, b = {.text = "bbb", .menu = "~E~dit"}, c = {.text = "ccc", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){8, 4, 20, 6}, 0, wm_app_ui, &b);
    tui_window *wc = tui_window_open(&t, "C", (tui_rect){14, 6, 20, 6}, 0, wm_app_ui, &c);
    tui_update(&t, wm_now);
    r2_assert("status names the prefix", strstr(wm_row(&t, VT_ROWS - 1), "Ctrl+\\ Commands") != 0);

    wm_keys(&t, "\x1c");
    r2_assert("prefix waits", t.chord && strstr(wm_row(&t, VT_ROWS - 1), "letter Menu") != 0);
    wm_keys(&t, "1");
    r2_assert("prefix 1: window 1", tui_focused_window(&t) == wa && !t.chord);
    wm_keys(&t, "\x1c" "f");
    r2_assert("prefix f: File menu", t.menu_open && t.menu_idx == 1);
    wm_keys(&t, "\x1b");
    wm_keys(&t, "\x1c" "F");
    r2_assert("prefix Shift+F: File menu too", t.menu_open && t.menu_idx == 1);
    wm_keys(&t, "\x1b");
    wm_keys(&t, "\x1c ");
    r2_assert("prefix Space: system menu", t.menu_open && t.menu_idx == 0);
    r2_assert("system menu shows the chords", wm_has(&t, "Ctrl+\\ Enter") && wm_has(&t, "Ctrl+\\ Bksp"));
    wm_keys(&t, "\x1b");
    wm_keys(&t, "\x1c\x1b[C");
    r2_assert("prefix Right: next window", tui_focused_window(&t) == wb);
    wm_keys(&t, "\x1c\t");
    r2_assert("prefix Tab: next window", tui_focused_window(&t) == wc);
    wm_keys(&t, "\x1c\x1b[Z");
    r2_assert("prefix Shift+Tab: previous", tui_focused_window(&t) == wb);
    wm_keys(&t, "\x1c\x1b[D");
    r2_assert("prefix Left: previous", tui_focused_window(&t) == wa);

    /* up maximizes, down restores, down again minimizes */
    wm_keys(&t, "\x1c\x1b[A");
    r2_assert("prefix Up: maximized", wa->zoomed && wa->frame.w == VT_COLS);
    wm_keys(&t, "\x1c\x1b[A");
    r2_assert("prefix Up again: still maximized", wa->zoomed);
    wm_keys(&t, "\x1c\x1b[B");
    r2_assert("prefix Down: restored", !wa->zoomed && wa->frame.w == 20);
    wm_keys(&t, "\x1c\x1b[B");
    r2_assert("prefix Down again: minimized", wa->hidden && tui_focused_window(&t) != wa);
    r2_assert("minimized window not drawn", !wm_has(&t, "aaa"));
    wm_keys(&t, "\x1c\x1b[C");
    wm_keys(&t, "\x1c\x1b[C");
    r2_assert("next skips minimized windows", tui_focused_window(&t) != wa);
    wm_keys(&t, "\x1c ");
    r2_assert("minimized window still listed", wm_has(&t, "1 A"));
    wm_keys(&t, "\x1b");
    wm_keys(&t, "\x1c" "1");
    r2_assert("prefix 1 restores the minimized window", !wa->hidden && tui_focused_window(&t) == wa && wm_has(&t, "aaa"));

    /* Enter: move mode; Backspace: close */
    wm_keys(&t, "\x1c\r");
    r2_assert("prefix Enter: move mode", t.moving);
    wm_keys(&t, "\x1b[C\r");
    r2_assert("moved", wa->rect.x == 3 && !t.moving);
    wm_keys(&t, "\x1c\x7f");
    r2_assert("prefix Backspace: closed", !wa->used);

    /* prefix twice: the window gets Ctrl+\ */
    c.last_ctrl = 0;
    tui_window_focus(&t, wc);
    wm_keys(&t, "\x1c\x1c");
    r2_assert("prefix twice reaches the window", c.last_ctrl == '\\' && !t.chord);
    /* Esc, an unknown key, or waiting cancels */
    wm_keys(&t, "\x1c");
    wm_keys(&t, "\x1b");
    r2_assert("Esc cancels", !t.chord && !t.menu_open);
    wm_keys(&t, "\x1c" "k");
    r2_assert("unknown key cancels and is not passed on", !t.chord && c.keys == 0);
    /* with Ctrl still held the terminal sends Ctrl+H as Backspace, Ctrl+M as
       Enter... so only plain (or Shifted) keys count after the prefix */
    tui_window_focus(&t, wb);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1c\x05"); /* Ctrl+E (B's Edit menu hot letter) */
    r2_assert("Ctrl+letter after the prefix does nothing", !t.menu_open && !t.chord);
    wm_keys(&t, "\x1c");
    tui_update(&t, wm_now + R2_TUI_PREFIX_MS);
    r2_assert("prefix times out", !t.chord && strstr(wm_row(&t, VT_ROWS - 1), "Commands") != 0);
    return 0;
}

static const char *test_wm_prefix_config(void)
{
    tui t;
    wm_app a = {.text = "aaa", .menu = "~F~ile"};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    r2_assert("bad prefix rejected", !tui_set_prefix(&t, "Hyper+Q"));
    /* Ctrl+^ (0x1E); Ctrl+] would collide with the palette key */
    r2_assert("Ctrl+^ accepted", tui_set_prefix(&t, "Ctrl+^"));
    wm_keys(&t, "\x1c");
    r2_assert("old prefix now a plain key", !t.chord && a.last_ctrl == '\\');
    wm_keys(&t, "\x1e" "f");
    r2_assert("new prefix works", t.menu_open && t.menu_idx == 1);
    wm_keys(&t, "\x1b");
    r2_assert("prefix off", tui_set_prefix(&t, 0));
    wm_keys(&t, "\x1e");
    r2_assert("no prefix: key goes to the window", !t.chord && a.last_ctrl == '^');
    r2_assert("status without a prefix", strstr(wm_row(&t, VT_ROWS - 1), "Ctrl+F5 Move") != 0);
    wm_keys(&t, "\x1b ");
    r2_assert("system menu falls back to F-keys", wm_has(&t, "Alt+F3"));
    return 0;
}

static const char *test_wm_prefix_modal_and_dock(void)
{
    tui t;
    wm_app a = {.text = "aaa", .menu = "~F~ile"}, d = {.text = "dock", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wd = tui_window_open(&t, "D", (tui_rect){0, 0, 0, 0}, 0, wm_app_ui, &d);
    tui_window_dock(&t, wd, TUI_DOCK_LEFT, 15);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){20, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    /* minimizing a docked window gives its space back */
    tui_window_focus(&t, wd);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1c\x1b[B");
    r2_assert("docked window minimized", wd->hidden && tui_work_area(&t).x == 0);
    wm_keys(&t, "\x1c" "1");
    r2_assert("docked window back", !wd->hidden && tui_work_area(&t).x == 15);
    /* under a modal box the prefix does nothing to the windows */
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1c" "1");
    r2_assert("prefix blocked by a modal", !t.chord && wm_has(&t, "Quit?"));
    wm_keys(&t, "\x1b");
    r2_assert("box answered", a.answer == TUI_MB_CANCEL);
    return 0;
}

static int fuzzy(const char *query, const char *text)
{
    rune q[32], c[128];
    int qn = r2__tui_runes(query, q, 32), cn = r2__tui_runes(text, c, 128);
    return r2__tui_fuzzy(q, qn, c, cn, 0);
}

static const char *test_palette_fuzzy(void)
{
    uint8_t marks[128] = {0};
    rune q[32], c[128];
    r2_assert("letters in order match", fuzzy("fnn", "File \xE2\x80\xBA New note") >= 0);
    r2_assert("case does not matter", fuzzy("FILE", "file") >= 0 && fuzzy("file", "FILE") >= 0);
    r2_assert("out of order does not match", fuzzy("wf", "File New") < 0);
    r2_assert("missing letter does not match", fuzzy("zz", "Zoom") < 0);
    r2_assert("empty query matches everything", fuzzy("", "anything") >= 0);
    r2_assert("whole word beats scattered", fuzzy("save", "File \xE2\x80\xBA Save") > fuzzy("save", "Show all values everywhere"));
    r2_assert("start beats middle", fuzzy("zo", "Zoom") > fuzzy("zo", "Window \xE2\x80\xBA Fuzzy zone"));
    r2_assert("word start beats mid-word", fuzzy("n", "File \xE2\x80\xBA New") > fuzzy("n", "File \xE2\x80\xBA Open"));
    r2_assert("consecutive beats gaps", fuzzy("new", "New note") > fuzzy("new", "Next window"));
    r2_assert("shorter wins a tie", fuzzy("close", "Close") > fuzzy("close", "Close all the windows"));
    /* the best placement, not the first one found: "ab" at the end, where
       it is a whole word, beats the a at the start with a far-away b */
    int qn = r2__tui_runes("ab", q, 32), cn = r2__tui_runes("a xb ab", c, 128);
    r2_assert("matches", r2__tui_fuzzy(q, qn, c, cn, marks) >= 0);
    r2_assert("marks the better placement", marks[5] && marks[6] && !marks[0] && !marks[3]);
    return 0;
}

static const char *test_palette(void)
{
    tui t;
    wm_app a = {.text = "aaa", .menu = "~F~ile"}, b = {.text = "bbb", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wb = tui_window_open(&t, "Bravo", (tui_rect){30, 8, 20, 6}, 0, wm_app_ui, &b);
    tui_window *wa = tui_window_open(&t, "Alpha", (tui_rect){2, 2, 26, 8}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    r2_assert("status names the palette key", strstr(wm_row(&t, VT_ROWS - 1), "Ctrl+] Search") != 0);

    TUI_FEED(&t, "\x1d", wm_now); /* Ctrl+] */
    tui_update(&t, wm_now);
    r2_assert("Ctrl+] opens the palette", t.pal_open && strstr(wm_row(&t, 1), " Search ") != 0);
    r2_assert("lists menu items with their shortcuts", wm_has(&t, "File \xE2\x80\xBA Save") && wm_has(&t, "Ctrl+S"));
    r2_assert("lists window commands", wm_has(&t, "Window \xE2\x80\xBA Zoom"));
    r2_assert("lists other windows", wm_has(&t, "Go to window \xE2\x80\xBA Bravo"));
    r2_assert("disabled items left out", !wm_has(&t, "File \xE2\x80\xBA Quit"));
    r2_assert("cursor in the query", t.cur_visible && t.cur_y == 2);

    /* "window": the window commands start with the word, so they outrank
       "File > New window" even though menu items are added first */
    wm_keys(&t, "window");
    r2_assert("better matches sorted first", t.pal_n > 2 && t.pal[0].kind == R2__TUI_PAL_SYS);
    for (int i = 1; i < t.pal_n; i++)
        r2_assert("results in score order", t.pal[i - 1].score >= t.pal[i].score);
    wm_keys(&t, "\x15");
    wm_keys(&t, "sav");
    r2_assert("query shown", wm_has(&t, "> sav"));
    r2_assert("best match first", t.pal_n > 0 && t.pal[0].kind == R2__TUI_PAL_WIN);
    wm_keys(&t, "\r");
    r2_assert("Enter runs the item", a.saved == 1 && !t.pal_open && strstr(wm_row(&t, 1), " Search ") == 0);

    /* a window command */
    wm_keys(&t, "\x1c/"); /* the prefix then / opens it too */
    r2_assert("prefix / opens the palette", t.pal_open);
    wm_keys(&t, "zoom\r");
    r2_assert("Zoom from the palette", wa->zoomed);

    /* go to a window, editing the query on the way */
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    wm_keys(&t, "brx");
    r2_assert("no matches shown", t.pal_n == 0 && wm_has(&t, "No matches"));
    wm_keys(&t, "\x7f"); /* Backspace */
    r2_assert("Backspace edits the query", t.pal_n == 1 && wm_has(&t, "> br"));
    wm_keys(&t, "\r");
    r2_assert("went to Bravo", tui_focused_window(&t) == wb);

    /* moving the selection: Down / Ctrl+N / Tab and Up / Ctrl+P */
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    int n = t.pal_n;
    r2_assert("several results", n > 3);
    wm_keys(&t, "\x1b[B");
    wm_keys(&t, "\x0e");
    wm_keys(&t, "\t");
    r2_assert("down three", t.pal_sel == 3);
    wm_keys(&t, "\x10");
    wm_keys(&t, "\x1b[A");
    r2_assert("up two", t.pal_sel == 1);
    for (int i = 0; i < 50; i++)
        wm_keys(&t, "\x1b[B");
    r2_assert("stops at the end", t.pal_sel == n - 1);
    r2_assert("scrolled to show it", t.pal_top + r2__tui_pal_rows(&t) > t.pal_sel);
    wm_keys(&t, "\x15"); /* Ctrl+U */
    r2_assert("Ctrl+U clears", t.pal_query[0] == 0);
    wm_keys(&t, "\x1b");
    r2_assert("Esc closes", !t.pal_open);
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x07");
    r2_assert("Ctrl+G closes", !t.pal_open);
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    r2_assert("Ctrl+] again closes", !t.pal_open);
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

/* a menu item that opens the palette from inside a controller */
static void wm_pal_opener_ui(tui *t, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    tui_menu_begin(t, "~T~ools");
    if (tui_menu_item(t, "~S~earch commands", "F7"))
        tui_palette(t);
    tui_menu_item(t, "~L~ater item", 0);
    tui_menu_end(t);
}

static const char *test_palette_from_controller(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "T", (tui_rect){2, 2, 26, 8}, 0, wm_pal_opener_ui, 0);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b[18~"); /* F7 */
    r2_assert("opened from a controller", t.pal_open);
    r2_assert("lists items declared after the call", wm_has(&t, "Tools \xE2\x80\xBA Later item"));
    return 0;
}

static const char *test_palette_desktop_and_modal(void)
{
    /* desktop menus are searchable too (without duplicating the window's) */
    static wm_app desk = {.text = "", .menu = "~F~ile"};
    tui t;
    wm_app a = {.text = "aaa", .menu = "~F~ile"}, b = {.text = "bbb", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_set_desktop(&t, wm_app_ui, &desk);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){2, 2, 26, 8}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    tui_palette(&t);
    tui_update(&t, wm_now);
    int saves = 0;
    for (int i = 0; i < t.pal_n; i++)
    {
        char text[128], kbuf[32];
        const char *key;
        r2__tui_pal_text(&t, &t.pal[i], text, sizeof(text), &key, kbuf, sizeof(kbuf));
        if (strcmp(text, "File \xE2\x80\xBA Save") == 0)
            saves++;
    }
    r2_assert("same command listed once", saves == 1);
    wm_keys(&t, "\x1b");
    /* a window without menus: the desktop's items run on the desktop */
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){30, 8, 20, 6}, 0, wm_app_ui, &b);
    tui_update(&t, wm_now);
    tui_palette(&t);
    wm_keys(&t, "save\r");
    r2_assert("desktop item ran on the desktop", desk.saved == 1 && a.saved == 0 && tui_focused_window(&t) == wb);
    /* not under a modal box */
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    r2_assert("no palette under a modal", !t.pal_open);
    wm_keys(&t, "\x1b");
    /* configurable */
    r2_assert("palette key off", tui_set_palette_key(&t, 0));
    TUI_FEED(&t, "\x1d", wm_now);
    tui_update(&t, wm_now);
    r2_assert("Ctrl+] no longer opens it", !t.pal_open);
    wm_keys(&t, "\x1c/");
    r2_assert("prefix / still does", t.pal_open);
    wm_keys(&t, "\x1b");
    r2_assert("other key", tui_set_palette_key(&t, "F2"));
    wm_keys(&t, "\x1b[12~");
    r2_assert("F2 opens it", t.pal_open);
    return 0;
}

static const char *test_wm_ask_close(void)
{
    tui t;
    wm_app a = {.text = "a", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *w = tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, TUI_WIN_ASK_CLOSE, wm_app_ui, &a);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b[1;3R");
    r2_assert("ASK_CLOSE asks the controller", a.closing_seen == 1 && w->used);
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    r2_assert("closing is reported once", a.closing_seen == 1);
    a.allow_close = 1;
    wm_keys(&t, "\x1b[1;3R");
    r2_assert("controller closed it", a.closing_seen == 2 && !w->used);
    return 0;
}

static const char *test_wm_message_box(void)
{
    tui t;
    wm_app a = {.text = "main", .menu = "~F~ile"}, b = {.text = "other", .menu = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wb = tui_window_open(&t, "B", (tui_rect){1, 2, 20, 6}, 0, wm_app_ui, &b);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){20, 4, 30, 8}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    r2_assert("box shown", wm_has(&t, " Quit? ") && wm_has(&t, "[ Yes ]") && wm_has(&t, "[ Cancel ]"));
    r2_assert("text wrapped inside", wm_has(&t, "Really quit"));
    r2_assert("still waiting", a.answer == 0);
    wm_keys(&t, "\x1b[17~"); /* F6: blocked */
    r2_assert("modal keeps focus", tui_focused_window(&t) != wb && tui_focused_window(&t) != wa);
    wm_keys(&t, "\x1b[21~"); /* F10: blocked */
    r2_assert("no menus under a modal", !t.menu_open);
    wm_keys(&t, "\x13"); /* Ctrl+S: blocked */
    r2_assert("no shortcuts under a modal", a.saved == 0);
    wm_keys(&t, "n");
    r2_assert("hot letter answers", a.answer == TUI_MB_NO && !wm_has(&t, "Quit?"));
    r2_assert("focus back on the owner", tui_focused_window(&t) == wa);
    /* Esc means Cancel; Right then Enter picks the second button */
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b");
    r2_assert("Esc cancels", a.answer == TUI_MB_CANCEL);
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b[C");
    wm_keys(&t, "\r");
    r2_assert("Right, Enter: second button", a.answer == TUI_MB_NO);
    a.ask = 1;
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    wm_keys(&t, "\r");
    r2_assert("Enter: first button", a.answer == TUI_MB_YES);
    /* another window asking while the box is up must not take the answer */
    a.answer = 0;
    a.ask = 1;
    b.ask = 1;
    b.title = "Other question";
    tui_window_invalidate(&t, wa);
    tui_update(&t, wm_now);
    tui_window_invalidate(&t, wb); /* b (a lower slot) runs first after the answer */
    wm_keys(&t, "y");
    r2_assert("answer went to the asker", a.answer == TUI_MB_YES && b.answer == 0);
    r2_assert("the other window gets its own box next", wm_has(&t, "Other question"));
    wm_keys(&t, "\x1b");
    r2_assert("and its own answer", b.answer == TUI_MB_CANCEL);
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

/* "Open" opens a window (moving focus mid-run); items after it must not fire */
static int wm_opens, wm_after;
static void wm_opener_ui(tui *t, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    tui_menu_begin(t, "~F~ile");
    if (tui_menu_item(t, "~O~pen", "Ctrl+O"))
    {
        wm_opens++;
        tui_window_open(t, "Opened", (tui_rect){-1, -1, 0, 0}, 0, wm_opener_ui, 0);
    }
    if (tui_menu_item(t, "~A~fter", 0))
        wm_after++;
    tui_menu_end(t);
}

static const char *test_wm_open_from_menu_mid_run(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    wm_opens = wm_after = 0;
    tui_window_open(&t, "First", (tui_rect){-1, -1, 0, 0}, 0, wm_opener_ui, 0);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x0f"); /* Ctrl+O */
    r2_assert("Open fired once", wm_opens == 1);
    r2_assert("the item after Open must not fire", wm_after == 0);
    wm_keys(&t, "\x1b[21~");
    wm_keys(&t, "\r");
    r2_assert("from the menu: Open fired once more", wm_opens == 2 && wm_after == 0);
    r2_assert("new window has its own menus", strstr(wm_row(&t, 0), "File") != 0);
    return 0;
}

/* more menu text than the bar holds: what is stored still maps to the right items */
static int wm_big_hits[64];
static void wm_big_menu_ui(tui *t, tui_window *w, void *user)
{
    char label[64];
    (void)w;
    (void)user;
    tui_menu_item(t, "outside any menu", 0); /* ignored */
    tui_menu_begin(t, "~B~ig");
    for (int i = 0; i < 64; i++)
    {
        snprintf(label, sizeof(label), "Item %02d with a fairly long label to fill the text", i);
        if (tui_menu_item(t, label, i == 63 ? "Ctrl+B" : 0))
            wm_big_hits[i]++;
    }
    tui_menu_end(t);
}

static const char *test_wm_menu_overflow(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    memset(wm_big_hits, 0, sizeof(wm_big_hits));
    tui_window_open(&t, "Big", (tui_rect){-1, -1, 0, 0}, 0, wm_big_menu_ui, 0);
    tui_update(&t, wm_now);
    int stored = t.bar_win->nitems;
    r2_assert("bar filled up", stored > 0 && stored < 64);
    wm_keys(&t, "\x02"); /* Ctrl+B: its item was not stored, so nothing */
    for (int i = 0; i < 64; i++)
        r2_assert("unstored shortcut fired something", wm_big_hits[i] == 0);
    wm_keys(&t, "\x1b[21~");
    wm_keys(&t, "\x1b[F"); /* End: last stored item */
    wm_keys(&t, "\r");
    r2_assert("last stored item maps to its own code", wm_big_hits[stored - 1] == 1);
    for (int i = 0; i < 64; i++)
        r2_assert("only that item fired", i == stored - 1 || wm_big_hits[i] == 0);
    return 0;
}

static const char *test_wm_no_controller_window(void)
{
    /* a window without a controller (nothing to draw, no menus) must not
       inherit the previous window's menus */
    tui t;
    wm_app a = {.text = "a", .menu = "~F~ile"};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "A", (tui_rect){2, 2, 20, 6}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    r2_assert("A's menu shown", strstr(wm_row(&t, 0), "File") != 0);
    tui_window_open(&t, "Empty", (tui_rect){10, 4, 20, 6}, 0, 0, 0);
    tui_update(&t, wm_now);
    r2_assert("no stale menus", strstr(wm_row(&t, 0), "File") == 0);
    wm_keys(&t, "\x13"); /* Ctrl+S must not reach A's Save either */
    r2_assert("no stale shortcuts", a.saved == 0);
    return 0;
}

static const char *test_wm_limits(void)
{
    tui t;
    wm_app a = {.text = "spam", .menu = 0};
    int opened = 0;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    while (tui_window_open(&t, "W", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &a))
        opened++;
    r2_assert("R2_TUI_MAX_WINDOWS windows", opened == R2_TUI_MAX_WINDOWS);
    tui_update(&t, wm_now);
    /* one window drawing far more than the command pool holds */
    a.spam = 500;
    tui_window_invalidate(&t, tui_focused_window(&t));
    tui_update(&t, wm_now);
    r2_assert("overflow flagged", tui_focused_window(&t)->overflow);
    r2_assert("overflow mark on the frame", wm_has(&t, "!"));
    a.spam = 0;
    tui_window_invalidate(&t, tui_focused_window(&t));
    tui_update(&t, wm_now);
    r2_assert("overflow clears", !tui_focused_window(&t)->overflow);
    /* the other windows kept their drawing through the compaction */
    int seen = 0;
    for (int i = 2; i < R2__TUI_SLOTS; i++)
        if (t.win[i].used && t.win[i].cmd_len > 0)
            seen++;
    r2_assert("other windows kept their commands", seen == R2_TUI_MAX_WINDOWS);
    /* titles are cut on a character boundary */
    tui_window_set_title(&t, tui_focused_window(&t),
                         "\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD"
                         "\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD");
    r2_assert("title cut on a character boundary",
              s8_is_valid_utf8(s8_from_cstr(tui_focused_window(&t)->title)) && strlen(tui_focused_window(&t)->title) == 30);
    return 0;
}

static const char *test_wm_resize(void)
{
    tui t;
    uint8_t out[64];
    wm_app a = {.text = "a", .menu = 0};
    tui_test_init(&t, 40, 12);
    tui_window *w = tui_window_open(&t, "A", (tui_rect){10, 2, 25, 8}, 0, wm_app_ui, &a);
    tui_update(&t, wm_now);
    tui_query_size(&t, out, sizeof(out));
    wm_keys(&t, "\x1b[20;60R"); /* the terminal says 60x20 */
    r2_assert("resized", t.cols == 60 && t.rows == 20);
    r2_assert("status moved to the new last row", strstr(wm_row(&t, 19), "F10") != 0);
    tui_query_size(&t, out, sizeof(out));
    wm_keys(&t, "\x1b[8;20R"); /* much smaller */
    r2_assert("window squeezed into the screen", w->frame.x + w->frame.w <= 20 && w->frame.y + w->frame.h <= 7);
    vt_reset(t.cols, t.rows);
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

static const char *test_wm_shortcut_parse(void)
{
    uint16_t key;
    uint8_t mods;
    rune ch;
    r2_assert("Ctrl+S", r2__tui_parse_shortcut("Ctrl+S", &key, &mods, &ch) && key == TUI_KEY_RUNE && mods == TUI_MOD_CTRL && ch == 's');
    r2_assert("F3", r2__tui_parse_shortcut("F3", &key, &mods, &ch) && key == TUI_KEY_F1 + 2 && mods == 0);
    r2_assert("F12", r2__tui_parse_shortcut("F12", &key, &mods, &ch) && key == TUI_KEY_F12);
    r2_assert("Shift+F3", r2__tui_parse_shortcut("Shift+F3", &key, &mods, &ch) && mods == TUI_MOD_SHIFT);
    r2_assert("Alt+X", r2__tui_parse_shortcut("Alt+X", &key, &mods, &ch) && mods == TUI_MOD_ALT && ch == 'x');
    r2_assert("Ctrl+Ins", r2__tui_parse_shortcut("Ctrl+Ins", &key, &mods, &ch) && key == TUI_KEY_INSERT);
    r2_assert("Del", r2__tui_parse_shortcut("Del", &key, &mods, &ch) && key == TUI_KEY_DELETE);
    r2_assert("Ctrl++", r2__tui_parse_shortcut("Ctrl++", &key, &mods, &ch) && ch == '+' && mods == TUI_MOD_CTRL);
    r2_assert("plain letter keeps case", r2__tui_parse_shortcut("A", &key, &mods, &ch) && ch == 'A');
    r2_assert("F13 rejected", !r2__tui_parse_shortcut("F13", &key, &mods, &ch));
    r2_assert("Hyper rejected", !r2__tui_parse_shortcut("Hyper+X", &key, &mods, &ch));
    return 0;
}

static const char *test_wm_random_keys(void)
{
    /* random window-manager keys: nothing crashes, the screen always
       renders correctly, focus is always on an open window */
    static const char *const keys[] = {"\x1b[17~", "\x1b[17;2~", "\x1b[15~",  "\x1b[15;5~", "\x1b[1;3R", "\x1b[21~",
                                       "\x1b ",    "\033f",   "\x1b[A",    "\x1b[B",     "\x1b[C",    "\x1b[D",
                                       "\x1b[1;2C", "\r",        "\x1b",      "\x13",       "\x1b[12~",  "k",
                                       "b",        "l",          "r",         "n",          "y",         "\t",
                                       "\x1c",    "\x1c" "1",  "\x1c" "3", "\x1c\x1b[A", "\x1c\x1b[B", "\x1c ",
                                       "\x1c" "f", "\x1c\x7f", "\x1c\r",  "\x1c\x1c",
                                       "\x1c/",   "z",          "o",         "\x7f",      "\x15",     "\x0e"};
    static wm_app apps[4] = {{.text = "one", .menu = "~F~ile"}, {.text = "two", .menu = "~E~dit"}, {.text = "three", .menu = 0}, {.text = "four", .menu = "~F~ile"}};
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (int i = 0; i < 4; i++)
        tui_window_open(&t, "W", (tui_rect){-1, -1, 0, 0}, i == 2 ? TUI_WIN_ASK_CLOSE : 0, wm_app_ui, &apps[i]);
    for (int iter = 0; iter < 4000; iter++)
    {
        if (tui_rand() % 50 == 0)
            apps[tui_rand() % 4].ask = 1;
        if (tui_rand() % 200 == 0 && !tui_focused_window(&t))
            tui_window_open(&t, "Again", (tui_rect){-1, -1, 0, 0}, 0, wm_app_ui, &apps[tui_rand() % 4]);
        wm_keys(&t, keys[tui_rand() % (sizeof(keys) / sizeof(keys[0]))]);
        if (t.focus >= 0)
            r2_assert("focus on a closed window", t.win[t.focus].used);
        if (iter % 50 == 0)
        {
            vt_flush(&t, 1);
            r2_assert(vt_err ? vt_err : "random wm render", !vt_err && vt_matches(&t));
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// Widgets

typedef struct wform
{
    char name[16];
    int wrap, indent, sel;
    int changed, submits, toggles, radios, list_changes, list_submits, oks, cancels, extra;
    int gone; /* hide the checkbox */
    int dup;  /* two buttons with the same label */
    int dup_hits[2];
} wform;

static const char *const wf_items[] = {"apple", "banana", "cherry", "date", "elderberry", "fig", "grape", "honeydew"};

static void wform_ui(tui *t, tui_window *w, void *user)
{
    wform *f = (wform *)user;
    unsigned r;
    (void)w;
    tui_at(t, 1, 0);
    tui_label(t, "~N~ame:");
    tui_same_line(t);
    r = tui_input(t, "~N~ame##name", f->name, sizeof(f->name), 10);
    if (r & TUI_CHANGED)
        f->changed++;
    if (r & TUI_SUBMIT)
        f->submits++;
    if (!f->gone && tui_checkbox(t, "~W~rap", &f->wrap))
        f->toggles++;
    if (f->gone)
        tui_space(t, 1);
    if (tui_radio(t, "~S~paces", &f->indent, 0))
        f->radios++;
    tui_same_line(t);
    if (tui_radio(t, "~T~abs", &f->indent, 1))
        f->radios++;
    r = tui_list(t, "fruit", wf_items, 8, &f->sel, 14, 4);
    if (r & TUI_CHANGED)
        f->list_changes++;
    if (r & TUI_SUBMIT)
        f->list_submits++;
    if (tui_button(t, "~O~K"))
        f->oks++;
    tui_same_line(t);
    if (tui_button(t, "~C~ancel"))
        f->cancels++;
    if (f->dup)
    {
        if (tui_button(t, "Go"))
            f->dup_hits[0]++;
        tui_same_line(t);
        if (tui_button(t, "Go"))
            f->dup_hits[1]++;
    }
}

/* client area of the form window starts at (3, 3) */
#define WF(x, y) wm_at(&t, 3 + (x), 3 + (y))

static const char *test_widgets_layout(void)
{
    tui t;
    wform f = {.wrap = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *w = tui_window_open(&t, "Form", (tui_rect){2, 2, 40, 13}, TUI_WIN_DIALOG, wform_ui, &f);
    w->rect = (tui_rect){2, 2, 40, 13}; /* not centred: easier to read */
    tui_update(&t, wm_now);
    r2_assert("label", WF(1, 0) == 'N' && WF(5, 0) == ':');
    r2_assert("input beside the label",
              TUI_CELL_STYLE(tui_get_cell(&t, 3 + 7, 3)) == t.theme->input_focus);
    r2_assert("checkbox", WF(1, 1) == '[' && WF(2, 1) == ' ' && WF(3, 1) == ']' && WF(5, 1) == 'W');
    r2_assert("radios side by side", WF(1, 2) == '(' && WF(2, 2) == 0x2022 && WF(12, 2) == '(' && WF(16, 2) == 'T');
    r2_assert("list", WF(2, 3) == 'a' && WF(2, 6) == 'd' && WF(14, 3) == 0x2588);
    r2_assert("buttons", WF(1, 7) == '[' && WF(3, 7) == 'O' && WF(8, 7) == '[' && WF(10, 7) == 'C');
    r2_assert("first widget has focus, cursor in it", w->focus_kind == R2__TUI_W_TEXT && t.cur_visible &&
                                                          t.cur_x == 3 + 7 && t.cur_y == 3);
    r2_assert("hot letter styled", TUI_STYLE_FG(TUI_CELL_STYLE(tui_get_cell(&t, 3 + 5, 4))) == TUI_STYLE_FG(t.theme->hot));
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

static const char *test_widgets_input(void)
{
    tui t;
    wform f = {.wrap = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "Form", (tui_rect){2, 2, 40, 13}, 0, wform_ui, &f);
    tui_update(&t, wm_now);
    wm_keys(&t, "h\xC3\xA9llo");
    r2_assert("typed", strcmp(f.name, "h\xC3\xA9llo") == 0 && f.changed == 5);
    r2_assert("cursor after the text", t.cur_x == 3 + 7 + 5);
    wm_keys(&t, "\x1b[D\x1b[D\x7f");
    r2_assert("left, left, backspace", strcmp(f.name, "h\xC3\xA9lo") == 0);
    wm_keys(&t, "\x7f"); /* the cursor is right after the 2 byte "é" */
    r2_assert("backspace takes a whole UTF-8 character", strcmp(f.name, "hlo") == 0);
    wm_keys(&t, "\x01"); /* Ctrl+A */
    wm_keys(&t, "\x04"); /* Ctrl+D */
    r2_assert("Ctrl+A, Ctrl+D", strcmp(f.name, "lo") == 0);
    wm_keys(&t, "\x05X"); /* Ctrl+E, X */
    r2_assert("Ctrl+E", strcmp(f.name, "loX") == 0);
    wm_keys(&t, "\x02\x02\x0b"); /* Ctrl+B Ctrl+B Ctrl+K */
    r2_assert("Ctrl+B, Ctrl+K", strcmp(f.name, "l") == 0);
    wm_keys(&t, "\x1b[H\x1b[3~");
    r2_assert("Home, Delete", f.name[0] == 0);
    wm_keys(&t, "aaaaaaaaaaaaaa");
    wm_keys(&t, "\xC3\xA9");
    r2_assert("full: a 2 byte character does not fit", strlen(f.name) == 14);
    wm_keys(&t, "b");
    r2_assert("one byte does", strlen(f.name) == 15);
    wm_keys(&t, "c");
    r2_assert("cap holds", strlen(f.name) == 15);
    r2_assert("scrolled sideways: cursor still in the field", t.cur_x >= 3 + 7 && t.cur_x <= 3 + 7 + 9);
    r2_assert("the end of the text is visible", WF(7 + 8, 0) == 'b');
    wm_keys(&t, "\x1b[H");
    r2_assert("Home scrolls back", t.cur_x == 3 + 7 && WF(7, 0) == 'a');
    wm_keys(&t, "\r");
    r2_assert("Enter submits", f.submits == 1);
    /* the text changed by the program: the cursor stays valid */
    strcpy(f.name, "\xE4\xB8\xAD");
    tui_window_invalidate(&t, tui_focused_window(&t));
    wm_keys(&t, "\x1b[F");
    wm_keys(&t, "!");
    r2_assert("cursor clamped to the new text", strcmp(f.name, "\xE4\xB8\xAD!") == 0);
    r2_assert("wide text: cursor after 2 columns", t.cur_x == 3 + 7 + 3);
    return 0;
}

static const char *test_widgets_focus_and_keys(void)
{
    tui t;
    wform f = {.wrap = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *w = tui_window_open(&t, "Form", (tui_rect){2, 2, 40, 13}, 0, wform_ui, &f);
    tui_update(&t, wm_now);
    wm_keys(&t, "\t");
    r2_assert("Tab to the checkbox", w->focus_kind == R2__TUI_W_CHECK && t.cur_x == 3 + 2 && t.cur_y == 4);
    wm_keys(&t, " ");
    r2_assert("Space toggles", f.wrap == 1 && f.toggles == 1);
    wm_keys(&t, "w");
    r2_assert("hot letter toggles", f.wrap == 0 && f.toggles == 2);
    wm_keys(&t, "\t\t ");
    r2_assert("Tab, Tab, Space: Tabs radio", f.indent == 1 && f.radios == 1);
    wm_keys(&t, " ");
    r2_assert("choosing the chosen radio again is not a change", f.radios == 1);
    wm_keys(&t, "s");
    r2_assert("hot letter radio", f.indent == 0 && f.radios == 2);
    r2_assert("hot letter moved focus there", w->focus_kind == R2__TUI_W_RADIO && t.cur_y == 5 && t.cur_x == 3 + 2);
    wm_keys(&t, "\t\t");
    r2_assert("on the list", w->focus_kind == R2__TUI_W_LIST);
    wm_keys(&t, "\x1b[B");
    r2_assert("Down", f.sel == 1 && f.list_changes == 1);
    wm_keys(&t, "\x1b[F");
    r2_assert("End scrolls the list", f.sel == 7 && WF(2, 3) == 'e' && WF(2, 6) == 'h');
    r2_assert("scrollbar thumb at the bottom", WF(14, 6) == 0x2588);
    wm_keys(&t, "b");
    r2_assert("letter jumps", f.sel == 1 && WF(2, 3) == 'b');
    wm_keys(&t, "o");
    r2_assert("the list takes letters: o is not OK", f.oks == 0);
    wm_keys(&t, "w");
    r2_assert("nor w the checkbox drawn before the list", f.toggles == 2);
    wm_keys(&t, "\033o");
    r2_assert("Alt+O is", f.oks == 1);
    wm_keys(&t, "\r");
    r2_assert("Enter on OK (focus moved there)", f.oks == 2);
    wm_keys(&t, "\x1b[Z");
    wm_keys(&t, "\r");
    r2_assert("Shift+Tab back to the list, Enter submits it", f.list_submits == 1);
    wm_keys(&t, "\t\t ");
    r2_assert("Space presses Cancel", f.cancels == 1);
    wm_keys(&t, "\t");
    r2_assert("Tab wraps to the first widget", w->focus_kind == R2__TUI_W_TEXT);
    wm_keys(&t, "\x1b[Z");
    wm_keys(&t, "c");
    r2_assert("Shift+Tab wraps to the last; c presses Cancel", f.cancels == 2);
    /* the focused widget disappears: focus goes to one that exists */
    tui_window_focus(&t, w);
    wm_keys(&t, "\x1b[Z\x1b[Z\x1b[Z\x1b[Z\x1b[Z");
    r2_assert("on the checkbox", w->focus_kind == R2__TUI_W_CHECK);
    f.gone = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    r2_assert("focus moved off the missing checkbox", w->focus_kind == R2__TUI_W_TEXT);
    /* same label twice: two widgets */
    f.dup = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x1b[Z\r");
    r2_assert("second Go", f.dup_hits[1] == 1 && f.dup_hits[0] == 0);
    wm_keys(&t, "\x1b[Z\r");
    r2_assert("first Go", f.dup_hits[0] == 1 && f.dup_hits[1] == 1);
    return 0;
}

static const char *test_widgets_only_focused_window(void)
{
    tui t;
    wform a = {.wrap = 0}, b = {.wrap = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *wa = tui_window_open(&t, "A", (tui_rect){0, 1, 30, 13}, 0, wform_ui, &a);
    tui_window_open(&t, "B", (tui_rect){30, 1, 30, 13}, 0, wform_ui, &b);
    tui_update(&t, wm_now);
    wm_keys(&t, "xyz");
    r2_assert("keys go to the focused window's widgets", strcmp(b.name, "xyz") == 0 && a.name[0] == 0);
    tui_window_focus(&t, wa);
    wm_keys(&t, "q");
    r2_assert("and follow focus", strcmp(a.name, "q") == 0 && strcmp(b.name, "xyz") == 0);
    r2_assert("only the focused window shows a focused widget",
              TUI_CELL_STYLE(tui_get_cell(&t, 30 + 1 + 7, 2)) == t.theme->input);
    return 0;
}

static int wsv_lines = 30;
static void wscroll_ui(tui *t, tui_window *w, void *user)
{
    char line[32];
    (void)w;
    (void)user;
    tui_at(t, 0, 0);
    tui_label(t, "above");
    tui_scroll_begin(t, "~V~iew", 20, 5, wsv_lines);
    for (int i = 0; i < wsv_lines; i++)
    {
        snprintf(line, sizeof(line), "line %02d", i);
        tui_text(t, 0, i, s8_from_cstr(line), TUI_DEFAULT);
    }
    tui_scroll_end(t);
    tui_label(t, "below");
    tui_button(t, "~B~utton");
}

static const char *test_widgets_scroll_view(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "S", (tui_rect){2, 2, 30, 12}, 0, wscroll_ui, 0);
    tui_update(&t, wm_now);
    /* client at (3, 3): "above" row 0, view rows 1-5, "below" row 6 */
    r2_assert("label above", WF(0, 0) == 'a');
    r2_assert("first lines shown", WF(5, 1) == '0' && WF(6, 1) == '0' && WF(6, 5) == '4');
    r2_assert("content clipped to the view",
              WF(0, 6) == 'b' && !wm_has(&t, "line 05") && !wm_has(&t, "line 07") && !wm_has(&t, "line 08"));
    r2_assert("layout continues below the view", WF(0, 6) == 'b' && WF(1, 7) == ' ' && WF(2, 7) == 'B');
    r2_assert("scrollbar", WF(19, 1) == 0x2588 && WF(19, 3) == 0x2591);
    wm_keys(&t, "\x1b[B");
    r2_assert("Down scrolls one line", WF(6, 1) == '1' && WF(6, 5) == '5');
    wm_keys(&t, "\x1b[6~");
    r2_assert("PgDn scrolls a page", WF(6, 1) == '5');
    wm_keys(&t, "\x1b[F");
    r2_assert("End: the last lines", WF(5, 5) == '2' && WF(6, 5) == '9' && WF(19, 5) == 0x2588);
    for (int i = 0; i < 5; i++)
        wm_keys(&t, "\x1b[B");
    r2_assert("cannot scroll past the end", WF(6, 5) == '9');
    wm_keys(&t, "\x1b[H");
    r2_assert("Home", WF(6, 1) == '0');
    wsv_lines = 3;
    tui_window_invalidate(&t, tui_focused_window(&t));
    tui_update(&t, wm_now);
    r2_assert("short content: no scrollbar", WF(19, 1) == ' ' && WF(6, 3) == '2');
    wsv_lines = 30;
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    return 0;
}

typedef struct wib
{
    char text[24];
    int asking;
    unsigned answer;
} wib;

static void wib_ui(tui *t, tui_window *w, void *user)
{
    wib *b = (wib *)user;
    (void)w;
    tui_text(t, 0, 0, s8_from_cstr(b->text), TUI_DEFAULT);
    if (b->asking)
    {
        unsigned r = tui_input_box(t, "Rename", "New ~n~ame:", b->text, sizeof(b->text));
        if (r)
        {
            b->answer = r;
            b->asking = 0;
        }
    }
}

static const char *test_widgets_input_box(void)
{
    tui t;
    wib b = {"old", 0, 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window *w = tui_window_open(&t, "W", (tui_rect){2, 2, 30, 8}, 0, wib_ui, &b);
    tui_update(&t, wm_now);
    b.asking = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    r2_assert("box shown", wm_has(&t, " Rename ") && wm_has(&t, "New name:") && wm_has(&t, "[ OK ]") &&
                               wm_has(&t, "[ Cancel ]"));
    r2_assert("prefilled", wm_has(&t, "old"));
    wm_keys(&t, "er");
    r2_assert("typed at the end", wm_has(&t, "older") && b.answer == 0);
    r2_assert("caller's text untouched until OK", strcmp(b.text, "old") == 0);
    wm_keys(&t, "\r");
    r2_assert("Enter: OK, text copied", b.answer == TUI_MB_OK && strcmp(b.text, "older") == 0);
    r2_assert("box gone, focus back", !wm_has(&t, " Rename ") && tui_focused_window(&t) == w);
    b.asking = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    wm_keys(&t, "xyz");
    wm_keys(&t, "\x1b");
    r2_assert("Esc: Cancel, text untouched", b.answer == TUI_MB_CANCEL && strcmp(b.text, "older") == 0);
    b.asking = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    wm_keys(&t, "\x15!"); /* Ctrl+U is not an input key: ignored */
    wm_keys(&t, "\t\t\r");
    r2_assert("Tab, Tab, Enter: Cancel button", b.answer == TUI_MB_CANCEL && strcmp(b.text, "older") == 0);
    b.asking = 1;
    tui_window_invalidate(&t, w);
    tui_update(&t, wm_now);
    wm_keys(&t, "\t\r");
    r2_assert("Tab, Enter: OK button (the cancelled \"older!\" is gone)", b.answer == TUI_MB_OK && strcmp(b.text, "older") == 0);
    /* the box's text fits the caller's buffer */
    char tiny[4] = "ab";
    b.asking = 0;
    tui_input_box(&t, "Tiny", "x", tiny, sizeof(tiny));
    tui_update(&t, wm_now);
    wm_keys(&t, "cdef\r");
    r2_assert("answer for an outside caller", tui_input_box(&t, "Tiny", "x", tiny, sizeof(tiny)) == TUI_MB_OK);
    r2_assert("cut to the caller's buffer", strcmp(tiny, "abc") == 0);
    return 0;
}

static const char *test_widgets_state_pool(void)
{
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    for (uint32_t id = 1; id <= R2_TUI_STATE_SLOTS; id++)
    {
        t.frame++;
        tui_wstate *st = r2__tui_state(&t, id);
        st->a = st->b = 99;
    }
    t.frame++;
    r2_assert("existing state found again", r2__tui_state(&t, 5)->a == 99);
    t.frame++;
    tui_wstate *fresh = r2__tui_state(&t, 1000);
    r2_assert("a recycled slot starts clean", fresh->a == 0 && fresh->b == 0);
    r2_assert("the least recently used slot went", r2__tui_state(&t, 5)->a == 99 && r2__tui_state(&t, 1)->a == 0);
    return 0;
}

static void wmany_ui(tui *t, tui_window *w, void *user)
{
    static char bufs[40][8];
    char label[16];
    (void)w;
    (void)user;
    for (int i = 0; i < 40; i++)
    {
        snprintf(label, sizeof(label), "##in%d", i);
        tui_at(t, (i % 4) * 9, i / 4);
        tui_input(t, label, bufs[i], sizeof(bufs[i]), 8);
    }
}

static const char *test_widgets_many(void)
{
    /* more widgets with state than R2_TUI_STATE_SLOTS: no crash, still usable */
    tui t;
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "Many", (tui_rect){0, 1, 40, 14}, 0, wmany_ui, 0);
    tui_update(&t, wm_now);
    for (int i = 0; i < 40; i++)
        wm_keys(&t, "ab\t");
    vt_flush(&t, 1);
    r2_assert(vt_err ? vt_err : "render", !vt_err && vt_matches(&t));
    r2_assert("every input took its text", wm_has(&t, "ab       ab       ab       ab"));
    return 0;
}

static const char *test_widgets_random(void)
{
    static const char *const keys[] = {"\t", "\x1b[Z", " ", "\r", "a", "\xC3\xA9", "\xE4\xB8\xAD", "\x7f", "\x1b[3~",
                                       "\x1b[D", "\x1b[C", "\x1b[H", "\x1b[F", "\x1b[A", "\x1b[B", "\x1b[5~",
                                       "\x1b[6~", "w", "s", "t", "o", "c", "\033o", "\033n", "\x0b", "\x01", "\x05"};
    tui t;
    wform f = {.wrap = 0};
    tui_test_init(&t, VT_COLS, VT_ROWS);
    tui_window_open(&t, "Form", (tui_rect){2, 2, 40, 13}, 0, wform_ui, &f);
    tui_window_open(&t, "S", (tui_rect){20, 4, 30, 12}, 0, wscroll_ui, 0);
    for (int iter = 0; iter < 5000; iter++)
    {
        if (tui_rand() % 40 == 0)
            wm_keys(&t, "\x1b[17~");
        wm_keys(&t, keys[tui_rand() % (sizeof(keys) / sizeof(keys[0]))]);
        r2_assert("name stays valid UTF-8", s8_is_valid_utf8(s8_from_cstr(f.name)));
        r2_assert("name fits", strlen(f.name) < sizeof(f.name));
        r2_assert("selection in range", f.sel >= 0 && f.sel < 8);
        r2_assert("radio value valid", f.indent == 0 || f.indent == 1);
        if (iter % 100 == 0)
        {
            vt_flush(&t, 1);
            r2_assert(vt_err ? vt_err : "random widgets render", !vt_err && vt_matches(&t));
        }
    }
    return 0;
}

////////////////////////////////////////////

static const char *r2_tui_test(void)
{
    r2_run_test(test_tui_init_sizes);

    // width
    r2_run_test(test_tui_width_table_matches_generator);
    r2_run_test(test_tui_width_spot);

    // drawing
    r2_run_test(test_tui_draw_text_cells);
    r2_run_test(test_tui_wide_at_clip_edge);
    r2_run_test(test_tui_overwrite_half_of_wide);
    r2_run_test(test_tui_combining_and_dropped);
    r2_run_test(test_tui_controls_sanitized);
    r2_run_test(test_tui_box_fill_shadow);

    // renderer
    r2_run_test(test_tui_render_first_frame);
    r2_run_test(test_tui_render_minimal_diff);
    r2_run_test(test_tui_render_small_cap);
    r2_run_test(test_tui_render_random);
    r2_run_test(test_tui_render_resize_repaints);

    // input
    r2_run_test(test_tui_keys_table);
    r2_run_test(test_tui_keys_back_to_back);
    r2_run_test(test_tui_esc_timeout);
    r2_run_test(test_tui_bad_input);
    r2_run_test(test_tui_size_query);
    r2_run_test(test_tui_input_fuzz);
    r2_run_test(test_tui_key_names);

    // window manager
    r2_run_test(test_wm_shortcut_parse);
    r2_run_test(test_wm_open_and_draw);
    r2_run_test(test_wm_overlap_and_focus);
    r2_run_test(test_wm_menus);
    r2_run_test(test_wm_system_menu_and_docking);
    r2_run_test(test_wm_move_zoom_close);
    r2_run_test(test_wm_ask_close);
    r2_run_test(test_palette_fuzzy);
    r2_run_test(test_palette);
    r2_run_test(test_palette_desktop_and_modal);
    r2_run_test(test_palette_from_controller);

    // widgets
    r2_run_test(test_widgets_layout);
    r2_run_test(test_widgets_input);
    r2_run_test(test_widgets_focus_and_keys);
    r2_run_test(test_widgets_only_focused_window);
    r2_run_test(test_widgets_scroll_view);
    r2_run_test(test_widgets_input_box);
    r2_run_test(test_widgets_state_pool);
    r2_run_test(test_widgets_many);
    r2_run_test(test_widgets_random);
    r2_run_test(test_wm_prefix);
    r2_run_test(test_wm_prefix_config);
    r2_run_test(test_wm_prefix_modal_and_dock);
    r2_run_test(test_wm_message_box);
    r2_run_test(test_wm_open_from_menu_mid_run);
    r2_run_test(test_wm_menu_overflow);
    r2_run_test(test_wm_no_controller_window);
    r2_run_test(test_wm_limits);
    r2_run_test(test_wm_resize);
    r2_run_test(test_wm_random_keys);
    return 0;
}
