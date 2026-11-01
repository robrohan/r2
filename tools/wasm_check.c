/*
    Builds the freestanding headers for wasm32 with clang alone (no libc,
    no emcc): r2_strings.h, r2_quant.h (+ r2_simd.h, scalar path),
    r2_tui.h (+ r2_termui.h) and r2_tui_edit.h. `make wasm` links this
    with wasm-ld, which fails if anything needs a function the module
    doesn't define: that is
    the check that these headers stay usable bare-metal (Chestnut) and in a
    browser. r2_maths.h is left out because it needs libm and stdio.

    The exports are a small C ABI a host (e.g. a JS terminal) drives:
    write key bytes into r2_in(), call r2_feed / r2_update, then read
    r2_render() bytes from r2_out(). examples/wasm/index.html is such a
    host: a test page with the editor and the widgets.
*/
#define R2_STRINGS_IMPLEMENTATION
#include "../r2_strings.h"

#define R2_QUANT_IMPLEMENTATION
#include "../r2_quant.h"

#define R2_TUI_CMD_POOL (32 * 1024) /* an editor filling a big window */
#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"

#define R2_TUI_EDIT_IMPLEMENTATION
#include "../r2_tui_edit.h"

#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(#name)))
#else
#define EXPORT(name) /* tools/wasm_host_test.c builds this natively */
#endif

/* Compilers may emit calls to these four even in freestanding code (e.g.
   for `= {0}`), so every freestanding target supplies them; the headers
   never call them. volatile keeps clang from turning the loops back into
   calls to themselves. */
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

void *memset(void *dst, int c, size_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (n--)
        *d++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s)
        while (n--)
            *d++ = *s++;
    else
        while (n--)
            d[n] = s[n];
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x < *y ? -1 : 1;
    return 0;
}

static uint8_t arena[R2_TUI_ARENA_SIZE(160, 60)];
static uint8_t in_buf[256], out_buf[4096];
static tui t;

static char edit_buf[16 * 1024], clip_buf[4096], undo_buf[8192];
static tui_edit ed;
static tui_clipboard clip;
static tui_window *ed_win, *wid_win;
static int about, greeting;

/* name, check, radio and list state for the widgets window */
static char name[32] = "wasm";
static int shout, size = 1, colour;

static void editor_ui(tui *tt, tui_window *w, void *user);
static void widgets_ui(tui *tt, tui_window *w, void *user);

/* no stdio here: numbers by hand */
static size_t put_u(char *buf, size_t n, size_t cap, uint32_t v)
{
    char tmp[10];
    int k = 0;
    do
        tmp[k++] = (char)('0' + v % 10);
    while ((v /= 10) && k < 10);
    while (k && n + 1 < cap)
        buf[n++] = tmp[--k];
    buf[n] = 0;
    return n;
}

static int is_open(tui_window *w, tui_ui_fn ui)
{
    return w && w->used && !w->dead && w->ui == ui;
}

static void open_editor(void)
{
    if (is_open(ed_win, editor_ui))
        tui_window_focus(&t, ed_win);
    else
        ed_win = tui_window_open(&t, "Editor", (tui_rect){1, 2, 64, 18}, 0, editor_ui, 0);
}

static void open_widgets(void)
{
    if (is_open(wid_win, widgets_ui))
        tui_window_focus(&t, wid_win);
    else
    {
        tui_rect r = {-1, -1, 40, 17};
        if (t.cols >= 1 + 64 + 40 + 2) /* beside the editor when there is room */
            r.x = t.cols - 40 - 2, r.y = 2;
        wid_win = tui_window_open(&t, "Widgets", r, 0, widgets_ui, 0); /* not a dialog: those open centred */
    }
}

/* menus every window shares */
static void app_menus(tui *tt)
{
    tui_menu_begin(tt, "~W~indow");
    if (tui_menu_item(tt, "~E~ditor", "F8"))
        open_editor();
    if (tui_menu_item(tt, "~W~idgets", "F9"))
        open_widgets();
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "~S~earch commands", "Ctrl+]"))
        tui_palette(tt);
    tui_menu_end(tt);
    tui_menu_begin(tt, "~H~elp");
    if (tui_menu_item(tt, "~A~bout", 0))
        about = 1;
    tui_menu_end(tt);
    if (about && tui_message(tt, "About",
                             "r2_tui compiled to wasm32 with clang alone: no libc, no emcc, nothing imported.\n"
                             "This page is the terminal.",
                             TUI_MB_OK))
        about = 0;
}

static void desktop_ui(tui *tt, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    app_menus(tt);
    tui_status(tt, " ~Ctrl+\\~ Commands  ~Ctrl+]~ Search  ~F10~ Menu  ~F8~ Editor  ~F9~ Widgets");
}

static void editor_ui(tui *tt, tui_window *w, void *user)
{
    char status[R2_TUI_STATUS_MAX];
    int cw, ch;
    size_t n;
    uint32_t from, to;
    (void)w;
    (void)user;
    tui_menu_begin(tt, "~E~dit");
    /* Emacs notation: shown, but not bound, so the keys stay with the editor */
    if (tui_menu_item_flags(tt, "~U~ndo", "C-/", ed.upos ? 0 : TUI_ITEM_DISABLED))
        tui_edit_undo(&ed);
    if (tui_menu_item_flags(tt, "~R~edo", "C-M-_", ed.upos < ed.ulen ? 0 : TUI_ITEM_DISABLED))
        tui_edit_redo(&ed);
    tui_menu_separator(tt);
    if (tui_menu_item_flags(tt, "Cu~t~", "C-w", tui_edit_selection(&ed, &from, &to) ? 0 : TUI_ITEM_DISABLED))
        tui_edit_cut(&ed);
    if (tui_menu_item_flags(tt, "~C~opy", "M-w", tui_edit_selection(&ed, &from, &to) ? 0 : TUI_ITEM_DISABLED))
        tui_edit_copy(&ed);
    if (tui_menu_item_flags(tt, "~P~aste", "C-y", clip.len ? 0 : TUI_ITEM_DISABLED))
        tui_edit_paste(&ed);
    tui_menu_separator(tt);
    if (tui_menu_item_flags(tt, "~L~ine numbers", "F4", ed.flags & TUI_EDIT_LINE_NUMBERS ? TUI_ITEM_CHECKED : 0))
        ed.flags ^= TUI_EDIT_LINE_NUMBERS;
    if (tui_menu_item_flags(tt, "~A~uto indent", 0, ed.flags & TUI_EDIT_AUTO_INDENT ? TUI_ITEM_CHECKED : 0))
        ed.flags ^= TUI_EDIT_AUTO_INDENT;
    tui_menu_end(tt);
    tui_menu_begin(tt, "~S~earch");
    if (tui_menu_item(tt, "~F~ind", "C-s"))
        tui_edit_search(&ed, 0);
    if (tui_menu_item(tt, "Find ~b~ackward", "C-r"))
        tui_edit_search(&ed, 1);
    if (tui_menu_item(tt, "~R~eplace", "M-%"))
        tui_edit_replace(&ed);
    tui_menu_end(tt);
    app_menus(tt);

    tui_size(tt, &cw, &ch);
    tui_at(tt, 0, 0);
    tui_editor(tt, "##ed", &ed, cw, ch);

    status[0] = ' ';
    if (tui_edit_prompt(&ed, status + 1, sizeof(status) - 1))
    {
        tui_status(tt, status); /* searching / replacing */
        return;
    }
    n = r2__tui_append(status, 1, sizeof(status), "Line ", 1);
    n = put_u(status, n, sizeof(status), ed.line + 1);
    n = r2__tui_append(status, n, sizeof(status), "  Col ", 1);
    n = put_u(status, n, sizeof(status), tui_edit_column(&ed) + 1);
    if (ed.modified)
        n = r2__tui_append(status, n, sizeof(status), "  Modified", 1);
    r2__tui_append(status, n, sizeof(status), "  ~C-SPC~ Mark  ~C-w~ Cut  ~C-y~ Paste  ~C-/~ Undo  ~C-s~ Find", 0);
    tui_status(tt, status);
}

static void widgets_ui(tui *tt, tui_window *w, void *user)
{
    static const char *const colours[] = {"Borland blue", "Amber", "Green screen", "Paper", "Midnight"};
    (void)w;
    (void)user;
    app_menus(tt);
    tui_at(tt, 2, 1);
    tui_label(tt, "~N~ame:");
    tui_same_line(tt);
    tui_input(tt, "~N~ame##name", name, sizeof(name), 20);
    tui_space(tt, 1);
    /* hot letters keep clear of the menu titles (W, H): Alt+letter goes to the menu bar first */
    tui_checkbox(tt, "~S~hout it", &shout);
    tui_radio(tt, "S~m~all", &size, 0);
    tui_same_line(tt);
    tui_radio(tt, "M~e~dium", &size, 1);
    tui_same_line(tt);
    tui_radio(tt, "~L~arge", &size, 2);
    tui_space(tt, 1);
    tui_list(tt, "colours", colours, 5, &colour, 20, 4);
    tui_space(tt, 1);
    if (tui_button(tt, "~G~reet"))
        greeting = 1;
    tui_same_line(tt);
    if (tui_button(tt, "~C~lose"))
        tui_window_close(tt, wid_win);
    tui_status(tt, " ~Tab~ Next  ~Alt+letter~ or the letter  ~Space~ Toggle  ~Enter~ Press");
    if (greeting)
    {
        char msg[96];
        size_t n = r2__tui_append(msg, 0, sizeof(msg), shout ? "HELLO, " : "Hello, ", 1);
        n = r2__tui_append(msg, n, sizeof(msg), name[0] ? name : "nobody", 1);
        n = r2__tui_append(msg, n, sizeof(msg), shout ? "!!!\nIn " : ".\nIn ", 1);
        r2__tui_append(msg, n, sizeof(msg), colours[colour], 1);
        if (tui_message(tt, "Greeting", msg, TUI_MB_OK))
            greeting = 0;
    }
}

EXPORT(r2_init) int r2_init(int cols, int rows)
{
    if (tui_init(&t, arena, sizeof(arena), cols, rows) != 0)
        return -1;
    tui_edit_init(&ed, edit_buf, sizeof(edit_buf));
    tui_clipboard_init(&clip, clip_buf, sizeof(clip_buf));
    ed.clip = &clip;
    tui_edit_undo_init(&ed, undo_buf, sizeof(undo_buf));
    ed.flags = TUI_EDIT_LINE_NUMBERS | TUI_EDIT_AUTO_INDENT;
    tui_edit_set(&ed, S8("r2_tui in wasm32: clang alone, no libc, nothing imported.\n"
                         "\n"
                         "Emacs keys: Ctrl+A/E/B/F/P/N, Ctrl+K kill, Ctrl+Y yank,\n"
                         "Ctrl+Space mark, Ctrl+W cut, Esc w copy, Ctrl+/ undo,\n"
                         "Ctrl+S / Ctrl+R search, Esc % replace (y n ! . q).\n"
                         "\n"
                         "The browser keeps a few Ctrl keys for itself (Ctrl+W\n"
                         "closes the tab, Ctrl+N and Ctrl+T open windows): use\n"
                         "the buttons under the terminal for those.\n"
                         "\n"
                         "Ctrl+\\ then a key: windows. Ctrl+] searches commands.\n"));
    tui_set_desktop(&t, desktop_ui, 0);
    wid_win = 0;
    ed_win = 0;
    open_widgets();
    open_editor();
    return 0;
}

EXPORT(r2_in) uint8_t *r2_in(void)
{
    return in_buf;
}

EXPORT(r2_out) uint8_t *r2_out(void)
{
    return out_buf;
}

EXPORT(r2_feed) void r2_feed(int n, uint32_t now_ms)
{
    tui_feed(&t, in_buf, (size_t)n, now_ms);
}

EXPORT(r2_update) int r2_update(uint32_t now_ms)
{
    return tui_update(&t, now_ms);
}

EXPORT(r2_resize) int r2_resize(int cols, int rows)
{
    return tui_resize(&t, cols, rows);
}

EXPORT(r2_render) int r2_render(void)
{
    return (int)tui_render(&t, out_buf, sizeof(out_buf));
}

/* 1 while a lone Esc waits: call r2_update again after r2_esc_ms() */
EXPORT(r2_pending) int r2_pending(void)
{
    return tui_input_pending(&t);
}

EXPORT(r2_esc_ms) int r2_esc_ms(void)
{
    return (int)t.esc_ms;
}

/* the host terminal's cursor advances by this much per character */
EXPORT(r2_width) int r2_width(uint32_t r)
{
    return tui_rune_width(r);
}

/* what the library drew (rune | style << 21), to check the host's screen */
EXPORT(r2_cell) uint32_t r2_cell(int x, int y)
{
    return tui_get_cell(&t, x, y);
}

/* the other freestanding headers, kept alive by exporting them */
EXPORT(r2_rune_count) uint32_t r2_rune_count(const uint8_t *s, uint32_t n)
{
    return s8_rune_count(s8_from_bytes(s, n));
}

EXPORT(r2_dot_i8) int32_t r2_dot_i8(const int8_t *a, const int8_t *b, int n)
{
    return vecn_dot_i8(a, b, n);
}

EXPORT(r2_quantize_i8) float r2_quantize_i8_export(const float *in, int n, int8_t *out)
{
    return r2_quantize_i8(in, n, out);
}

EXPORT(r2_mat_mul_i4) void r2_mat_mul_i4_export(const uint8_t *a, const uint8_t *b, unsigned r, unsigned c, unsigned k,
                                                int32_t *out)
{
    mat_mul_i4(a, b, r, c, c, k, out);
}
