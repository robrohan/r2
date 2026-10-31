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

    F8 (File > Editor) opens the text editor widget: arrows, Home/End,
    PgUp/PgDn, Ctrl+Home/End, Ctrl+Left/Right, and Emacs Ctrl+A/E/B/F/P/N/D/K;
    Ctrl+Space mark, Ctrl+W / Esc w / Ctrl+Y cut / copy / paste.

    F9 (File > Preferences) shows the widgets: text input, checkboxes,
    radios, a list, a scrolling view and buttons. Tab moves between them.

    Ctrl+] (or Ctrl+\ then /) searches every command and window:
    type a few letters (fuzzy: "fnn" finds File > New note), Up / Down
    (or Ctrl+P / Ctrl+N), Enter runs it, Esc closes.

    Ctrl+N new note, Ctrl+Q quit. The Borland keys work too where the
    terminal sends them (F10 menu, F6 next, F5 zoom, Alt+letter menus).
*/
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "../r2_tui_posix.h"

/* the editor window draws a lot: more room for recorded drawing */
#define R2_TUI_CMD_POOL (32 * 1024)
#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"
#define R2_TUI_EDIT_IMPLEMENTATION
#include "../r2_tui_edit.h"

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
static void prefs_ui(tui *tt, tui_window *w, void *user);
static tui_window *prefs_win;
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

static void editor_ui(tui *tt, tui_window *w, void *user);
static tui_window *editor_win;

static void open_editor(void)
{
    if (editor_win && editor_win->used)
        tui_window_focus(&t, editor_win);
    else
        editor_win = tui_window_open(&t, "Editor", (tui_rect){-1, -1, 64, 18}, 0, editor_ui, 0);
}

/* in the editor, Ctrl+N is Emacs "next line", so New note has no shortcut there */
static void file_menu_for(tui *tt, int in_editor)
{
    tui_menu_begin(tt, "~F~ile");
    if (tui_menu_item(tt, "~N~ew note", in_editor ? 0 : "Ctrl+N"))
        new_note();
    if (tui_menu_item(tt, "~E~ditor", "F8"))
        open_editor();
    if (tui_menu_item(tt, "~G~lyphs window", 0))
        tui_window_open(&t, "Glyphs", (tui_rect){-1, -1, 52, 13}, 0, glyphs_ui, 0);
    if (tui_menu_item(tt, "~K~eys window", 0))
        tui_window_open(&t, "Keys", (tui_rect){-1, -1, 30, 12}, 0, keys_ui, 0);
    if (tui_menu_item(tt, "~P~references (widgets)", "F9"))
    {
        if (prefs_win && prefs_win->used)
            tui_window_focus(&t, prefs_win);
        else
            prefs_win = tui_window_open(&t, "Preferences", (tui_rect){-1, -1, 58, 19}, TUI_WIN_DIALOG, prefs_ui, 0);
    }
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "~S~earch commands", "Ctrl+]"))
        tui_palette(tt);
    tui_menu_separator(tt);
    if (tui_menu_item(tt, "E~x~it", "Ctrl+Q"))
        quitting = 1;
    tui_menu_end(tt);
}

static void file_menu(tui *tt)
{
    file_menu_for(tt, 0);
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
    tui_status(tt, " ~Ctrl+\\~ Commands  ~Ctrl+]~ Search  ~Ctrl+N~ New note  ~Ctrl+Q~ Quit");
    ask_quit(tt);
}

static void note_ui(tui *tt, tui_window *w, void *user)
{
    static const char *const text[] = {"Each window has a controller: a", "function that draws the inside and",
                                       "declares menus every time it runs.", "",
                                       "This note's Note menu replaces the", "desktop's, like the Mac menu bar.", "",
                                       "Ctrl+\\ Backspace asks before closing."};
    static int upper;
    static tui_window *closing, *renaming;
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
        renaming = w;
    tui_menu_end(tt);
    if (renaming == w)
    {
        /* asks with a modal input box; 0 while it is up */
        static char title[32];
        unsigned r;
        if (!title[0])
            snprintf(title, sizeof(title), "%s", w->title);
        r = tui_input_box(tt, "Rename", "New ~t~itle for this note:", title, sizeof(title));
        if (r == TUI_MB_OK)
        {
            tui_window_set_title(tt, w, title);
            log_msg("Renamed a note");
        }
        if (r)
        {
            renaming = 0;
            title[0] = '\0';
        }
    }
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

/* the text editor widget, filling a window */
static char edit_buf[16 * 1024];
static tui_edit ed;
static char clip_buf[4096];
static tui_clipboard clip;

static void editor_ui(tui *tt, tui_window *w, void *user)
{
    char status[128];
    int cw, ch;
    uint32_t from, to;
    (void)w;
    (void)user;
    file_menu_for(tt, 1);
    tui_menu_begin(tt, "~E~dit");
    /* Emacs notation for the keys: shown, but not bound as menu shortcuts,
       so the keys stay with the editor */
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
    if (tui_menu_item_flags(tt, "Tab inserts ~s~paces", 0, ed.flags & TUI_EDIT_SPACES ? TUI_ITEM_CHECKED : 0))
        ed.flags ^= TUI_EDIT_SPACES;
    if (tui_menu_item_flags(tt, "~R~ead only", 0, ed.flags & TUI_EDIT_READ_ONLY ? TUI_ITEM_CHECKED : 0))
        ed.flags ^= TUI_EDIT_READ_ONLY;
    tui_menu_end(tt);
    help_menu(tt);

    tui_size(tt, &cw, &ch);
    tui_at(tt, 0, 0);
    tui_editor(tt, "##editor", &ed, cw, ch);
    if (tui_edit_selection(&ed, &from, &to))
        snprintf(status, sizeof(status), " Line %u  Col %u  %u bytes selected  ~C-w~ Cut  ~Esc w~ Copy  ~C-g~ Clear",
                 (unsigned)ed.line + 1, (unsigned)tui_edit_column(&ed) + 1, (unsigned)(to - from));
    else
        snprintf(status, sizeof(status),
                 " Line %u  Col %u  %s%u free  ~C-SPC~ Mark  ~C-y~ Paste  ~F4~ Numbers  ~Ctrl+\\~ Commands",
                 (unsigned)ed.line + 1, (unsigned)tui_edit_column(&ed) + 1, ed.modified ? "Modified  " : "",
                 (unsigned)(ed.cap - tui_edit_len(&ed)));
    tui_status(tt, status);
    ask_quit(tt);
}

/* every widget, in a dialog-coloured window */
static void prefs_ui(tui *tt, tui_window *w, void *user)
{
    static char name[24] = "Chestnut";
    static int wrap = 1, numbers, indent, scheme;
    static const char *const schemes[] = {"Borland blue", "Midnight", "Amber terminal", "Green screen",
                                          "Paper", "Solarized-ish", "High contrast", "Chestnut"};
    static const char *const notes[] = {
        "Tab / Shift+Tab move between widgets.",
        "Space toggles, Enter presses buttons.",
        "Hot letters: alone, or with Alt when a",
        "text box or list has focus.",
        "The name box knows Emacs keys: Ctrl+A,",
        "Ctrl+E, Ctrl+B, Ctrl+F, Ctrl+D, Ctrl+K.",
        "Lists jump to the first letter you type.",
        "This box scrolls: focus it, then use the",
        "arrows, PgUp / PgDn, Home / End.",
        "Everything is immediate mode: this whole",
        "window is one function, prefs_ui().",
    };
    const tui_event *ev;
    char line[64];
    (void)user;
    file_menu(tt);
    help_menu(tt);
    tui_status(tt, " ~Tab~ Next  ~Shift+Tab~ Back  ~Space~ Toggle  ~Enter~ Press  ~Esc~ Close");

    tui_at(tt, 2, 1);
    tui_label(tt, "~N~ame:");
    tui_same_line(tt);
    if (tui_input(tt, "~N~ame##name", name, sizeof(name), 22) & TUI_SUBMIT)
        log_msg("Name entered");
    tui_at(tt, 2, 3);
    if (tui_checkbox(tt, "~W~rap long lines", &wrap))
        log_msg(wrap ? "Wrap on" : "Wrap off");
    if (tui_checkbox(tt, "Line n~u~mbers", &numbers))
        log_msg(numbers ? "Line numbers on" : "Line numbers off");
    tui_space(tt, 1);
    tui_label(tt, "Indent with:");
    tui_radio(tt, "~S~paces", &indent, 0);
    tui_same_line(tt);
    tui_radio(tt, "~T~abs", &indent, 1);

    tui_at(tt, 34, 1);
    tui_label(tt, "~C~olour scheme:");
    tui_at(tt, 34, 2);
    if (tui_list(tt, "~C~olour scheme##list", schemes, 8, &scheme, 20, 6) & TUI_CHANGED)
    {
        snprintf(line, sizeof(line), "Scheme: %s", schemes[scheme]);
        log_msg(line);
    }

    tui_at(tt, 2, 9);
    tui_label(tt, "~H~ow these work:");
    tui_scroll_begin(tt, "~H~ow##view", 52, 4, 11);
    for (int i = 0; i < 11; i++)
        tui_text(tt, 1, i, s8_from_cstr(notes[i]), TUI_DEFAULT);
    tui_scroll_end(tt);

    tui_at(tt, 16, 15);
    if (tui_button(tt, "~O~K"))
    {
        log_msg("Preferences saved");
        tui_window_close(tt, w);
    }
    tui_same_line(tt);
    if (tui_button(tt, "~A~pply"))
        log_msg("Preferences applied");
    tui_same_line(tt);
    if (tui_button(tt, "Cancel"))
        tui_window_close(tt, w);
    ev = tui_key(tt);
    if (ev && ev->key == TUI_KEY_ESC)
    {
        tui_use_key(tt);
        tui_window_close(tt, w);
    }
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
    tui_edit_init(&ed, edit_buf, sizeof(edit_buf));
    tui_clipboard_init(&clip, clip_buf, sizeof(clip_buf));
    ed.clip = &clip;
    ed.flags = TUI_EDIT_LINE_NUMBERS | TUI_EDIT_AUTO_INDENT;
    tui_edit_set(&ed, S8("r2_tui_edit: a text editor widget\n"
                         "\n"
                         "Arrows, Home/End, PgUp/PgDn, Ctrl+Home/End move around;\n"
                         "Ctrl+Left/Right jump words. Emacs keys work too:\n"
                         "\tCtrl+A / Ctrl+E\tstart / end of the line\n"
                         "\tCtrl+B / Ctrl+F\tback / forward\n"
                         "\tCtrl+P / Ctrl+N\tup / down\n"
                         "\tCtrl+D\t\tdelete\n"
                         "\tCtrl+K\t\tkill to the end of the line (or join)\n"
                         "\n"
                         "Select: Ctrl+Space sets the mark, then move (or Shift+arrows).\n"
                         "\tCtrl+W cut, Esc w copy, Ctrl+Y paste; Ctrl+G / Esc Esc clear.\n"
                         "\tKills in a row join up: Ctrl+K Ctrl+K takes the line and its break.\n"
                         "Esc then a key is Meta: Esc f / Esc b words, Esc < / Esc > top / bottom.\n"
                         "\n"
                         "Tab types a tab; Shift+Tab leaves the editor. Edit > Line numbers (F4)\n"
                         "toggles the gutter; Edit > Auto indent keeps your indentation on Enter.\n"
                         "\n"
                         "Wide text is fine: \xE4\xB8\xAD\xE6\x96\x87, \xE6\xBC\xA2\xE5\xAD\x97, caf\xC3\xA9, \xF0\x9F\x90\xB1.\n"
                         "This line is long on purpose, so that you can see the editor scroll sideways when the cursor goes past the right edge of the window.\n"));
    open_editor();

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
