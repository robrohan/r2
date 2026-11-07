/*
    r2_tui app: the window manager on its own (no editor, no test windows).
    A starting point to copy for your own app.

        make app && ./bin/tui_app

    Ctrl+\ then a key for window commands (a letter: that menu, 1-9: that
    window, Space: system menu, arrows: max / min / next, Enter: move,
    Backspace: close), Ctrl+] to search every command, Ctrl+Q to quit.
*/
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "../r2_tui_posix.h"

#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"

#include <stdio.h>

#define MAX_COLS 400
#define MAX_ROWS 200

static uint8_t arena[R2_TUI_ARENA_SIZE(MAX_COLS, MAX_ROWS)];
static tui t;
static int running = 1, quitting, windows;

static void doc_ui(tui *tt, tui_window *w, void *user);

/* menus every window shares: in immediate mode, just call a function */
static void app_menus(tui *tt)
{
    static int about;
    tui_menu_begin(tt, "~F~ile");
    if (tui_menu_item(tt, "~N~ew window", "Ctrl+N"))
    {
        char title[32];
        snprintf(title, sizeof(title), "Window %d", ++windows);
        tui_window_open(&t, title, (tui_rect){-1, -1, 40, 10}, 0, doc_ui, 0);
    }
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "E~x~it", "Ctrl+Q"))
        quitting = 1;
    tui_menu_end(tt);

    tui_menu_begin(tt, "~H~elp");
    if (tui_menu_item(tt, "~A~bout", "F1"))
        about = 1;
    tui_menu_end(tt);

    if (about && tui_message(tt, "About", "A starter r2_tui app: windows, menus, docking, dialogs.", TUI_MB_OK))
        about = 0;
    if (quitting)
    {
        unsigned r = tui_message(tt, "Quit", "Leave the app?", TUI_MB_YES | TUI_MB_NO);
        if (r == TUI_MB_YES)
            running = 0;
        if (r)
            quitting = 0;
    }
}

static void desktop_ui(tui *tt, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    app_menus(tt);
    tui_status(tt, " ~Ctrl+\\~ Commands  ~Ctrl+]~ Search  ~Ctrl+N~ New window  ~Ctrl+Q~ Quit");
}

/* a window: draw what it shows, declare its menus */
static void doc_ui(tui *tt, tui_window *w, void *user)
{
    int cw, ch;
    (void)user;
    app_menus(tt);
    tui_size(tt, &cw, &ch);
    tui_text(tt, 1, 0, s8_from_cstr(w->title), TUI_DEFAULT);
    tui_text(tt, 1, 2, S8("Your content goes here."), TUI_DEFAULT);
    tui_text(tt, 1, ch - 1, S8("Ctrl+\\ then Enter to move me."), TUI_DEFAULT);
}

static void welcome_ui(tui *tt, tui_window *w, void *user)
{
    static const char *const lines[] = {
        "Welcome to r2_tui.",
        "",
        "Ctrl+\\ then a key runs window commands:",
        "  a letter   that menu (F: File, H: Help)",
        "  1-9        that window",
        "  Space      the system menu",
        "  Up / Down  maximize / restore, minimize",
        "  Left/Right previous / next window",
        "  Enter      move (arrows), size (Shift+arrows)",
        "  Backspace  close",
        "",
        "Ctrl+] searches every command and window.",
    };
    (void)w;
    (void)user;
    app_menus(tt);
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
        tui_text(tt, 1, i, s8_from_cstr(lines[i]), TUI_DEFAULT);
}

static void side_ui(tui *tt, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    app_menus(tt);
    tui_text(tt, 1, 0, S8("A docked pane."), TUI_DEFAULT);
    tui_text(tt, 1, 2, S8("Ctrl+\\ Space:"), TUI_DEFAULT);
    tui_text(tt, 1, 3, S8("dock elsewhere"), TUI_DEFAULT);
    tui_text(tt, 1, 4, S8("or float it."), TUI_DEFAULT);
}

static void resize(void)
{
    int cols, rows;
    if (tui_posix_size(&cols, &rows) != 0)
        cols = 80, rows = 25;
    tui_resize(&t, cols < MAX_COLS ? cols : MAX_COLS, rows < MAX_ROWS ? rows : MAX_ROWS);
}

int main(void)
{
    tui_posix p;
    tui_window *side;
    uint8_t in[256], out[1024];

    if (tui_init(&t, arena, sizeof(arena), 80, 25) != 0)
        return 1;
    if (tui_posix_open(&p) != 0)
    {
        fprintf(stderr, "tui_app: needs a terminal\n");
        return 1;
    }
    resize();

    tui_set_desktop(&t, desktop_ui, 0);
    side = tui_window_open(&t, "Side", (tui_rect){0, 0, 0, 0}, TUI_WIN_NO_CLOSE, side_ui, 0);
    tui_window_dock(&t, side, TUI_DOCK_LEFT, 20);
    tui_window_open(&t, "Welcome", (tui_rect){-1, -1, 52, 14}, 0, welcome_ui, 0);

    while (running)
    {
        uint32_t now = tui_posix_now_ms();
        size_t n;
        if (tui_posix_resized())
            resize();
        n = tui_posix_read(in, sizeof(in));
        tui_feed(&t, in, n, now);
        tui_update(&t, now);
        while ((n = tui_render(&t, out, sizeof(out))) > 0)
            tui_posix_write(out, n);
        if (running)
            tui_posix_wait(tui_input_pending(&t) ? (int)t.esc_ms : 1000);
    }
    tui_posix_close(&p);
    return 0;
}
