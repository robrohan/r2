/*
    r2_tui demo (step 2: the window manager)

        make demo && ./bin/tui_demo
        ./bin/tui_demo --baud 115200      # throttle output to serial speed

    Window commands: press Ctrl+\, let go, then
        a letter        that menu (the bar follows the focused window)
        1..9            window 1..9 (also brings back a minimized one)
        Space           system menu: move, zoom, minimize, dock, window list
        Up / Down       maximize / restore, then minimize
        Left / Right    previous / next window (Tab and Shift+Tab too)
        Enter           move (arrows) and size (Shift+arrows), Enter / Esc
        Backspace       close
        Ctrl+\          a real Ctrl+\ for the window

    Ctrl+Space (or Ctrl+\ then /) searches every command and window:
    type a few letters (fuzzy: "fnn" finds File > New note), Up / Down
    (or Ctrl+P / Ctrl+N), Enter runs it, Esc closes.

    Ctrl+N new note, Ctrl+Q quit. The Borland keys work too where the
    terminal sends them (F10 menu, F6 next, F5 zoom, Alt+letter menus).
*/
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "../r2_tui_posix.h"

#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COLS 400
#define MAX_ROWS 200
#define LOG_LINES 32

static uint8_t arena[R2_TUI_ARENA_SIZE(MAX_COLS, MAX_ROWS)];
static tui t;
static int running = 1, quitting;
static char log_lines[LOG_LINES][64];
static int log_count;
static tui_window *log_win;
static int note_count;

static void log_msg(const char *msg)
{
    memmove(log_lines[1], log_lines[0], sizeof(log_lines[0]) * (LOG_LINES - 1));
    snprintf(log_lines[0], sizeof(log_lines[0]), "%s", msg);
    log_count++;
    tui_window_invalidate(&t, log_win);
}

/* ---- menus every window shares (immediate mode: just call it) ---- */

static void note_ui(tui *tt, tui_window *w, void *user);
static void glyphs_ui(tui *tt, tui_window *w, void *user);
static void keys_ui(tui *tt, tui_window *w, void *user);

static void new_note(void)
{
    char title[32];
    snprintf(title, sizeof(title), "Note %d", ++note_count);
    if (!tui_window_open(&t, title, (tui_rect){-1, -1, 40, 10}, TUI_WIN_ASK_CLOSE, note_ui, 0))
        log_msg("No room for another window");
    else
        log_msg("Opened a note");
}

static void file_menu(tui *tt)
{
    tui_menu_begin(tt, "~F~ile");
    if (tui_menu_item(tt, "~N~ew note", "Ctrl+N"))
        new_note();
    if (tui_menu_item(tt, "~G~lyphs window", 0))
        tui_window_open(&t, "Glyphs", (tui_rect){-1, -1, 52, 13}, 0, glyphs_ui, 0);
    if (tui_menu_item(tt, "~K~eys window", 0))
        tui_window_open(&t, "Keys", (tui_rect){-1, -1, 30, 12}, 0, keys_ui, 0);
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "~S~earch commands", "Ctrl+Space"))
        tui_palette(tt);
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "E~x~it", "Ctrl+Q"))
        quitting = 1;
    tui_menu_end(tt);
}

static void help_menu(tui *tt)
{
    static int about;
    tui_menu_begin(tt, "~H~elp");
    if (tui_menu_item(tt, "~A~bout", "F1"))
        about = 1;
    tui_menu_end(tt);
    if (about && tui_message(tt, "About",
                             "r2_tui: a keyboard driven terminal UI for Chestnut. No malloc, never blocks, "
                             "sends only the cells that changed.",
                             TUI_MB_OK))
        about = 0;
}

/* quitting asks first; whichever controller runs next shows the question */
static void ask_quit(tui *tt)
{
    unsigned r;
    if (!quitting)
        return;
    r = tui_message(tt, "Quit", "Leave the r2_tui demo?", TUI_MB_YES | TUI_MB_NO);
    if (r == TUI_MB_YES)
        running = 0;
    if (r)
        quitting = 0;
}

/* ---- windows ---- */

static void desktop_ui(tui *tt, tui_window *w, void *user)
{
    (void)w;
    (void)user;
    file_menu(tt);
    help_menu(tt);
    tui_status(tt, " ~Ctrl+\\~ Commands  ~Ctrl+Space~ Search  ~Ctrl+N~ New note  ~Ctrl+Q~ Quit");
    ask_quit(tt);
}

static void note_ui(tui *tt, tui_window *w, void *user)
{
    static const char *const text[] = {"Each window has a controller: a", "function that draws the inside and",
                                       "declares menus every time it runs.", "",
                                       "This note's Note menu replaces the", "desktop's, like the Mac menu bar.", "",
                                       "Ctrl+\\ Backspace asks before closing."};
    static int upper;
    static tui_window *closing;
    int cw, ch;
    (void)user;
    file_menu(tt);
    tui_menu_begin(tt, "~N~ote");
    if (tui_menu_item_flags(tt, "~S~houting", "Ctrl+U", upper ? TUI_ITEM_CHECKED : 0))
    {
        upper = !upper;
        log_msg(upper ? "Shouting on" : "Shouting off");
    }
    if (tui_menu_item(tt, "~R~ename", "F2"))
    {
        char title[32];
        snprintf(title, sizeof(title), "Renamed %d", note_count++);
        tui_window_set_title(tt, w, title);
    }
    tui_menu_end(tt);
    help_menu(tt);
    tui_status(tt, " ~Ctrl+U~ Shout  ~F2~ Rename  ~Ctrl+\\ Bksp~ Close  ~Ctrl+\\ Enter~ Move  ~Ctrl+\\ \xE2\x86\x92~ Next");

    tui_size(tt, &cw, &ch);
    for (int i = 0; i < 8 && i < ch; i++)
    {
        char line[64];
        snprintf(line, sizeof(line), "%s", text[i]);
        if (upper)
            for (char *p = line; *p; p++)
                if (*p >= 'a' && *p <= 'z')
                    *p = (char)(*p - 32);
        tui_text(tt, 1, i, s8_from_cstr(line), TUI_DEFAULT);
    }

    /* TUI_WIN_ASK_CLOSE: closing asks this controller, which asks the user */
    if (tui_closing(tt))
        closing = w;
    if (closing == w)
    {
        unsigned r = tui_message(tt, "Close", "Close this note?", TUI_MB_YES | TUI_MB_NO);
        if (r == TUI_MB_YES)
        {
            tui_window_close(tt, w);
            log_msg("Closed a note");
        }
        if (r)
            closing = 0;
    }
    ask_quit(tt);
}

static void glyphs_ui(tui *tt, tui_window *w, void *user)
{
    tui_style label = TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, 0);
    int y = 0;
    (void)w;
    (void)user;
    file_menu(tt);
    help_menu(tt);
    tui_text(tt, 1, y, S8("Colours"), label);
    for (int c = 0; c < 16; c++)
        tui_text(tt, 12 + c * 2, y, S8("  "), TUI_STYLE(TUI_WHITE, (unsigned)c, 0));
    y += 2;
    tui_text(tt, 1, y, S8("Wide"), label);
    tui_text(tt, 12, y++, S8("\xE4\xB8\xAD\xE6\x96\x87 \xE6\xBC\xA2\xE5\xAD\x97 \xF0\x9F\x91\x8B \xF0\x9F\x90\xB1 ok"),
             TUI_DEFAULT);
    tui_text(tt, 1, y, S8("Combining"), label);
    tui_text(tt, 12, y++, S8("e\xCC\x81 a\xCC\x88 n\xCC\x83 (built from marks)"), TUI_DEFAULT);
    tui_text(tt, 1, y, S8("ZWJ"), label);
    tui_text(tt, 12, y++, S8("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA6 drawn as parts"),
             TUI_DEFAULT);
    tui_text(tt, 1, y, S8("Controls"), label);
    tui_text(tt, 12, y++, S8("\x1b[31mnot red\x07 (sanitized)"), TUI_DEFAULT);
    tui_text(tt, 1, y, S8("Styles"), label);
    tui_text(tt, 12, y, S8("bold"), TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, TUI_BOLD));
    tui_text(tt, 17, y, S8("underline"), TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, TUI_UNDERLINE));
    tui_text(tt, 27, y++, S8("reverse"), TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, TUI_REVERSE));
    tui_box(tt, (tui_rect){1, y + 1, 20, 3}, TUI_DEFAULT, TUI_BOX_SINGLE);
    tui_box(tt, (tui_rect){23, y + 1, 20, 3}, TUI_DEFAULT, TUI_BOX_DOUBLE);
    tui_text(tt, 3, y + 2, S8("single box"), TUI_DEFAULT);
    tui_text(tt, 25, y + 2, S8("double box"), TUI_DEFAULT);
    ask_quit(tt);
}

/* echoes the keys it gets (the window manager's own keys never arrive) */
static void keys_ui(tui *tt, tui_window *w, void *user)
{
    static char keys[10][32];
    static int n;
    const tui_event *ev = tui_key(tt);
    int cw, ch;
    (void)w;
    (void)user;
    file_menu(tt);
    help_menu(tt);
    if (ev)
    {
        memmove(keys[1], keys[0], sizeof(keys[0]) * 9);
        tui_key_name(ev, keys[0], sizeof(keys[0]));
        n++;
        tui_use_key(tt);
    }
    tui_size(tt, &cw, &ch);
    for (int i = 0; i < 10 && i < n && i < ch - 1; i++)
        tui_text(tt, 1, i, s8_from_cstr(keys[i]),
                 i == 0 ? TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, TUI_BOLD) : TUI_DEFAULT);
    tui_text(tt, 1, ch - 1, S8("> "), TUI_DEFAULT);
    tui_cursor(tt, 3, ch - 1);
    tui_status(tt, " Type anything: it shows up here  ~Ctrl+\\ \xE2\x86\x92~ Next window");
    ask_quit(tt);
}

static void files_ui(tui *tt, tui_window *w, void *user)
{
    static const char *const files[] = {"boot.lisp", "kernel/",   "  sched.c", "  uart.c",
                                        "lib/",      "  r2_tui.h", "  r2_strings.h", "README.md",
                                        "\xE4\xB8\xAD\xE6\x96\x87.txt"};
    (void)w;
    (void)user;
    file_menu(tt);
    help_menu(tt);
    for (int i = 0; i < 9; i++)
        tui_text(tt, 1, i, s8_from_cstr(files[i]), TUI_DEFAULT);
    tui_status(tt, " Docked left: ~Ctrl+\\ Enter~ then ~arrows~ to resize  ~Ctrl+\\ Space~ to float");
    ask_quit(tt);
}

static void log_ui(tui *tt, tui_window *w, void *user)
{
    int cw, ch;
    (void)w;
    (void)user;
    file_menu(tt);
    help_menu(tt);
    tui_size(tt, &cw, &ch);
    for (int i = 0; i < ch && i < log_count && i < LOG_LINES; i++)
        tui_text(tt, 1, ch - 1 - i, s8_from_cstr(log_lines[i]),
                 i == 0 ? TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, 0) : TUI_DEFAULT);
    tui_status(tt, " Docked bottom: the demo's log");
    ask_quit(tt);
}

static void resize(void)
{
    int cols, rows;
    if (tui_posix_size(&cols, &rows) != 0)
        cols = 80, rows = 25;
    if (cols > MAX_COLS)
        cols = MAX_COLS;
    if (rows > MAX_ROWS)
        rows = MAX_ROWS;
    tui_resize(&t, cols, rows);
}

int main(int argc, char **argv)
{
    tui_posix p;
    tui_window *files;
    uint8_t in[256], out[512];
    long baud = 0;

    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--baud") == 0)
            baud = atol(argv[i + 1]);

    if (tui_init(&t, arena, sizeof(arena), 80, 25) != 0)
        return 1;
    if (tui_posix_open(&p) != 0)
    {
        fprintf(stderr, "tui_demo: needs a terminal\n");
        return 1;
    }
    resize();

    tui_set_desktop(&t, desktop_ui, 0);
    files = tui_window_open(&t, "Files", (tui_rect){0, 0, 0, 0}, TUI_WIN_NO_CLOSE, files_ui, 0);
    tui_window_dock(&t, files, TUI_DOCK_LEFT, 20);
    log_win = tui_window_open(&t, "Log", (tui_rect){0, 0, 0, 0}, TUI_WIN_NO_CLOSE, log_ui, 0);
    tui_window_dock(&t, log_win, TUI_DOCK_BOTTOM, 6);
    tui_window_open(&t, "Glyphs", (tui_rect){-1, -1, 52, 13}, 0, glyphs_ui, 0);
    tui_window_open(&t, "Keys", (tui_rect){-1, -1, 30, 12}, 0, keys_ui, 0);
    new_note();

    while (running)
    {
        uint32_t now = tui_posix_now_ms();
        size_t n;

        if (tui_posix_resized())
            resize();
        n = tui_posix_read(in, sizeof(in));
        tui_feed(&t, in, n, now);
        tui_update(&t, now);

        /* Full speed: send the whole frame. Throttled: one chunk per loop,
           sleeping for its transmission time, like a cooperative task on a
           serial line. */
        do
        {
            n = tui_render(&t, out, baud ? 128 : sizeof(out));
            if (n)
            {
                tui_posix_write(out, n);
                if (baud)
                {
                    long ns = (long)((double)n * 10.0 * 1e9 / (double)baud);
                    struct timespec ts = {ns / 1000000000L, ns % 1000000000L};
                    nanosleep(&ts, 0);
                }
            }
        } while (n && !baud);

        if (running && !tui_render_pending(&t))
            tui_posix_wait(tui_input_pending(&t) ? (int)t.esc_ms : 1000);
    }
    tui_posix_close(&p);
    return 0;
}
