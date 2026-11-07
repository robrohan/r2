/*
    The window manager and widgets driven by arbitrary keys: menus, the
    Ctrl+\ prefix, the palette, message and input boxes, docking, resizes
    (answers to size queries), every widget type.

    Data: one byte of setup, then the key stream (see fuzz.h).
*/
#define R2_TUI_IMPLEMENTATION
#include "../../tui/r2_tui.h"
#include "fuzz.h"

#include <string.h>

#define COLS 60
#define ROWS 20

static uint8_t arena[R2_TUI_ARENA_SIZE(COLS, ROWS)];

static const char *const items[] = {"apple", "banana", "cherry", "date", "elderberry", "fig", "grape"};

typedef struct app
{
    const char *menu;
    int check, radio, sel, ask, asking_input, open_child;
    char name[12], answer[16];
} app;

static app apps[4], desk;

static void app_ui(tui *t, tui_window *w, void *user)
{
    app *a = (app *)user;
    if (a->menu)
    {
        tui_menu_begin(t, a->menu);
        if (tui_menu_item(t, "~A~sk", "F2"))
            a->ask = 1;
        if (tui_menu_item_flags(t, "~D~isabled", 0, TUI_ITEM_DISABLED))
            FUZZ_CHECK("disabled item ran", 0);
        tui_menu_separator(t);
        if (tui_menu_item_flags(t, "C~h~eck", 0, a->check ? TUI_ITEM_CHECKED : 0))
            a->check = !a->check;
        if (tui_menu_item(t, "~I~nput", 0))
            a->asking_input = 1;
        if (tui_menu_item(t, "~N~ew window", 0))
            a->open_child = 1;
        if (tui_menu_item(t, "Doc~k~", 0))
            tui_window_dock(t, w, 1 + (a->sel % 3), 3 + a->sel);
        tui_menu_end(t);
    }
    tui_at(t, 0, 0);
    tui_label(t, "Name:");
    tui_same_line(t);
    tui_input(t, "##name", a->name, sizeof(a->name), 10);
    tui_checkbox(t, "~C~heck", &a->check);
    tui_radio(t, "~O~ne", &a->radio, 0);
    tui_same_line(t);
    tui_radio(t, "~T~wo", &a->radio, 1);
    tui_list(t, "##list", items, (int)(sizeof(items) / sizeof(items[0])), &a->sel, 12, 3);
    if (tui_button(t, "~B~utton"))
        a->ask = 1;
    tui_scroll_begin(t, "##scroll", 14, 2, 6);
    for (int i = 0; i < 6; i++)
        tui_text(t, 0, i, S8("scrolled \xE4\xB8\xAD line"), TUI_DEFAULT);
    tui_scroll_end(t);
    if (tui_closing(t))
        tui_window_close(t, w);
    if (a->ask && tui_message(t, "Quit?", "Really quit? Unsaved work\nis lost.", TUI_MB_YES | TUI_MB_NO | TUI_MB_CANCEL))
        a->ask = 0;
    if (a->asking_input && tui_input_box(t, "Name", "Your name:", a->answer, sizeof(a->answer)))
        a->asking_input = 0;
}

/* the desktop: menus, a status line and a message box, no widgets */
static void desk_ui(tui *t, tui_window *w, void *user)
{
    app *a = (app *)user;
    (void)w;
    tui_menu_begin(t, a->menu);
    if (tui_menu_item(t, "~A~sk", "F2"))
        a->ask = 1;
    if (tui_menu_item(t, "~N~ew window", "Ctrl+N"))
        a->open_child = 1;
    tui_menu_end(t);
    tui_status(t, " ~Ctrl+\\~ Commands  ~Ctrl+]~ Search  ~F2~ Ask");
    if (a->ask && tui_message(t, "Hello", "From the desktop", TUI_MB_OK))
        a->ask = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_stream s;
    uint8_t chunk[FUZZ_CHUNK_MAX], q[64];
    size_t n;
    uint32_t ms, now = 1000;
    int what;
    tui t;
    if (size < 1)
        return 0;
    memset(apps, 0, sizeof(apps));
    memset(&desk, 0, sizeof(desk));
    apps[0].menu = "~F~ile";
    apps[1].menu = "~E~dit";
    apps[3].menu = "~F~ile";
    desk.menu = "~D~esk";
    FUZZ_CHECK("init", tui_init(&t, arena, sizeof(arena), COLS, ROWS) == 0);
    tui_set_desktop(&t, desk_ui, &desk);
    for (int i = 0; i < 1 + data[0] % 4; i++)
        tui_window_open(&t, "W", (tui_rect){-1, -1, 0, 0}, (data[0] >> (2 + i)) & 1 ? TUI_WIN_ASK_CLOSE : 0, app_ui,
                        &apps[i]);
    tui_update(&t, now);

    s = (fuzz_stream){data + 1, size - 1, 0};
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
        if (desk.open_child)
        {
            desk.open_child = 0;
            tui_window_open(&t, "New", (tui_rect){-1, -1, 0, 0}, 0, app_ui, &apps[2]);
        }
        for (int i = 0; i < 4; i++)
            if (apps[i].open_child)
            {
                apps[i].open_child = 0;
                tui_window_open(&t, "Child", (tui_rect){-1, -1, 0, 0}, 0, app_ui, &apps[(i + 1) % 4]);
            }
        fuzz_render(&t);
        FUZZ_CHECK("size in the arena", t.cols > 0 && t.rows > 0 && (uint32_t)(t.cols * t.rows) <= t.cap_cells);
        if (t.focus >= 0)
            FUZZ_CHECK("focus on a closed window", t.win[t.focus].used);
        for (int i = 0; i < 4; i++)
            FUZZ_CHECK("input stays NUL terminated", memchr(apps[i].name, 0, sizeof(apps[i].name)) &&
                                                         memchr(apps[i].answer, 0, sizeof(apps[i].answer)));
    }
    return 0;
}
