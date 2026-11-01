/* r2_tui - v0.1 - public domain text user interface for terminals
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    Written for Chestnut (a cooperative OS on the Raspberry Pi and ESP32)
    but it runs anywhere there is a terminal: over a serial line, in a Mac
    or Linux terminal, or on a device console with a VT100 emulator.

    No allocation, no libc beyond <stdint.h>/<stddef.h>, never blocks:
    you feed it the bytes that arrive from the keyboard, and pull from it
    the bytes to send to the terminal, in chunks as small as you like.

    Do this:
       #define R2_TUI_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the
    implementation. It pulls in r2_strings.h (and its implementation) and
    r2_termui.h, which must sit next to it.

OVERVIEW
    The screen is a grid of cells. You draw into a back buffer; the
    renderer compares it with what the terminal shows (the front buffer)
    and sends only the changed cells.

        static uint8_t arena[R2_TUI_ARENA_SIZE(132, 50)];
        tui t;
        tui_init(&t, arena, sizeof(arena), 80, 25);

        tui_draw_fill(&t, (tui_rect){0, 0, 80, 25}, ' ', TUI_STYLE(TUI_WHITE, TUI_BLUE, 0));
        tui_draw_text(&t, 2, 1, S8("Hello"), TUI_STYLE(TUI_YELLOW, TUI_BLUE, TUI_BOLD));

        uint8_t out[256];
        size_t n;
        while ((n = tui_render(&t, out, sizeof(out))) > 0)
            uart_write(out, n);         // or yield between chunks

    Input goes the other way:

        tui_feed(&t, bytes, count, now_ms);
        tui_step(&t, now_ms);           // call when idle too: a lone ESC
                                        // becomes the Esc key after
                                        // R2_TUI_ESC_MS
        tui_event ev;
        while (tui_poll(&t, &ev)) { ... }

    Text is UTF-8 (an s8 from r2_strings.h). Wide characters (CJK, most
    emoji) take two cells, combining marks attach to the previous cell,
    and joiners / variation selectors / bidi controls are dropped so the
    terminal cannot merge or reorder glyphs behind our back. Control
    characters are drawn as '?', so text from a file can never inject an
    escape sequence.

CHANGES
    v0.4  immediate-mode widgets (label, button, checkbox, radio, text input,
          list, scroll view) with Tab focus and hot letters; tui_input_box
    v0.7  windows settle after every key, so keys that arrive together (serial,
          paste, typing ahead) behave like keys typed one at a time
    v0.6  after handling a key or a menu choice a window runs once more, so
          menus and status declared before the change show the new state
    v0.5  the command palette moves to Ctrl+] (Ctrl+Space is set-mark in the editor)
    v0.3  window command prefix (Ctrl+\\ then a key), minimize, and a fuzzy
          command palette over menus, window commands and windows
    v0.2  window manager: overlapping and docked windows, z-order, focus,
          the menu bar following focus, system menu, move / size / zoom,
          status line, message boxes; immediate-mode window contents
    v0.1  core: cells, width table, drawing, diff renderer, input parser

LICENSE
    See end of file for license information.
*/
#ifndef R2_TUI_H
#define R2_TUI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(R2_TUI_IMPLEMENTATION) && !defined(R2_STRINGS_IMPLEMENTATION)
#define R2_STRINGS_IMPLEMENTATION
#endif
#include "r2_strings.h"
#include "r2_termui.h"

/* ---------------- configuration ---------------- */

/* combining marks the back buffer can hold (the front holds twice this) */
#ifndef R2_TUI_MAX_COMBINING
#define R2_TUI_MAX_COMBINING 64
#endif

/* keyboard events waiting for tui_poll */
#ifndef R2_TUI_EVENT_QUEUE
#define R2_TUI_EVENT_QUEUE 32
#endif

/* how long a lone ESC waits for the rest of a sequence; raise it for slow
   serial links (or set t->esc_ms at run time) */
#ifndef R2_TUI_ESC_MS
#define R2_TUI_ESC_MS 50
#endif

/* tui_render needs at least this much room to make progress */
#define R2_TUI_RENDER_MIN 64

#if defined(__GNUC__) || defined(__clang__)
#define R2_TUI_DEF static __attribute__((unused))
#else
#define R2_TUI_DEF static
#endif

/* ---------------- types ---------------- */

    /* the 16 ANSI colours */
    enum
    {
        TUI_BLACK, TUI_RED, TUI_GREEN, TUI_YELLOW, TUI_BLUE, TUI_MAGENTA, TUI_CYAN, TUI_WHITE,
        TUI_DARK_GRAY, TUI_BRIGHT_RED, TUI_BRIGHT_GREEN, TUI_BRIGHT_YELLOW,
        TUI_BRIGHT_BLUE, TUI_BRIGHT_MAGENTA, TUI_BRIGHT_CYAN, TUI_BRIGHT_WHITE
    };

    /* attributes */
#define TUI_BOLD 1u
#define TUI_UNDERLINE 2u
#define TUI_REVERSE 4u

    /* fg (4 bits) | bg (4 bits) | attributes (3 bits) */
    typedef uint16_t tui_style;
#define TUI_STYLE(fg, bg, attr) ((tui_style)(((fg) & 15u) | (((bg) & 15u) << 4) | (((attr) & 7u) << 8)))
#define TUI_STYLE_FG(s) ((s) & 15u)
#define TUI_STYLE_BG(s) (((s) >> 4) & 15u)
#define TUI_STYLE_ATTR(s) (((s) >> 8) & 7u)

    /* rune (21 bits) | style (11 bits) */
    typedef uint32_t tui_cell;
#define TUI_CELL(r, style) ((tui_cell)(((r) & 0x1FFFFFu) | ((uint32_t)(style) << 21)))
#define TUI_CELL_RUNE(c) ((c) & 0x1FFFFFu)
#define TUI_CELL_STYLE(c) ((tui_style)((c) >> 21))
    /* the right half of a wide character */
#define TUI_RUNE_CONT 0x1FFFFFu

    typedef struct tui_rect
    {
        int x, y, w, h;
    } tui_rect;

    typedef struct tui_comb
    {
        uint32_t index; /* cell index */
        rune mark;
    } tui_comb;

    enum
    {
        TUI_EV_NONE,
        TUI_EV_KEY,
        TUI_EV_RESIZE /* reply to tui_query_size: x = columns, y = rows */
    };

    enum
    {
        TUI_KEY_NONE,
        TUI_KEY_RUNE, /* a character: ev.ch (with TUI_MOD_CTRL for Ctrl+letter) */
        TUI_KEY_ENTER,
        TUI_KEY_TAB,
        TUI_KEY_BACKSPACE,
        TUI_KEY_ESC,
        TUI_KEY_UP,
        TUI_KEY_DOWN,
        TUI_KEY_LEFT,
        TUI_KEY_RIGHT,
        TUI_KEY_HOME,
        TUI_KEY_END,
        TUI_KEY_PGUP,
        TUI_KEY_PGDN,
        TUI_KEY_INSERT,
        TUI_KEY_DELETE,
        TUI_KEY_F1,
        TUI_KEY_F12 = TUI_KEY_F1 + 11
    };

#define TUI_MOD_SHIFT 1u
#define TUI_MOD_ALT 2u
#define TUI_MOD_CTRL 4u

    typedef struct tui_event
    {
        uint8_t type; /* TUI_EV_* */
        uint8_t mods; /* TUI_MOD_* */
        uint16_t key; /* TUI_KEY_* */
        rune ch;      /* for TUI_KEY_RUNE */
        int x, y;     /* for TUI_EV_RESIZE */
    } tui_event;

    /* ---------------- window manager types ---------------- */

/* user windows open at once (two more slots are used internally: the
   desktop and the message box) */
#ifndef R2_TUI_MAX_WINDOWS
#define R2_TUI_MAX_WINDOWS 8
#endif
/* bytes shared by every window's recorded drawing commands */
#ifndef R2_TUI_CMD_POOL
#define R2_TUI_CMD_POOL 6144
#endif
#ifndef R2_TUI_TITLE_MAX
#define R2_TUI_TITLE_MAX 32
#endif
#ifndef R2_TUI_STATUS_MAX
#define R2_TUI_STATUS_MAX 96
#endif
/* per menu bar (the focused window's, and the desktop's) */
#ifndef R2_TUI_MENU_MAX
#define R2_TUI_MENU_MAX 8
#endif
#ifndef R2_TUI_MENU_ITEMS
#define R2_TUI_MENU_ITEMS 48
#endif
#ifndef R2_TUI_MENU_TEXT
#define R2_TUI_MENU_TEXT 768
#endif
#ifndef R2_TUI_MESSAGE_MAX
#define R2_TUI_MESSAGE_MAX 256
#endif
/* the window command prefix (press it, let go, then a key); see tui_set_prefix */
#ifndef R2_TUI_PREFIX
#define R2_TUI_PREFIX "Ctrl+\\"
#endif
/* how long the prefix waits for its key */
#ifndef R2_TUI_PREFIX_MS
#define R2_TUI_PREFIX_MS 2000
#endif
/* focusable widgets one window can have (more still draw, but Tab skips them) */
#ifndef R2_TUI_MAX_WIDGETS
#define R2_TUI_MAX_WIDGETS 64
#endif
/* widget state kept between runs (text cursors, scroll positions), least recently used goes */
#ifndef R2_TUI_STATE_SLOTS
#define R2_TUI_STATE_SLOTS 32
#endif
/* longest text tui_input_box edits */
#ifndef R2_TUI_INPUT_MAX
#define R2_TUI_INPUT_MAX 128
#endif
/* the key that opens the command palette; see tui_set_palette_key */
#ifndef R2_TUI_PALETTE_KEY
/* Ctrl+] because Ctrl+Space is Emacs set-mark (the editor's), and Ctrl+] is
   one byte (0x1D) in every terminal */
#define R2_TUI_PALETTE_KEY "Ctrl+]"
#endif
/* commands the palette can list at once */
#ifndef R2_TUI_PALETTE_MAX
#define R2_TUI_PALETTE_MAX 128
#endif

/* the window's own colour (theme body, or dialog body for dialogs) */
#define TUI_DEFAULT ((tui_style)0xFFFFu)

/* window flags */
#define TUI_WIN_DIALOG 1u    /* dialog colours, opened centred */
#define TUI_WIN_MODAL 2u     /* takes all input until it closes */
#define TUI_WIN_NO_SHADOW 4u
#define TUI_WIN_NO_RESIZE 8u
#define TUI_WIN_ASK_CLOSE 16u /* Alt+F3 sets tui_closing() instead of closing */
#define TUI_WIN_NO_CLOSE 32u

    enum
    {
        TUI_DOCK_NONE,
        TUI_DOCK_LEFT,
        TUI_DOCK_RIGHT,
        TUI_DOCK_BOTTOM
    };

/* menu item flags */
#define TUI_ITEM_DISABLED 1u
#define TUI_ITEM_CHECKED 2u

/* message box buttons / results */
#define TUI_MB_OK 1u
#define TUI_MB_YES 2u
#define TUI_MB_NO 4u
#define TUI_MB_CANCEL 8u

    typedef struct tui tui;
    typedef struct tui_window tui_window;
    /** A window's controller: draws the contents and declares menus, every time it runs. */
    typedef void (*tui_ui_fn)(tui *t, tui_window *w, void *user);

    struct tui_window
    {
        uint8_t used, number, dock, dirty, closing, dead, overflow, zoomed, hidden;
        uint16_t flags;
        int dock_size;
        tui_rect rect;  /* floating position, frame included */
        tui_rect saved; /* restore point for zoom and move mode */
        tui_rect frame; /* where it is on screen now */
        tui_ui_fn ui;
        void *user;
        uint32_t cmd_off, cmd_len;
        uint32_t runs; /* times the controller ran */
        uint32_t focus_id;  /* the focused widget */
        uint8_t focus_kind; /* and what kind it is */
        char title[R2_TUI_TITLE_MAX];
        char status[R2_TUI_STATUS_MAX];
    };

    typedef struct tui_theme
    {
        tui_style desktop;
        rune desktop_rune;
        tui_style menu, menu_hot, menu_sel, menu_sel_hot, menu_disabled;
        tui_style frame, frame_active, frame_move;
        tui_style body;
        tui_style dialog_frame, dialog_body;
        tui_style status, status_hot;
        tui_style button, button_focus, button_hot;
        /* widgets; for hot and focus only the foreground is used (on the window's background) */
        tui_style hot, focus;
        tui_style input, input_focus;
        tui_style list, list_sel, list_sel_focus;
        tui_style scrollbar;
    } tui_theme;

    typedef struct tui_menu_rec
    {
        uint16_t title; /* offset into text */
        uint8_t first, count;
    } tui_menu_rec;

    typedef struct tui_item_rec
    {
        uint16_t label; /* offset into text, 0xFFFF for a separator */
        uint16_t shortcut;
        uint16_t key;
        uint8_t flags, mods;
        rune ch;
    } tui_item_rec;

    typedef struct tui_menubar
    {
        tui_menu_rec menu[R2_TUI_MENU_MAX];
        tui_item_rec item[R2_TUI_MENU_ITEMS];
        char text[R2_TUI_MENU_TEXT];
        uint16_t ntext;
        uint8_t nmenus, nitems;
        int8_t open;  /* menu being declared, -1 none */
        uint8_t full; /* ran out of room: record nothing more this run */
    } tui_menubar;

    /* state a widget keeps between runs */
    typedef struct tui_wstate
    {
        uint32_t id;
        int32_t a, b;
        uint32_t touched;
    } tui_wstate;

    /* widget results */
#define TUI_CHANGED 1u
#define TUI_SUBMIT 2u

    /* one command palette result */
    typedef struct tui_pal_entry
    {
        uint8_t kind;
        uint8_t spare;
        uint16_t index;
        int16_t score;
    } tui_pal_entry;

#define R2__TUI_SLOTS (R2_TUI_MAX_WINDOWS + 2)
#if R2__TUI_SLOTS > 32
#error "R2_TUI_MAX_WINDOWS can be at most 30"
#endif

    struct tui
    {
        int cols, rows;
        uint32_t cap_cells;
        tui_cell *back, *front;
        uint8_t *back_has, *front_has; /* bitmaps: cell has a combining mark */
        tui_comb *back_comb, *front_comb;
        uint32_t back_ncomb, front_ncomb;
        tui_rect clip;

        /* where the application wants the cursor */
        int cur_x, cur_y, cur_visible;

        /* renderer */
        uint8_t dirty;    /* something changed since the last frame began */
        uint8_t full;     /* front buffer unknown: repaint everything */
        uint8_t in_frame; /* a frame is part way out */
        uint8_t stage;
        uint8_t opened;   /* this frame's prefix (sync, hide cursor) is out */
        int8_t shown_vis; /* cursor as the terminal shows it; -1 unknown */
        int shown_x, shown_y;
        uint8_t sync;     /* wrap frames in synchronized update (?2026) */
        uint32_t scan;
        int tx, ty;       /* terminal cursor, -1 when unknown */
        int32_t tstyle;   /* terminal SGR state, -1 when unknown */

        /* input */
        uint8_t in_buf[16];
        uint8_t in_len;
        uint8_t cpr_pending;
        uint32_t in_time;
        uint32_t esc_ms;
        tui_event queue[R2_TUI_EVENT_QUEUE];
        uint32_t q_head, q_len;

        /* window manager (slot 0 is the desktop, slot 1 the message box) */
        tui_window *win;
        uint8_t *cmd_pool;
        uint32_t cmd_used;
        tui_menubar *bar_win, *bar_desk, *rec_bar;
        const tui_theme *theme;
        uint8_t z[R2__TUI_SLOTS]; /* floating windows, bottom to top */
        uint8_t nz;
        int focus;                /* slot, or -1 */
        int cur;                  /* slot whose controller is running, or -1 */
        const tui_event *cur_ev;
        uint8_t ev_used;
        int menu_act;             /* item to report as chosen in this run, or -1 */
        int item_seq;             /* items declared so far in this run */
        uint8_t item_seq_on;      /* inside tui_menu_begin / end */
        uint8_t bar_stale;        /* focus moved during a run: clear bar_win after it */
        uint8_t menu_open, moving, layout_dirty, compose_dirty;
        int menu_idx, menu_sel;
        int menu_x[R2_TUI_MENU_MAX + 1];
        tui_rect move_saved;
        int move_saved_dock;
        tui_rect desk, work;
        /* message box */
        uint8_t mb_state, mb_buttons, mb_focus;
        unsigned mb_result;
        int mb_owner;
        uint32_t mb_waiting; /* slots told "busy" by tui_message, to re-run when it is free */
        /* the command prefix */
        uint16_t prefix_key;
        uint8_t prefix_mods, chord;
        rune prefix_ch;
        uint32_t chord_time, now;
        char prefix_name[16];
        /* the command palette */
        uint8_t pal_open, pal_mods;
        uint16_t pal_key;
        rune pal_ch;
        char pal_name[16];
        char pal_query[64];
        int pal_n, pal_sel, pal_top;
        tui_pal_entry pal[R2_TUI_PALETTE_MAX];
        /* widgets: the focusable ones seen in this run, layout cursor, scroll view */
        uint32_t wid[R2_TUI_MAX_WIDGETS];
        uint8_t wkind[R2_TUI_MAX_WIDGETS];
        int nwid;
        int lay_x, lay_y, lay_lx, lay_ly, lay_lw, lay_same;
        int org_x, org_y;
        int sv_on, sv_x, sv_y, sv_w, sv_h, sv_content, sv_focused, sv_lx, sv_ly;
        int32_t sv_scroll;
        tui_wstate wstate[R2_TUI_STATE_SLOTS];
        uint32_t frame;
        uint8_t mb_kind, ib_fresh;
        char mb_edit[R2_TUI_INPUT_MAX];
        uint32_t mb_hash;
        char mb_text[R2_TUI_MESSAGE_MAX];
    };

/* bytes of arena tui_init needs for a screen of up to cols x rows */
#define R2__TUI_ALIGN(n) (((size_t)(n) + 7u) & ~(size_t)7u)
#define R2__TUI_FIXED                                                                                \
    (R2__TUI_ALIGN(sizeof(tui_comb) * R2_TUI_MAX_COMBINING) * 3u +                                   \
     R2__TUI_ALIGN(sizeof(tui_window) * R2__TUI_SLOTS) + R2__TUI_ALIGN(R2_TUI_CMD_POOL) +           \
     R2__TUI_ALIGN(sizeof(tui_menubar)) * 2u + 8u)
#define R2_TUI_ARENA_SIZE(cols, rows)                                                               \
    (R2__TUI_ALIGN((size_t)(cols) * (size_t)(rows) * 4u) * 2u +                                    \
     R2__TUI_ALIGN(((size_t)(cols) * (size_t)(rows) + 7u) / 8u) * 2u + R2__TUI_FIXED)

    /** Set up t over arena (see R2_TUI_ARENA_SIZE). 0 on success, -1 if the arena is too small. */
    R2_TUI_DEF int tui_init(tui *t, void *arena, size_t size, int cols, int rows);
    /** Change the screen size (up to what the arena holds). Clears the screen. 0 on success. */
    R2_TUI_DEF int tui_resize(tui *t, int cols, int rows);
    /** Forget what the terminal shows; the next frame repaints everything. */
    R2_TUI_DEF void tui_invalidate(tui *t);

    /** Columns a rune takes: 0 (combining), 1 or 2 (wide). */
    R2_TUI_DEF int tui_rune_width(rune r);
    /** Columns a string takes. */
    R2_TUI_DEF int tui_text_width(s8 str);

    /** Limit drawing to r (intersected with the screen). */
    R2_TUI_DEF void tui_clip(tui *t, tui_rect r);
    R2_TUI_DEF void tui_clip_reset(tui *t);
    /** Draw one rune; returns the columns it takes (0, 1 or 2). */
    R2_TUI_DEF int tui_draw_rune(tui *t, int x, int y, rune r, tui_style s);
    /** Draw text on one line; returns the columns it takes (stops at the clip edge). */
    R2_TUI_DEF int tui_draw_text(tui *t, int x, int y, s8 str, tui_style s);
    R2_TUI_DEF void tui_draw_fill(tui *t, tui_rect r, rune ch, tui_style s);
    /** Box outline: TUI_BOX_SINGLE or TUI_BOX_DOUBLE. */
    R2_TUI_DEF void tui_draw_box(tui *t, tui_rect r, tui_style s, int kind);
    /** Darken the cells in r (keeps the characters), for window shadows. */
    R2_TUI_DEF void tui_draw_shadow(tui *t, tui_rect r);
    /** Where the cursor should sit after a frame (visible 0 hides it). */
    R2_TUI_DEF void tui_set_cursor(tui *t, int x, int y, int visible);
    /** The cell at x, y of the back buffer (0 outside the screen). */
    R2_TUI_DEF tui_cell tui_get_cell(const tui *t, int x, int y);
    /** The combining mark on cell x, y of the back buffer, or 0. */
    R2_TUI_DEF rune tui_get_mark(const tui *t, int x, int y);

#define TUI_BOX_SINGLE 0
#define TUI_BOX_DOUBLE 1

    /**
     * Write the next part of the frame into out (at most cap bytes, cap
     * >= R2_TUI_RENDER_MIN). Returns the bytes written; 0 when the
     * terminal is up to date. Resumable: call until it returns 0, sending
     * each chunk (and yielding) in between. Drawing in between is fine.
     */
    R2_TUI_DEF size_t tui_render(tui *t, uint8_t *out, size_t cap);
    /** 1 while tui_render has more to send. */
    R2_TUI_DEF int tui_render_pending(const tui *t);

    /** Feed bytes from the keyboard / serial line. */
    R2_TUI_DEF void tui_feed(tui *t, const uint8_t *bytes, size_t n, uint32_t now_ms);
    /** Let time pass: turns a lone ESC into the Esc key after t->esc_ms. */
    R2_TUI_DEF void tui_step(tui *t, uint32_t now_ms);
    /** Next event, 1 if there was one. */
    R2_TUI_DEF int tui_poll(tui *t, tui_event *ev);
    /** 1 while input is waiting on a timeout (call tui_step soon). */
    R2_TUI_DEF int tui_input_pending(const tui *t);
    /**
     * Write a request for the terminal's size into out (cap >= 32); the
     * reply arrives as a TUI_EV_RESIZE event. For serial lines where there
     * is no ioctl. Returns the bytes written.
     */
    R2_TUI_DEF size_t tui_query_size(tui *t, uint8_t *out, size_t cap);
    /** Human readable key name ("Ctrl+Alt+F5", "Shift+Tab", "a") into buf. */
    R2_TUI_DEF void tui_key_name(const tui_event *ev, char *buf, size_t cap);

    /* ---------------- window manager ----------------

    Windows are retained (position, z-order, focus, docking); what is in
    them is immediate mode. Each window has a controller that draws its
    contents and declares its menus every time it runs. A controller runs
    when its window needs it: a key for it, a menu choice, focus or size
    changing, or tui_window_invalidate(). Moving windows, opening menus
    and so on re-composite the recorded drawing without running anything.

        static void notes(tui *t, tui_window *w, void *user)
        {
            tui_menu_begin(t, "~F~ile");
            if (tui_menu_item(t, "~S~ave", "Ctrl+S")) save();
            tui_menu_end(t);
            tui_text(t, 1, 1, S8("Hello"), TUI_DEFAULT);
        }

        tui_window_open(&t, "Notes", (tui_rect){4, 3, 40, 12}, 0, notes, 0);
        for (;;) {
            tui_feed(&t, bytes, n, now);
            tui_update(&t, now);
            while ((n = tui_render(&t, out, sizeof(out))) > 0) send(out, n);
        }

    The menu bar shows the focused window's menus (or the desktop's when
    it declares none, see tui_set_desktop), after the system menu (≡).
    Keys: everything is behind the command prefix, Ctrl+\\ by default (see
    tui_set_prefix): Ctrl+\\ then F opens the File menu, Ctrl+\\ then 2
    focuses window 2. The Borland keys work too where the terminal sends
    them: F10 or Alt+letter open the menus, Alt+Space the system menu,
    F6 / Shift+F6 cycle windows, Alt+1..9 jump to one, F5 zooms, Ctrl+F5
    moves (arrows) and sizes (Shift+arrows), Alt+F3 closes.
    ~ marks hot letters in titles and labels: "~F~ile". */

    /** Handle input, run the controllers that need it, redraw. 1 if the screen changed. */
    R2_TUI_DEF int tui_update(tui *t, uint32_t now_ms);
    /**
     * Open a window. A rect with w or h <= 0 gets a default size; x < 0 places it
     * automatically (dialogs are centred). Returns 0 when all windows are in use.
     */
    R2_TUI_DEF tui_window *tui_window_open(tui *t, const char *title, tui_rect r, unsigned flags, tui_ui_fn ui,
                                           void *user);
    R2_TUI_DEF void tui_window_close(tui *t, tui_window *w);
    /** Dock to a side (TUI_DOCK_LEFT/RIGHT/BOTTOM, size in cells), or TUI_DOCK_NONE to float. */
    R2_TUI_DEF void tui_window_dock(tui *t, tui_window *w, int side, int size);
    R2_TUI_DEF void tui_window_focus(tui *t, tui_window *w);
    /** Run the window's controller again (its data changed). */
    R2_TUI_DEF void tui_window_invalidate(tui *t, tui_window *w);
    R2_TUI_DEF void tui_window_set_title(tui *t, tui_window *w, const char *title);
    R2_TUI_DEF tui_window *tui_focused_window(tui *t);
    /** Controller for the desktop: its menus show when the focused window has none. */
    R2_TUI_DEF void tui_set_desktop(tui *t, tui_ui_fn ui, void *user);
    R2_TUI_DEF void tui_set_theme(tui *t, const tui_theme *theme);
    /**
     * The window command prefix, as a shortcut ("Ctrl+\\", "Ctrl+]"), or 0 to
     * turn it off. Press it, let go, then: a letter opens that menu, 1-9 a
     * window, Space the system menu, Up maximizes, Down restores or
     * minimizes, Left/Right/Tab switch windows, Enter moves and sizes,
     * Backspace closes, the prefix again sends it to the window. 0 if the
     * shortcut was not understood.
     */
    R2_TUI_DEF int tui_set_prefix(tui *t, const char *shortcut);
    /**
     * The key that opens the command palette ("Ctrl+]" by default), or 0
     * for none (the prefix then / still opens it). The palette fuzzy-searches
     * every menu item, the window commands and the windows themselves.
     */
    R2_TUI_DEF int tui_set_palette_key(tui *t, const char *shortcut);
    /** Open the command palette (e.g. from a menu item). */
    R2_TUI_DEF void tui_palette(tui *t);
    /** The desktop area left for floating windows (inside the docks). */
    R2_TUI_DEF tui_rect tui_work_area(const tui *t);

    /* ---- inside a controller (coordinates are relative to the window's inside) ---- */

    R2_TUI_DEF void tui_text(tui *t, int x, int y, s8 str, tui_style s);
    R2_TUI_DEF void tui_fill(tui *t, tui_rect r, rune ch, tui_style s);
    R2_TUI_DEF void tui_box(tui *t, tui_rect r, tui_style s, int kind);
    /** Show the cursor here while this window has focus. */
    R2_TUI_DEF void tui_cursor(tui *t, int x, int y);
    /** Size of the inside of the window. */
    R2_TUI_DEF void tui_size(tui *t, int *w, int *h);
    /** The key this run is for, or 0. Call tui_use_key when you handled it. */
    R2_TUI_DEF const tui_event *tui_key(tui *t);
    R2_TUI_DEF void tui_use_key(tui *t);
    /** 1 if this window has focus. */
    R2_TUI_DEF int tui_focused(tui *t);
    /** Status line text while this window has focus (~ marks hot keys). */
    R2_TUI_DEF void tui_status(tui *t, const char *text);
    /** 1 once when the user asked to close a TUI_WIN_ASK_CLOSE window. */
    R2_TUI_DEF int tui_closing(tui *t);

    R2_TUI_DEF void tui_menu_begin(tui *t, const char *title);
    R2_TUI_DEF void tui_menu_end(tui *t);
    /** 1 when chosen (from the menu or by its shortcut, e.g. "Ctrl+S", "F3", "Alt+X"). */
    R2_TUI_DEF int tui_menu_item(tui *t, const char *label, const char *shortcut);
    R2_TUI_DEF int tui_menu_item_flags(tui *t, const char *label, const char *shortcut, unsigned flags);
    R2_TUI_DEF void tui_menu_separator(tui *t);
    /**
     * Ask with a modal message box (buttons: TUI_MB_OK | YES | NO | CANCEL).
     * Call it on every run while you want the answer: it returns 0 while
     * the box is up and the chosen button once. Text wraps; '\n' breaks.
     */
    R2_TUI_DEF unsigned tui_message(tui *t, const char *title, const char *text, unsigned buttons);
    /**
     * Ask for a line of text in a modal box. Like tui_message: call it on
     * every run while you want the answer; 0 while the box is up, then
     * TUI_MB_OK (buf now holds the text) or TUI_MB_CANCEL (buf untouched).
     */
    R2_TUI_DEF unsigned tui_input_box(tui *t, const char *title, const char *prompt, char *buf, size_t cap);

    /* ---- widgets (inside a controller) ----

    Immediate mode: call them every run, they draw themselves and report
    what happened. They are placed by a layout cursor: tui_at() moves it,
    each widget goes below the last one, tui_same_line() puts the next one
    beside it. Tab / Shift+Tab move focus in call order. ~ marks the hot
    letter: alone it works unless the focused widget takes letters (text
    input, list); with Alt it always works. Labels must be unique within a
    window, or add "##something" (not shown) to tell them apart. */

    R2_TUI_DEF void tui_at(tui *t, int x, int y);
    R2_TUI_DEF void tui_same_line(tui *t);
    R2_TUI_DEF void tui_space(tui *t, int lines);
    R2_TUI_DEF void tui_label(tui *t, const char *text);
    /** 1 when pressed (Enter or Space when focused, or its hot letter). */
    R2_TUI_DEF int tui_button(tui *t, const char *label);
    /** 1 when toggled (Space, or its hot letter). */
    R2_TUI_DEF int tui_checkbox(tui *t, const char *label, int *value);
    /** Sets *value to option when chosen; 1 then. Radios sharing value form a group. */
    R2_TUI_DEF int tui_radio(tui *t, const char *label, int *value, int option);
    /**
     * One line of editable UTF-8 text in buf (cap bytes, NUL terminated), w
     * columns wide. Arrows, Home/End, Backspace/Delete, and Emacs keys:
     * Ctrl+A/E/B/F/D/K. Returns TUI_CHANGED and/or TUI_SUBMIT (Enter).
     */
    R2_TUI_DEF unsigned tui_input(tui *t, const char *label, char *buf, size_t cap, int w);
    /**
     * A w x h list of count items; *selected moves with Up/Down/PgUp/PgDn/
     * Home/End or the first letter. Returns TUI_CHANGED and/or TUI_SUBMIT.
     */
    R2_TUI_DEF unsigned tui_list(tui *t, const char *label, const char *const *items, int count, int *selected, int w,
                                 int h);
    /**
     * A w x h view onto content_h lines; draw the content between begin and
     * end at y 0..content_h (it is clipped and scrolled). Focus it to scroll
     * with the arrows and PgUp/PgDn. Views don't nest.
     */
    R2_TUI_DEF void tui_scroll_begin(tui *t, const char *label, int w, int h, int content_h);
    R2_TUI_DEF void tui_scroll_end(tui *t);

/* ================================================================== */
#ifdef R2_TUI_IMPLEMENTATION

/* BEGIN GENERATED WIDTH TABLE: tools/gen_tui_width.py, Unicode 16.0.0 */
#define R2_TUI_WIDTH_UNICODE "16.0.0"
#define R2_TUI_WIDTH_CHECKSUM 0x64126B0Bu
    static const uint32_t r2__tui_zero[368] = {
        0x0018006Fu, 0x00241806u, 0x002C882Cu, 0x002DF800u, 0x002E0801u, 0x002E2001u,
        0x002E3800u, 0x00300005u, 0x0030800Au, 0x0030E000u, 0x00325814u, 0x00338000u,
        0x0036B007u, 0x0036F805u, 0x00373801u, 0x00375003u, 0x00387800u, 0x00388800u,
        0x0039801Au, 0x003D300Au, 0x003F5808u, 0x003FE800u, 0x0040B003u, 0x0040D808u,
        0x00412802u, 0x00414804u, 0x0042C802u, 0x00448001u, 0x0044B808u, 0x00465038u,
        0x0049D000u, 0x0049E000u, 0x004A0807u, 0x004A6800u, 0x004A8806u, 0x004B1001u,
        0x004C0800u, 0x004DE000u, 0x004E0803u, 0x004E6800u, 0x004F1001u, 0x004FF000u,
        0x00500801u, 0x0051E000u, 0x00520801u, 0x00523801u, 0x00525802u, 0x00528800u,
        0x00538001u, 0x0053A800u, 0x00540801u, 0x0055E000u, 0x00560804u, 0x00563801u,
        0x00566800u, 0x00571001u, 0x0057D005u, 0x00580800u, 0x0059E000u, 0x0059F800u,
        0x005A0803u, 0x005A6800u, 0x005AA801u, 0x005B1001u, 0x005C1000u, 0x005E0000u,
        0x005E6800u, 0x00600000u, 0x00602000u, 0x0061E000u, 0x0061F002u, 0x00623002u,
        0x00625003u, 0x0062A801u, 0x00631001u, 0x00640800u, 0x0065E000u, 0x0065F800u,
        0x00663000u, 0x00666001u, 0x00671001u, 0x00680001u, 0x0069D801u, 0x006A0803u,
        0x006A6800u, 0x006B1001u, 0x006C0800u, 0x006E5000u, 0x006E9002u, 0x006EB000u,
        0x00718800u, 0x0071A006u, 0x00723807u, 0x00758800u, 0x0075A008u, 0x00764006u,
        0x0078C001u, 0x0079A800u, 0x0079B800u, 0x0079C800u, 0x007B880Du, 0x007C0004u,
        0x007C3001u, 0x007C680Au, 0x007CC823u, 0x007E3000u, 0x00816803u, 0x00819005u,
        0x0081C801u, 0x0081E801u, 0x0082C001u, 0x0082F002u, 0x00838803u, 0x00841000u,
        0x00842801u, 0x00846800u, 0x0084E800u, 0x008B009Fu, 0x009AE802u, 0x00B89002u,
        0x00B99001u, 0x00BA9001u, 0x00BB9001u, 0x00BDA001u, 0x00BDB806u, 0x00BE3000u,
        0x00BE480Au, 0x00BEE800u, 0x00C05804u, 0x00C42801u, 0x00C54800u, 0x00C90002u,
        0x00C93801u, 0x00C99000u, 0x00C9C802u, 0x00D0B801u, 0x00D0D800u, 0x00D2B000u,
        0x00D2C006u, 0x00D30000u, 0x00D31000u, 0x00D32807u, 0x00D39809u, 0x00D3F800u,
        0x00D5801Eu, 0x00D80003u, 0x00D9A000u, 0x00D9B004u, 0x00D9E000u, 0x00DA1000u,
        0x00DB5808u, 0x00DC0001u, 0x00DD1003u, 0x00DD4001u, 0x00DD5802u, 0x00DF3000u,
        0x00DF4001u, 0x00DF6800u, 0x00DF7802u, 0x00E16007u, 0x00E1B001u, 0x00E68002u,
        0x00E6A00Cu, 0x00E71006u, 0x00E76800u, 0x00E7A000u, 0x00E7C001u, 0x00EE003Fu,
        0x01005804u, 0x01015004u, 0x01030004u, 0x01033009u, 0x01068020u, 0x01677802u,
        0x016BF800u, 0x016F001Fu, 0x01815003u, 0x0184C801u, 0x05337803u, 0x0533A009u,
        0x0534F001u, 0x05378001u, 0x05401000u, 0x05403000u, 0x05405800u, 0x05412801u,
        0x05416000u, 0x05462001u, 0x05470011u, 0x0547F800u, 0x05493007u, 0x054A380Au,
        0x054C0002u, 0x054D9800u, 0x054DB003u, 0x054DE001u, 0x054F2800u, 0x05514805u,
        0x05518801u, 0x0551A801u, 0x05521800u, 0x05526000u, 0x0553E000u, 0x05558000u,
        0x05559002u, 0x0555B801u, 0x0555F001u, 0x05560800u, 0x05576001u, 0x0557B000u,
        0x055F2800u, 0x055F4000u, 0x055F6800u, 0x07D8F000u, 0x07F0000Fu, 0x07F1000Fu,
        0x07F7F800u, 0x07FFC802u, 0x080FE800u, 0x08170000u, 0x081BB004u, 0x08500802u,
        0x08502801u, 0x08506003u, 0x0851C002u, 0x0851F800u, 0x08572801u, 0x08692003u,
        0x086B4804u, 0x08755801u, 0x0877E003u, 0x087A300Au, 0x087C1003u, 0x08800800u,
        0x0881C00Eu, 0x08838000u, 0x08839801u, 0x0883F802u, 0x08859803u, 0x0885C801u,
        0x0885E800u, 0x08861000u, 0x08866800u, 0x08880002u, 0x08893804u, 0x08896807u,
        0x088B9800u, 0x088C0001u, 0x088DB008u, 0x088E4803u, 0x088E7800u, 0x08917802u,
        0x0891A000u, 0x0891B001u, 0x0891F000u, 0x08920800u, 0x0896F800u, 0x08971807u,
        0x08980001u, 0x0899D801u, 0x089A0000u, 0x089B3006u, 0x089B8004u, 0x089DD805u,
        0x089E7000u, 0x089E8000u, 0x089E9000u, 0x089F0801u, 0x08A1C007u, 0x08A21002u,
        0x08A23000u, 0x08A2F000u, 0x08A59805u, 0x08A5D000u, 0x08A5F801u, 0x08A61001u,
        0x08AD9003u, 0x08ADE001u, 0x08ADF801u, 0x08AEE001u, 0x08B19807u, 0x08B1E800u,
        0x08B1F801u, 0x08B55800u, 0x08B56800u, 0x08B58005u, 0x08B5B800u, 0x08B8E800u,
        0x08B8F800u, 0x08B91003u, 0x08B93804u, 0x08C17808u, 0x08C1C801u, 0x08C9D801u,
        0x08C9F000u, 0x08CA1800u, 0x08CEA003u, 0x08CED001u, 0x08CF0000u, 0x08D00809u,
        0x08D19805u, 0x08D1D803u, 0x08D23800u, 0x08D28805u, 0x08D2C802u, 0x08D4500Cu,
        0x08D4C001u, 0x08E18006u, 0x08E1C005u, 0x08E1F800u, 0x08E49015u, 0x08E55006u,
        0x08E59001u, 0x08E5A801u, 0x08E98805u, 0x08E9D000u, 0x08E9E001u, 0x08E9F806u,
        0x08EA3800u, 0x08EC8001u, 0x08ECA800u, 0x08ECB800u, 0x08F79801u, 0x08F80001u,
        0x08F9B004u, 0x08FA0000u, 0x08FA1000u, 0x08FAD000u, 0x09A18010u, 0x09A2380Eu,
        0x0B08F00Bu, 0x0B096802u, 0x0B578004u, 0x0B598006u, 0x0B7A7800u, 0x0B7C7803u,
        0x0B7F2000u, 0x0DE4E801u, 0x0DE50003u, 0x0E78002Du, 0x0E798016u, 0x0E8B3802u,
        0x0E8B980Fu, 0x0E8C2806u, 0x0E8D5003u, 0x0E921002u, 0x0ED00036u, 0x0ED1D831u,
        0x0ED3A800u, 0x0ED42000u, 0x0ED4D804u, 0x0ED5080Eu, 0x0F000006u, 0x0F004010u,
        0x0F00D806u, 0x0F011801u, 0x0F013004u, 0x0F047800u, 0x0F098006u, 0x0F157000u,
        0x0F176003u, 0x0F276003u, 0x0F2F7001u, 0x0F468006u, 0x0F4A2006u, 0x70000800u,
        0x7001005Fu, 0x700800EFu,
    };
    static const uint32_t r2__tui_wide[206] = {
        0x0088005Fu, 0x0118D001u, 0x01194801u, 0x011F4803u, 0x011F8000u, 0x011F9800u,
        0x012FE801u, 0x0130A001u, 0x01318007u, 0x0132400Bu, 0x0133F800u, 0x01345005u,
        0x01349800u, 0x01350800u, 0x01355001u, 0x0135E801u, 0x01362001u, 0x01367000u,
        0x0136A000u, 0x01375000u, 0x01379001u, 0x0137A800u, 0x0137D000u, 0x0137E800u,
        0x01382800u, 0x01385001u, 0x01394000u, 0x013A6000u, 0x013A7000u, 0x013A9802u,
        0x013AB800u, 0x013CA802u, 0x013D8000u, 0x013DF800u, 0x0158D801u, 0x015A8000u,
        0x015AA800u, 0x01740019u, 0x0174D858u, 0x017800D5u, 0x017F8039u, 0x01817010u,
        0x01820855u, 0x0184D864u, 0x0188282Au, 0x0189885Du, 0x018C8055u, 0x018F782Fu,
        0x01910027u, 0x019287FFu, 0x01D287FFu, 0x021287FFu, 0x025287FFu, 0x029287FFu,
        0x02D287FFu, 0x031287FFu, 0x035287FFu, 0x039287FFu, 0x03D287FFu, 0x041287FFu,
        0x045287FFu, 0x049287FFu, 0x04D287FFu, 0x0512823Cu, 0x05248036u, 0x054B001Cu,
        0x056007FFu, 0x05A007FFu, 0x05E007FFu, 0x062007FFu, 0x066007FFu, 0x06A003A3u,
        0x07C801FFu, 0x07F08009u, 0x07F18022u, 0x07F2A012u, 0x07F34003u, 0x07F8085Fu,
        0x07FF0006u, 0x0B7F0003u, 0x0B7F8001u, 0x0B8007FFu, 0x0BC007FFu, 0x0C0007F7u,
        0x0C4004D5u, 0x0C67F809u, 0x0D7F8003u, 0x0D7FA806u, 0x0D7FE801u, 0x0D800122u,
        0x0D899000u, 0x0D8A8002u, 0x0D8AA800u, 0x0D8B2003u, 0x0D8B818Bu, 0x0E980056u,
        0x0E9B0016u, 0x0F802000u, 0x0F867800u, 0x0F8C7000u, 0x0F8C8809u, 0x0F900002u,
        0x0F90802Bu, 0x0F920008u, 0x0F928001u, 0x0F930005u, 0x0F980020u, 0x0F996808u,
        0x0F99B845u, 0x0F9BF015u, 0x0F9D002Au, 0x0F9E7804u, 0x0F9F0010u, 0x0F9FA000u,
        0x0F9FC046u, 0x0FA20000u, 0x0FA210BAu, 0x0FA7F83Eu, 0x0FAA5803u, 0x0FAA8017u,
        0x0FABD000u, 0x0FACA801u, 0x0FAD2000u, 0x0FAFD854u, 0x0FB40045u, 0x0FB66000u,
        0x0FB68002u, 0x0FB6A802u, 0x0FB6E003u, 0x0FB75801u, 0x0FB7A008u, 0x0FBF000Bu,
        0x0FBF8000u, 0x0FC8602Eu, 0x0FC9E009u, 0x0FCA38B8u, 0x0FD3800Cu, 0x0FD40009u,
        0x0FD47837u, 0x0FD6700Eu, 0x0FD6F80Au, 0x0FD78008u, 0x100007FFu, 0x104007FFu,
        0x108007FFu, 0x10C007FFu, 0x110007FFu, 0x114007FFu, 0x118007FFu, 0x11C007FFu,
        0x120007FFu, 0x124007FFu, 0x128007FFu, 0x12C007FFu, 0x130007FFu, 0x134007FFu,
        0x138007FFu, 0x13C007FFu, 0x140007FFu, 0x144007FFu, 0x148007FFu, 0x14C007FFu,
        0x150007FFu, 0x154007FFu, 0x158007FFu, 0x15C007FFu, 0x160007FFu, 0x164007FFu,
        0x168007FFu, 0x16C007FFu, 0x170007FFu, 0x174007FFu, 0x178007FFu, 0x17C007FDu,
        0x180007FFu, 0x184007FFu, 0x188007FFu, 0x18C007FFu, 0x190007FFu, 0x194007FFu,
        0x198007FFu, 0x19C007FFu, 0x1A0007FFu, 0x1A4007FFu, 0x1A8007FFu, 0x1AC007FFu,
        0x1B0007FFu, 0x1B4007FFu, 0x1B8007FFu, 0x1BC007FFu, 0x1C0007FFu, 0x1C4007FFu,
        0x1C8007FFu, 0x1CC007FFu, 0x1D0007FFu, 0x1D4007FFu, 0x1D8007FFu, 0x1DC007FFu,
        0x1E0007FFu, 0x1E4007FFu, 0x1E8007FFu, 0x1EC007FFu, 0x1F0007FFu, 0x1F4007FFu,
        0x1F8007FFu, 0x1FC007FDu,
    };
/* END GENERATED WIDTH TABLE */

    /* ---------------- width ---------------- */

    static int r2__tui_in_table(const uint32_t *table, uint32_t count, rune r)
    {
        uint32_t lo = 0, hi = count;
        while (lo < hi)
        {
            uint32_t mid = (lo + hi) / 2;
            uint32_t first = table[mid] >> 11;
            uint32_t last = first + (table[mid] & 0x7FFu);
            if (r < first)
                hi = mid;
            else if (r > last)
                lo = mid + 1;
            else
                return 1;
        }
        return 0;
    }

    R2_TUI_DEF int tui_rune_width(rune r)
    {
        if (r < 0x300u)
            return 1;
        if (r2__tui_in_table(r2__tui_zero, sizeof(r2__tui_zero) / sizeof(r2__tui_zero[0]), r))
            return 0;
        if (r2__tui_in_table(r2__tui_wide, sizeof(r2__tui_wide) / sizeof(r2__tui_wide[0]), r))
            return 2;
        return 1;
    }

    /* zero-width runes we drop rather than attach: joiners, variation
       selectors, bidi controls, BOM, tags. A terminal may use them to merge
       or reorder glyphs, which would break our cell layout. */
    static int r2__tui_dropped(rune r)
    {
        return (r >= 0x200Bu && r <= 0x200Fu) || (r >= 0x202Au && r <= 0x202Eu) ||
               (r >= 0x2060u && r <= 0x206Fu) || (r >= 0xFE00u && r <= 0xFE0Fu) || r == 0xFEFFu ||
               (r >= 0xE0000u && r <= 0xE0FFFu);
    }

    static int r2__tui_is_control(rune r)
    {
        return r < 0x20u || (r >= 0x7Fu && r < 0xA0u);
    }

    R2_TUI_DEF int tui_text_width(s8 str)
    {
        uint32_t at = 0;
        rune r;
        int w = 0;
        while (s8_next_rune(str, &at, &r))
            w += r2__tui_is_control(r) ? 1 : tui_rune_width(r);
        return w;
    }

    /* ---------------- setup ---------------- */

    static size_t r2__tui_bytes_for(uint32_t cells)
    {
        return R2__TUI_ALIGN((size_t)cells * 4u) * 2u + R2__TUI_ALIGN(((size_t)cells + 7u) / 8u) * 2u + R2__TUI_FIXED;
    }

    /* Borland Turbo Vision colours */
    static const tui_theme r2__tui_theme_borland = {
        TUI_STYLE(TUI_WHITE, TUI_BLUE, 0), 0x2591,                 /* desktop */
        TUI_STYLE(TUI_BLACK, TUI_WHITE, 0),                        /* menu */
        TUI_STYLE(TUI_RED, TUI_WHITE, 0),                          /* menu_hot */
        TUI_STYLE(TUI_BLACK, TUI_GREEN, 0),                        /* menu_sel */
        TUI_STYLE(TUI_RED, TUI_GREEN, 0),                          /* menu_sel_hot */
        TUI_STYLE(TUI_DARK_GRAY, TUI_WHITE, 0),                    /* menu_disabled */
        TUI_STYLE(TUI_WHITE, TUI_BLUE, 0),                         /* frame */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, 0),                  /* frame_active */
        TUI_STYLE(TUI_BRIGHT_GREEN, TUI_BLUE, 0),                  /* frame_move */
        TUI_STYLE(TUI_BRIGHT_YELLOW, TUI_BLUE, 0),                 /* body */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_WHITE, 0),                 /* dialog_frame */
        TUI_STYLE(TUI_BLACK, TUI_WHITE, 0),                        /* dialog_body */
        TUI_STYLE(TUI_BLACK, TUI_WHITE, 0),                        /* status */
        TUI_STYLE(TUI_RED, TUI_WHITE, 0),                          /* status_hot */
        TUI_STYLE(TUI_BLACK, TUI_GREEN, 0),                        /* button */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_GREEN, 0),                 /* button_focus */
        TUI_STYLE(TUI_BRIGHT_YELLOW, TUI_GREEN, 0),                /* button_hot */
        TUI_STYLE(TUI_RED, TUI_WHITE, 0),                          /* hot (foreground) */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_WHITE, 0),                 /* focus (foreground) */
        TUI_STYLE(TUI_WHITE, TUI_BLUE, 0),                         /* input */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_BLUE, 0),                  /* input_focus */
        TUI_STYLE(TUI_BLACK, TUI_CYAN, 0),                         /* list */
        TUI_STYLE(TUI_BLACK, TUI_GREEN, 0),                        /* list_sel */
        TUI_STYLE(TUI_BRIGHT_WHITE, TUI_GREEN, 0),                 /* list_sel_focus */
        TUI_STYLE(TUI_BLUE, TUI_CYAN, 0),                          /* scrollbar */
    };

    static void r2__tui_blank(tui *t)
    {
        tui_cell blank = TUI_CELL(' ', TUI_STYLE(TUI_WHITE, TUI_BLACK, 0));
        uint32_t n = (uint32_t)(t->cols * t->rows), i;
        for (i = 0; i < n; i++)
            t->back[i] = blank;
        for (i = 0; i < (n + 7u) / 8u; i++)
            t->back_has[i] = 0;
        t->back_ncomb = 0;
    }

    R2_TUI_DEF void tui_invalidate(tui *t)
    {
        uint32_t i;
        for (i = 0; i < ((uint32_t)(t->cols * t->rows) + 7u) / 8u; i++)
            t->front_has[i] = 0;
        t->front_ncomb = 0;
        t->full = 1;
        t->dirty = 1;
        t->in_frame = 0;
        t->tx = t->ty = -1;
        t->tstyle = -1;
        t->shown_vis = -1;
    }

    R2_TUI_DEF int tui_resize(tui *t, int cols, int rows)
    {
        if (cols < 1 || rows < 1 || (uint32_t)cols * (uint32_t)rows > t->cap_cells)
            return -1;
        t->cols = cols;
        t->rows = rows;
        r2__tui_blank(t);
        tui_clip_reset(t);
        if (t->cur_x >= cols || t->cur_y >= rows)
            t->cur_visible = 0;
        tui_invalidate(t);
        if (t->win)
        {
            int i;
            for (i = 0; i < R2__TUI_SLOTS; i++)
                t->win[i].dirty = t->win[i].used;
            t->layout_dirty = 1;
            t->compose_dirty = 1;
        }
        return 0;
    }

    R2_TUI_DEF int tui_init(tui *t, void *arena, size_t size, int cols, int rows)
    {
        uint8_t *p = (uint8_t *)arena;
        uint32_t cells;
        size_t pad = (size_t)(-(uintptr_t)p & 7u);
        size_t fixed = R2__TUI_FIXED;
        uint32_t i;
        uint8_t *tb = (uint8_t *)t;

        for (i = 0; i < sizeof(*t); i++)
            tb[i] = 0;
        if (!arena || size < fixed + 64u)
            return -1;
        /* largest cell count the arena holds */
        cells = (uint32_t)(((size - fixed) * 8u) / 66u);
        while (cells > 0 && r2__tui_bytes_for(cells) > size)
            cells--;
        if (cols < 1 || rows < 1 || (uint32_t)cols * (uint32_t)rows > cells)
            return -1;

        p += pad;
        t->cap_cells = cells;
        t->back = (tui_cell *)p;
        p += R2__TUI_ALIGN((size_t)cells * 4u);
        t->front = (tui_cell *)p;
        p += R2__TUI_ALIGN((size_t)cells * 4u);
        t->back_has = p;
        p += R2__TUI_ALIGN(((size_t)cells + 7u) / 8u);
        t->front_has = p;
        p += R2__TUI_ALIGN(((size_t)cells + 7u) / 8u);
        t->back_comb = (tui_comb *)p;
        p += R2__TUI_ALIGN(sizeof(tui_comb) * R2_TUI_MAX_COMBINING);
        t->front_comb = (tui_comb *)p;
        p += R2__TUI_ALIGN(sizeof(tui_comb) * R2_TUI_MAX_COMBINING) * 2u;
        t->win = (tui_window *)p;
        p += R2__TUI_ALIGN(sizeof(tui_window) * R2__TUI_SLOTS);
        t->cmd_pool = p;
        p += R2__TUI_ALIGN(R2_TUI_CMD_POOL);
        t->bar_win = (tui_menubar *)p;
        p += R2__TUI_ALIGN(sizeof(tui_menubar));
        t->bar_desk = (tui_menubar *)p;
        for (i = 0; i < sizeof(tui_window) * R2__TUI_SLOTS; i++)
            ((uint8_t *)t->win)[i] = 0;
        t->bar_win->nmenus = t->bar_win->nitems = 0;
        t->bar_desk->nmenus = t->bar_desk->nitems = 0;

        t->sync = 1;
        t->esc_ms = R2_TUI_ESC_MS;
        t->theme = &r2__tui_theme_borland;
        t->focus = -1;
        t->cur = -1;
        t->menu_act = -1;
        t->mb_owner = -1;
        tui_set_prefix(t, R2_TUI_PREFIX);
        tui_set_palette_key(t, R2_TUI_PALETTE_KEY);
        return tui_resize(t, cols, rows);
    }

    /* ---------------- combining marks ---------------- */

    static int r2__tui_has(const uint8_t *bits, uint32_t i)
    {
        return (bits[i >> 3] >> (i & 7u)) & 1u;
    }

    static rune r2__tui_mark(const tui_comb *table, uint32_t count, uint32_t i)
    {
        uint32_t k;
        for (k = 0; k < count; k++)
            if (table[k].index == i)
                return table[k].mark;
        return 0;
    }

    static void r2__tui_mark_clear(uint8_t *bits, tui_comb *table, uint32_t *count, uint32_t i)
    {
        uint32_t k;
        if (!r2__tui_has(bits, i))
            return;
        bits[i >> 3] &= (uint8_t)~(1u << (i & 7u));
        for (k = 0; k < *count; k++)
        {
            if (table[k].index == i)
            {
                table[k] = table[--*count];
                return;
            }
        }
    }

    /* one mark per cell: a second mark on the same cell is dropped */
    static void r2__tui_mark_set(uint8_t *bits, tui_comb *table, uint32_t *count, uint32_t cap, uint32_t i,
                                 rune mark)
    {
        if (r2__tui_has(bits, i) || *count >= cap)
            return;
        bits[i >> 3] |= (uint8_t)(1u << (i & 7u));
        table[*count].index = i;
        table[*count].mark = mark;
        ++*count;
    }

    /* ---------------- drawing ---------------- */

    R2_TUI_DEF void tui_clip_reset(tui *t)
    {
        t->clip.x = 0;
        t->clip.y = 0;
        t->clip.w = t->cols;
        t->clip.h = t->rows;
    }

    R2_TUI_DEF void tui_clip(tui *t, tui_rect r)
    {
        int x0 = r.x < 0 ? 0 : r.x;
        int y0 = r.y < 0 ? 0 : r.y;
        int x1 = r.x + r.w > t->cols ? t->cols : r.x + r.w;
        int y1 = r.y + r.h > t->rows ? t->rows : r.y + r.h;
        t->clip.x = x0;
        t->clip.y = y0;
        t->clip.w = x1 > x0 ? x1 - x0 : 0;
        t->clip.h = y1 > y0 ? y1 - y0 : 0;
    }

    static int r2__tui_in_clip(const tui *t, int x, int y)
    {
        return x >= t->clip.x && x < t->clip.x + t->clip.w && y >= t->clip.y && y < t->clip.y + t->clip.h;
    }

    static int r2__tui_is_lead(const tui *t, uint32_t i, int x)
    {
        return x + 1 < t->cols && TUI_CELL_RUNE(t->back[i + 1]) == TUI_RUNE_CONT;
    }

    /* Store a cell, repairing any wide character it cuts in half */
    static void r2__tui_put(tui *t, int x, int y, tui_cell c)
    {
        uint32_t i = (uint32_t)(y * t->cols + x);
        tui_cell old = t->back[i];
        if (old == c && !r2__tui_has(t->back_has, i))
            return; /* same cell: no change, no frame */
        if (TUI_CELL_RUNE(old) == TUI_RUNE_CONT && TUI_CELL_RUNE(c) != TUI_RUNE_CONT && x > 0)
        {
            t->back[i - 1] = TUI_CELL(' ', TUI_CELL_STYLE(t->back[i - 1]));
            r2__tui_mark_clear(t->back_has, t->back_comb, &t->back_ncomb, i - 1);
        }
        if (TUI_CELL_RUNE(old) != TUI_RUNE_CONT && r2__tui_is_lead(t, i, x))
            t->back[i + 1] = TUI_CELL(' ', TUI_CELL_STYLE(t->back[i + 1]));
        r2__tui_mark_clear(t->back_has, t->back_comb, &t->back_ncomb, i);
        t->back[i] = c;
        t->dirty = 1;
    }

    R2_TUI_DEF int tui_draw_rune(tui *t, int x, int y, rune r, tui_style s)
    {
        int w;
        if (r2__tui_is_control(r))
            r = '?';
        if (r2__tui_dropped(r))
            return 0;
        w = tui_rune_width(r);
        if (w == 0)
        {
            /* attach to the cell before (the lead, if that is a wide char) */
            int px = x - 1;
            uint32_t i;
            if (px >= 0 && px < t->cols && y >= 0 && y < t->rows &&
                TUI_CELL_RUNE(t->back[y * t->cols + px]) == TUI_RUNE_CONT)
                px--;
            if (px < 0 || !r2__tui_in_clip(t, px, y))
                return 0;
            i = (uint32_t)(y * t->cols + px);
            if (!r2__tui_has(t->back_has, i))
            {
                r2__tui_mark_set(t->back_has, t->back_comb, &t->back_ncomb, R2_TUI_MAX_COMBINING, i, r);
                t->dirty = 1;
            }
            return 0;
        }
        if (w == 2)
        {
            int in0 = r2__tui_in_clip(t, x, y);
            int in1 = r2__tui_in_clip(t, x + 1, y);
            if (in0 && in1)
            {
                r2__tui_put(t, x, y, TUI_CELL(r, s));
                r2__tui_put(t, x + 1, y, TUI_CELL(TUI_RUNE_CONT, s));
            }
            else if (in0)
                r2__tui_put(t, x, y, TUI_CELL(' ', s));
            else if (in1)
                r2__tui_put(t, x + 1, y, TUI_CELL(' ', s));
            return 2;
        }
        if (r2__tui_in_clip(t, x, y))
            r2__tui_put(t, x, y, TUI_CELL(r, s));
        return 1;
    }

    R2_TUI_DEF int tui_draw_text(tui *t, int x, int y, s8 str, tui_style s)
    {
        uint32_t at = 0;
        rune r;
        int cx = x;
        int right = t->clip.x + t->clip.w;
        while (s8_next_rune(str, &at, &r))
        {
            if (cx >= right && tui_rune_width(r) != 0)
                break;
            cx += tui_draw_rune(t, cx, y, r, s);
        }
        return cx - x;
    }

    R2_TUI_DEF void tui_draw_fill(tui *t, tui_rect r, rune ch, tui_style s)
    {
        int x, y;
        if (r2__tui_is_control(ch) || tui_rune_width(ch) != 1)
            ch = ' ';
        for (y = r.y; y < r.y + r.h; y++)
            for (x = r.x; x < r.x + r.w; x++)
                if (r2__tui_in_clip(t, x, y))
                    r2__tui_put(t, x, y, TUI_CELL(ch, s));
    }

    R2_TUI_DEF void tui_draw_box(tui *t, tui_rect r, tui_style s, int kind)
    {
        /* tl tr bl br horizontal vertical */
        static const rune glyph[2][6] = {{0x250C, 0x2510, 0x2514, 0x2518, 0x2500, 0x2502},
                                         {0x2554, 0x2557, 0x255A, 0x255D, 0x2550, 0x2551}};
        const rune *g = glyph[kind == TUI_BOX_DOUBLE];
        int x, y, x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
        if (r.w < 2 || r.h < 2)
            return;
        for (x = r.x + 1; x < x1; x++)
        {
            tui_draw_rune(t, x, r.y, g[4], s);
            tui_draw_rune(t, x, y1, g[4], s);
        }
        for (y = r.y + 1; y < y1; y++)
        {
            tui_draw_rune(t, r.x, y, g[5], s);
            tui_draw_rune(t, x1, y, g[5], s);
        }
        tui_draw_rune(t, r.x, r.y, g[0], s);
        tui_draw_rune(t, x1, r.y, g[1], s);
        tui_draw_rune(t, r.x, y1, g[2], s);
        tui_draw_rune(t, x1, y1, g[3], s);
    }

    R2_TUI_DEF void tui_draw_shadow(tui *t, tui_rect r)
    {
        tui_style s = TUI_STYLE(TUI_DARK_GRAY, TUI_BLACK, 0);
        int x, y;
        for (y = r.y; y < r.y + r.h; y++)
        {
            for (x = r.x; x < r.x + r.w; x++)
            {
                if (r2__tui_in_clip(t, x, y))
                {
                    uint32_t i = (uint32_t)(y * t->cols + x);
                    tui_cell c = TUI_CELL(TUI_CELL_RUNE(t->back[i]), s);
                    if (t->back[i] != c)
                    {
                        t->back[i] = c;
                        t->dirty = 1;
                    }
                }
            }
        }
    }

    R2_TUI_DEF void tui_set_cursor(tui *t, int x, int y, int visible)
    {
        visible = visible && x >= 0 && y >= 0 && x < t->cols && y < t->rows;
        if (x != t->cur_x || y != t->cur_y || visible != t->cur_visible)
            t->dirty = 1;
        t->cur_x = x;
        t->cur_y = y;
        t->cur_visible = visible;
    }

    R2_TUI_DEF tui_cell tui_get_cell(const tui *t, int x, int y)
    {
        if (x < 0 || y < 0 || x >= t->cols || y >= t->rows)
            return 0;
        return t->back[y * t->cols + x];
    }

    R2_TUI_DEF rune tui_get_mark(const tui *t, int x, int y)
    {
        uint32_t i;
        if (x < 0 || y < 0 || x >= t->cols || y >= t->rows)
            return 0;
        i = (uint32_t)(y * t->cols + x);
        return r2__tui_has(t->back_has, i) ? r2__tui_mark(t->back_comb, t->back_ncomb, i) : 0;
    }

    /* ---------------- renderer ---------------- */

    static size_t r2__tui_puts(uint8_t *out, const char *s)
    {
        size_t n = 0;
        while (s[n])
        {
            out[n] = (uint8_t)s[n];
            n++;
        }
        return n;
    }

    static size_t r2__tui_putu(uint8_t *out, uint32_t v)
    {
        uint8_t tmp[10];
        size_t n = 0, k = 0;
        do
        {
            tmp[n++] = (uint8_t)('0' + v % 10u);
            v /= 10u;
        } while (v);
        while (n)
            out[k++] = tmp[--n];
        return k;
    }

    static size_t r2__tui_cup(uint8_t *out, int x, int y)
    {
        size_t n = r2__tui_puts(out, "\033[");
        n += r2__tui_putu(out + n, (uint32_t)y + 1u);
        out[n++] = ';';
        n += r2__tui_putu(out + n, (uint32_t)x + 1u);
        out[n++] = 'H';
        return n;
    }

    static size_t r2__tui_sgr(uint8_t *out, tui_style s)
    {
        uint32_t fg = TUI_STYLE_FG(s), bg = TUI_STYLE_BG(s), a = TUI_STYLE_ATTR(s);
        size_t n = r2__tui_puts(out, "\033[0");
        if (a & TUI_BOLD)
            n += r2__tui_puts(out + n, ";1");
        if (a & TUI_UNDERLINE)
            n += r2__tui_puts(out + n, ";4");
        if (a & TUI_REVERSE)
            n += r2__tui_puts(out + n, ";7");
        out[n++] = ';';
        n += r2__tui_putu(out + n, fg < 8 ? 30u + fg : 90u + fg - 8u);
        out[n++] = ';';
        n += r2__tui_putu(out + n, bg < 8 ? 40u + bg : 100u + bg - 8u);
        out[n++] = 'm';
        return n;
    }

    static int r2__tui_same(const tui *t, uint32_t i)
    {
        int hb, hf;
        if (t->full || t->back[i] != t->front[i])
            return 0;
        hb = r2__tui_has(t->back_has, i);
        hf = r2__tui_has(t->front_has, i);
        if (hb != hf)
            return 0;
        return !hb || r2__tui_mark(t->back_comb, t->back_ncomb, i) == r2__tui_mark(t->front_comb, t->front_ncomb, i);
    }

    static void r2__tui_sync_front(tui *t, uint32_t i)
    {
        t->front[i] = t->back[i];
        r2__tui_mark_clear(t->front_has, t->front_comb, &t->front_ncomb, i);
        if (r2__tui_has(t->back_has, i))
            r2__tui_mark_set(t->front_has, t->front_comb, &t->front_ncomb, 2u * R2_TUI_MAX_COMBINING, i,
                             r2__tui_mark(t->back_comb, t->back_ncomb, i));
    }

    /* Bytes to bring cell i (and its right half, if wide) up to date */
    static size_t r2__tui_cell_unit(tui *t, uint32_t i, uint8_t *out, int *advance)
    {
        int x = (int)(i % (uint32_t)t->cols), y = (int)(i / (uint32_t)t->cols);
        tui_cell c = t->back[i];
        rune r = TUI_CELL_RUNE(c);
        tui_style s = TUI_CELL_STYLE(c);
        size_t n = 0;
        int w = r2__tui_is_lead(t, i, x) ? 2 : 1;

        if (t->ty != y || t->tx != x)
        {
            if (t->ty == y && t->tx >= 0 && x > t->tx)
            {
                n += r2__tui_puts(out + n, "\033[");
                if (x - t->tx > 1)
                    n += r2__tui_putu(out + n, (uint32_t)(x - t->tx));
                out[n++] = 'C';
            }
            else
                n += r2__tui_cup(out + n, x, y);
        }
        if ((int32_t)s != t->tstyle)
            n += r2__tui_sgr(out + n, s);
        n += utf8_encode((r == 0 || r == TUI_RUNE_CONT) ? ' ' : r, out + n);
        if (r2__tui_has(t->back_has, i))
            n += utf8_encode(r2__tui_mark(t->back_comb, t->back_ncomb, i), out + n);
        *advance = w;
        return n;
    }

    static size_t r2__tui_prefix(tui *t, uint8_t *out)
    {
        size_t n = 0;
        if (t->sync)
            n += r2__tui_puts(out + n, ESC_SYNC_BEGIN);
        n += r2__tui_puts(out + n, ESC_HIDE_CURSOR);
        t->opened = 1;
        return n;
    }

    R2_TUI_DEF int tui_render_pending(const tui *t)
    {
        return t->in_frame || t->dirty;
    }

    R2_TUI_DEF size_t tui_render(tui *t, uint8_t *out, size_t cap)
    {
        uint8_t unit[64];
        size_t n = 0, len;
        uint32_t total = (uint32_t)(t->cols * t->rows);

        if (cap < R2_TUI_RENDER_MIN)
            return 0;
        if (!t->in_frame)
        {
            if (!t->dirty)
                return 0;
            t->in_frame = 1;
            t->dirty = 0;
            t->stage = 1;
            t->scan = 0;
            t->opened = 0;
        }
        /* The prefix goes out with the first changed cell, so a frame where
           nothing really changed (drawn again the same) sends nothing. */
        while (t->stage == 1 && t->scan < total)
        {
            uint32_t i = t->scan;
            int advance = 1;
            int x = (int)(i % (uint32_t)t->cols);
            int wide = r2__tui_is_lead(t, i, x);
            if (r2__tui_same(t, i) && (!wide || r2__tui_same(t, i + 1)))
            {
                t->scan += wide ? 2u : 1u;
                continue;
            }
            if (TUI_CELL_RUNE(t->back[i]) == TUI_RUNE_CONT)
            {
                /* orphaned right half (its lead was handled): just record it */
                r2__tui_sync_front(t, i);
                t->scan++;
                continue;
            }
            len = r2__tui_cell_unit(t, i, unit, &advance);
            if (n + len + (t->opened ? 0u : 16u) > cap)
                return n;
            if (!t->opened)
                n += r2__tui_prefix(t, out + n);
            for (size_t k = 0; k < len; k++)
                out[n + k] = unit[k];
            n += len;
            t->tstyle = (int32_t)TUI_CELL_STYLE(t->back[i]);
            r2__tui_sync_front(t, i);
            if (advance == 2)
                r2__tui_sync_front(t, i + 1);
            t->tx = x + advance;
            t->ty = (int)(i / (uint32_t)t->cols);
            if (t->tx >= t->cols)
                t->tx = t->ty = -1; /* pending wrap: position unknown */
            t->scan += (uint32_t)advance;
        }
        if (t->stage == 1)
            t->stage = 2;
        if (t->stage == 2)
        {
            int cursor_same = t->shown_vis == t->cur_visible &&
                              (!t->cur_visible || (t->shown_x == t->cur_x && t->shown_y == t->cur_y));
            if (n + 48u > cap)
                return n;
            if (t->opened || !cursor_same)
            {
                if (!t->opened)
                    n += r2__tui_prefix(t, out + n);
                if (t->cur_visible)
                {
                    n += r2__tui_cup(out + n, t->cur_x, t->cur_y);
                    n += r2__tui_puts(out + n, ESC_SHOW_CURSOR);
                    t->tx = t->cur_x;
                    t->ty = t->cur_y;
                }
                if (t->sync)
                    n += r2__tui_puts(out + n, ESC_SYNC_END);
            }
            t->shown_vis = (int8_t)t->cur_visible;
            t->shown_x = t->cur_x;
            t->shown_y = t->cur_y;
            t->in_frame = 0;
            t->full = 0;
        }
        return n;
    }

    /* ---------------- input ---------------- */

    static void r2__tui_push(tui *t, const tui_event *ev)
    {
        if (ev->type == TUI_EV_NONE || t->q_len >= R2_TUI_EVENT_QUEUE)
            return;
        t->queue[(t->q_head + t->q_len) % R2_TUI_EVENT_QUEUE] = *ev;
        t->q_len++;
    }

    static void r2__tui_key(tui_event *ev, uint16_t key, rune ch, uint8_t mods)
    {
        ev->type = TUI_EV_KEY;
        ev->key = key;
        ev->ch = ch;
        ev->mods = mods;
        ev->x = ev->y = 0;
    }

    static size_t r2__tui_parse(tui *t, const uint8_t *b, size_t len, int final, tui_event *ev);

    static uint8_t r2__tui_mods(uint32_t param)
    {
        uint32_t m = param > 1u ? param - 1u : 0u;
        return (uint8_t)((m & 1u ? TUI_MOD_SHIFT : 0u) | (m & 2u ? TUI_MOD_ALT : 0u) |
                         (m & 4u ? TUI_MOD_CTRL : 0u) | (m & 8u ? TUI_MOD_ALT : 0u));
    }

    /* CSI: ESC [ params final */
    static size_t r2__tui_csi(tui *t, const uint8_t *b, size_t len, int final, tui_event *ev)
    {
        uint32_t p[4] = {0, 0, 0, 0};
        int np = 0, have = 0, priv = 0;
        size_t j = 2;
        uint8_t f;

        if (len >= 3 && b[2] == '[')
        {
            /* Linux console F1-F5: ESC [ [ A..E */
            if (len < 4)
                return final ? (r2__tui_key(ev, TUI_KEY_RUNE, '[', TUI_MOD_ALT), 2u) : 0u;
            if (b[3] >= 'A' && b[3] <= 'E')
                r2__tui_key(ev, (uint16_t)(TUI_KEY_F1 + (b[3] - 'A')), 0, 0);
            return 4;
        }
        for (; j < len; j++)
        {
            uint8_t c = b[j];
            if (c >= '0' && c <= '9')
            {
                if (np < 4)
                    p[np] = p[np] * 10u + (uint32_t)(c - '0');
                have = 1;
            }
            else if (c == ';')
            {
                if (np < 4)
                    np++;
                have = 1;
            }
            else if (c >= 0x3C && c <= 0x3F)
                priv = 1; /* < = > ? : private, not a key */
            else if (c >= 0x20 && c <= 0x2F)
                priv = 1; /* intermediates: not a key we know */
            else
                break;
        }
        if (j == len)
        {
            if (!final)
                return 0;
            if (len == 2)
            {
                r2__tui_key(ev, TUI_KEY_RUNE, '[', TUI_MOD_ALT);
                return 2;
            }
            return len; /* cut off: drop it */
        }
        f = b[j];
        if (f < 0x40 || f > 0x7E)
            return j; /* broken: drop up to the stray byte */
        if (have)
            np++;
        if (priv)
            return j + 1;

        switch (f)
        {
        case 'A': r2__tui_key(ev, TUI_KEY_UP, 0, r2__tui_mods(p[1])); break;
        case 'B': r2__tui_key(ev, TUI_KEY_DOWN, 0, r2__tui_mods(p[1])); break;
        case 'C': r2__tui_key(ev, TUI_KEY_RIGHT, 0, r2__tui_mods(p[1])); break;
        case 'D': r2__tui_key(ev, TUI_KEY_LEFT, 0, r2__tui_mods(p[1])); break;
        case 'H': r2__tui_key(ev, TUI_KEY_HOME, 0, r2__tui_mods(p[1])); break;
        case 'F': r2__tui_key(ev, TUI_KEY_END, 0, r2__tui_mods(p[1])); break;
        case 'Z': r2__tui_key(ev, TUI_KEY_TAB, 0, TUI_MOD_SHIFT); break;
        case 'P': r2__tui_key(ev, TUI_KEY_F1, 0, r2__tui_mods(p[1])); break;
        case 'Q': r2__tui_key(ev, TUI_KEY_F1 + 1, 0, r2__tui_mods(p[1])); break;
        case 'S': r2__tui_key(ev, TUI_KEY_F1 + 3, 0, r2__tui_mods(p[1])); break;
        case 'R':
            if (t->cpr_pending && np == 2)
            {
                /* cursor position report: the reply to tui_query_size */
                t->cpr_pending = 0;
                ev->type = TUI_EV_RESIZE;
                ev->key = TUI_KEY_NONE;
                ev->mods = 0;
                ev->ch = 0;
                ev->x = (int)p[1];
                ev->y = (int)p[0];
            }
            else
                r2__tui_key(ev, TUI_KEY_F1 + 2, 0, r2__tui_mods(p[1]));
            break;
        case '~':
        {
            static const uint16_t tilde[25] = {
                0, TUI_KEY_HOME, TUI_KEY_INSERT, TUI_KEY_DELETE, TUI_KEY_END, TUI_KEY_PGUP, TUI_KEY_PGDN,
                TUI_KEY_HOME, TUI_KEY_END, 0, 0, TUI_KEY_F1, TUI_KEY_F1 + 1, TUI_KEY_F1 + 2, TUI_KEY_F1 + 3,
                TUI_KEY_F1 + 4, 0, TUI_KEY_F1 + 5, TUI_KEY_F1 + 6, TUI_KEY_F1 + 7, TUI_KEY_F1 + 8,
                TUI_KEY_F1 + 9, 0, TUI_KEY_F1 + 10, TUI_KEY_F1 + 11};
            if (p[0] < 25 && tilde[p[0]])
                r2__tui_key(ev, tilde[p[0]], 0, r2__tui_mods(p[1]));
            break;
        }
        default:
            break;
        }
        return j + 1;
    }

    /* SS3: ESC O [digits] final */
    static size_t r2__tui_ss3(const uint8_t *b, size_t len, int final, tui_event *ev)
    {
        size_t j = 2;
        uint32_t m = 0;
        while (j < len && b[j] >= '0' && b[j] <= '9')
            m = m * 10u + (uint32_t)(b[j++] - '0');
        if (j == len)
        {
            if (!final)
                return 0;
            if (len == 2)
            {
                r2__tui_key(ev, TUI_KEY_RUNE, 'O', TUI_MOD_ALT);
                return 2;
            }
            return len;
        }
        switch (b[j])
        {
        case 'A': r2__tui_key(ev, TUI_KEY_UP, 0, r2__tui_mods(m)); break;
        case 'B': r2__tui_key(ev, TUI_KEY_DOWN, 0, r2__tui_mods(m)); break;
        case 'C': r2__tui_key(ev, TUI_KEY_RIGHT, 0, r2__tui_mods(m)); break;
        case 'D': r2__tui_key(ev, TUI_KEY_LEFT, 0, r2__tui_mods(m)); break;
        case 'H': r2__tui_key(ev, TUI_KEY_HOME, 0, r2__tui_mods(m)); break;
        case 'F': r2__tui_key(ev, TUI_KEY_END, 0, r2__tui_mods(m)); break;
        case 'M': r2__tui_key(ev, TUI_KEY_ENTER, 0, r2__tui_mods(m)); break;
        case 'P': case 'Q': case 'R': case 'S':
            r2__tui_key(ev, (uint16_t)(TUI_KEY_F1 + (b[j] - 'P')), 0, r2__tui_mods(m));
            break;
        default:
            if (b[j] < 0x20)
                return j; /* broken: drop up to the stray byte */
            break;
        }
        return j + 1;
    }

    static size_t r2__tui_parse(tui *t, const uint8_t *b, size_t len, int final, tui_event *ev)
    {
        uint8_t c = b[0];
        ev->type = TUI_EV_NONE;
        if (c == 0x1B)
        {
            size_t k;
            if (len == 1)
            {
                if (!final)
                    return 0;
                r2__tui_key(ev, TUI_KEY_ESC, 0, 0);
                return 1;
            }
            if (b[1] == '[')
                return r2__tui_csi(t, b, len, final, ev);
            if (b[1] == 'O')
                return r2__tui_ss3(b, len, final, ev);
            if (b[1] == 0x1B)
            {
                r2__tui_key(ev, TUI_KEY_ESC, 0, 0);
                return 1;
            }
            /* ESC + key = Alt + key */
            k = r2__tui_parse(t, b + 1, len - 1, final, ev);
            if (k == 0)
                return 0;
            ev->mods |= TUI_MOD_ALT;
            return k + 1;
        }
        if (c < 0x80)
        {
            switch (c)
            {
            case 0x0D: case 0x0A: r2__tui_key(ev, TUI_KEY_ENTER, 0, 0); break;
            case 0x09: r2__tui_key(ev, TUI_KEY_TAB, 0, 0); break;
            case 0x7F: case 0x08: r2__tui_key(ev, TUI_KEY_BACKSPACE, 0, 0); break;
            case 0x00: r2__tui_key(ev, TUI_KEY_RUNE, ' ', TUI_MOD_CTRL); break;
            default:
                if (c <= 0x1A)
                    r2__tui_key(ev, TUI_KEY_RUNE, (rune)('a' + c - 1), TUI_MOD_CTRL);
                else if (c < 0x20)
                    r2__tui_key(ev, TUI_KEY_RUNE, (rune)("\\]^_"[c - 0x1C]), TUI_MOD_CTRL);
                else
                    r2__tui_key(ev, TUI_KEY_RUNE, c, 0);
                break;
            }
            return 1;
        }
        else
        {
            /* UTF-8: wait for the whole sequence unless it is already broken */
            size_t need = (c >= 0xC2 && c <= 0xDF) ? 2u : (c >= 0xE0 && c <= 0xEF) ? 3u : (c >= 0xF0 && c <= 0xF4) ? 4u : 1u;
            rune r;
            uint32_t used;
            if (len < need && !final)
            {
                size_t j;
                for (j = 1; j < len && (b[j] & 0xC0u) == 0x80u; j++)
                    ;
                if (j == len)
                    return 0;
            }
            used = utf8_decode(b, (uint32_t)len, &r);
            r2__tui_key(ev, TUI_KEY_RUNE, r, 0);
            return used;
        }
    }

    static void r2__tui_drain(tui *t, int final)
    {
        while (t->in_len > 0)
        {
            tui_event ev;
            size_t used = r2__tui_parse(t, t->in_buf, t->in_len, final, &ev);
            size_t k;
            if (used == 0)
                return;
            r2__tui_push(t, &ev);
            for (k = used; k < t->in_len; k++)
                t->in_buf[k - used] = t->in_buf[k];
            t->in_len = (uint8_t)(t->in_len - used);
        }
    }

    R2_TUI_DEF void tui_step(tui *t, uint32_t now_ms)
    {
        if (t->in_len > 0 && (uint32_t)(now_ms - t->in_time) >= t->esc_ms)
            r2__tui_drain(t, 1);
    }

    R2_TUI_DEF void tui_feed(tui *t, const uint8_t *bytes, size_t n, uint32_t now_ms)
    {
        size_t i;
        for (i = 0; i < n; i++)
        {
            tui_step(t, now_ms);
            if (t->in_len == 0)
                t->in_time = now_ms;
            t->in_buf[t->in_len++] = bytes[i];
            r2__tui_drain(t, 0);
            if (t->in_len == sizeof(t->in_buf))
                r2__tui_drain(t, 1); /* too long for anything we know */
        }
    }

    R2_TUI_DEF int tui_input_pending(const tui *t)
    {
        return t->in_len > 0;
    }

    R2_TUI_DEF int tui_poll(tui *t, tui_event *ev)
    {
        if (t->q_len == 0)
            return 0;
        *ev = t->queue[t->q_head];
        t->q_head = (t->q_head + 1u) % R2_TUI_EVENT_QUEUE;
        t->q_len--;
        return 1;
    }

    R2_TUI_DEF size_t tui_query_size(tui *t, uint8_t *out, size_t cap)
    {
        /* save cursor, go as far as possible, ask where we are, restore */
        static const char q[] = ESC_SAVE_CURSOR_DEC "\033[999;999H" ESC_QUERY_CURSOR_POSITION ESC_RESTORE_CURSOR_DEC;
        if (cap < sizeof(q) - 1)
            return 0;
        t->cpr_pending = 1;
        return r2__tui_puts(out, q);
    }

    R2_TUI_DEF void tui_key_name(const tui_event *ev, char *buf, size_t cap)
    {
        static const char *const names[] = {"", "", "Enter", "Tab", "Backspace", "Esc", "Up", "Down", "Left",
                                            "Right", "Home", "End", "PgUp", "PgDn", "Insert", "Delete"};
        char tmp[48];
        size_t n = 0, k;
        if (cap == 0)
            return;
        if (ev->type == TUI_EV_RESIZE)
        {
            n += r2__tui_puts((uint8_t *)tmp, "Resize ");
            n += r2__tui_putu((uint8_t *)tmp + n, (uint32_t)ev->x);
            tmp[n++] = 'x';
            n += r2__tui_putu((uint8_t *)tmp + n, (uint32_t)ev->y);
        }
        else if (ev->type == TUI_EV_KEY)
        {
            if (ev->mods & TUI_MOD_CTRL)
                n += r2__tui_puts((uint8_t *)tmp + n, "Ctrl+");
            if (ev->mods & TUI_MOD_ALT)
                n += r2__tui_puts((uint8_t *)tmp + n, "Alt+");
            if (ev->mods & TUI_MOD_SHIFT)
                n += r2__tui_puts((uint8_t *)tmp + n, "Shift+");
            if (ev->key == TUI_KEY_RUNE)
            {
                if (ev->ch == ' ')
                    n += r2__tui_puts((uint8_t *)tmp + n, "Space");
                else
                    n += utf8_encode(r2__tui_is_control(ev->ch) ? 0xFFFDu : ev->ch, (uint8_t *)tmp + n);
            }
            else if (ev->key >= TUI_KEY_F1 && ev->key <= TUI_KEY_F12)
            {
                tmp[n++] = 'F';
                n += r2__tui_putu((uint8_t *)tmp + n, (uint32_t)(ev->key - TUI_KEY_F1 + 1));
            }
            else if (ev->key < sizeof(names) / sizeof(names[0]))
                n += r2__tui_puts((uint8_t *)tmp + n, names[ev->key]);
        }
        for (k = 0; k < n && k + 1 < cap; k++)
            buf[k] = tmp[k];
        buf[k] = '\0';
    }

    /* ================================================================
       window manager
       ================================================================ */

#define R2__TUI_DESKTOP 0
#define R2__TUI_MSGBOX 1
#define R2__TUI_SEP 0xFFFFu

    enum
    {
        R2__TUI_CMD_TEXT = 1,
        R2__TUI_CMD_FILL,
        R2__TUI_CMD_BOX,
        R2__TUI_CMD_CURSOR,
        R2__TUI_CMD_CLIP
    };

    enum
    {
        R2__TUI_MB_IDLE,
        R2__TUI_MB_OPEN,
        R2__TUI_MB_ANSWERED
    };

    static void r2__tui_layout(tui *t);
    static void r2__tui_mb_idle(tui *t);
    static void r2__tui_settle_focus(tui *t, int slot);
    static void r2__tui_pal_refresh(tui *t, int keep_sel);

    /* copy a string into dst (cap bytes), never cutting a UTF-8 character in half */
    static void r2__tui_copy(char *dst, size_t cap, const char *src)
    {
        size_t i = 0;
        if (cap == 0)
            return;
        while (src && src[i] && i + 1 < cap)
        {
            dst[i] = src[i];
            i++;
        }
        if (src && src[i])
        {
            /* truncated: drop the last character if it is incomplete */
            size_t lead = i;
            while (lead > 0 && ((uint8_t)dst[lead - 1] & 0xC0u) == 0x80u)
                lead--;
            if (lead > 0 && (uint8_t)dst[lead - 1] >= 0xC0u)
            {
                uint8_t c = (uint8_t)dst[lead - 1];
                size_t need = c >= 0xF0u ? 4u : c >= 0xE0u ? 3u : 2u;
                if (i - (lead - 1) < need)
                    i = lead - 1;
            }
        }
        dst[i] = '\0';
    }

    static rune r2__tui_lower(rune r)
    {
        return (r >= 'A' && r <= 'Z') ? r + 32u : r;
    }

    /* the letter after the first ~, lower case; 0 if none */
    static rune r2__tui_hot(const char *s)
    {
        rune r;
        for (; s && *s; s++)
        {
            if (*s == '~' && s[1] && s[1] != '~')
            {
                utf8_decode((const uint8_t *)s + 1, 4, &r);
                return r2__tui_lower(r);
            }
        }
        return 0;
    }

    /* bytes of a label before its "##id" part */
    static uint32_t r2__tui_shown_len(const char *s)
    {
        uint32_t n = 0;
        while (s && s[n] && !(s[n] == '#' && s[n + 1] == '#'))
            n++;
        return n;
    }

    /* width of a label without its ~ marks or "##id" */
    static int r2__tui_label_width(const char *s)
    {
        s8 str = s8_from_bytes(s, r2__tui_shown_len(s));
        uint32_t at = 0;
        rune r;
        int w = 0;
        while (s8_next_rune(str, &at, &r))
            if (r != '~')
                w += r2__tui_is_control(r) ? 1 : tui_rune_width(r);
        return w;
    }

    /* draw a label, ~ toggling between normal and hot style; returns its width */
    static int r2__tui_label(tui *t, int x, int y, const char *s, tui_style normal, tui_style hot)
    {
        s8 str = s8_from_cstr(s);
        uint32_t at = 0;
        rune r;
        int cx = x, is_hot = 0;
        while (s8_next_rune(str, &at, &r))
        {
            if (r == '~')
                is_hot = !is_hot;
            else
                cx += tui_draw_rune(t, cx, y, r, is_hot ? hot : normal);
        }
        return cx - x;
    }

    /* record a label (see r2__tui_label) as text commands; returns its width */
    static int r2__tui_rec_label(tui *t, int x, int y, const char *s, tui_style normal, tui_style hot);

    /* ---------------- shortcuts ---------------- */

    static int r2__tui_word(const char *a, size_t n, const char *b)
    {
        size_t i;
        for (i = 0; i < n; i++)
            if (!b[i] || r2__tui_lower((rune)(uint8_t)a[i]) != r2__tui_lower((rune)(uint8_t)b[i]))
                return 0;
        return b[n] == '\0';
    }

    /* "Ctrl+Shift+F3", "Alt+X", "Del" -> key, mods, ch. 0 if not understood. */
    static int r2__tui_parse_shortcut(const char *s, uint16_t *key, uint8_t *mods, rune *ch)
    {
        static const struct
        {
            const char *name;
            uint16_t key;
        } names[] = {{"Enter", TUI_KEY_ENTER},   {"Tab", TUI_KEY_TAB},      {"Backspace", TUI_KEY_BACKSPACE},
                     {"Esc", TUI_KEY_ESC},       {"Up", TUI_KEY_UP},        {"Down", TUI_KEY_DOWN},
                     {"Left", TUI_KEY_LEFT},     {"Right", TUI_KEY_RIGHT},  {"Home", TUI_KEY_HOME},
                     {"End", TUI_KEY_END},       {"PgUp", TUI_KEY_PGUP},    {"PgDn", TUI_KEY_PGDN},
                     {"Ins", TUI_KEY_INSERT},    {"Insert", TUI_KEY_INSERT}, {"Del", TUI_KEY_DELETE},
                     {"Delete", TUI_KEY_DELETE}, {"Space", TUI_KEY_RUNE}};
        *key = TUI_KEY_NONE;
        *mods = 0;
        *ch = 0;
        if (!s || !*s)
            return 0;
        for (;;)
        {
            const char *e = s;
            size_t n, i;
            while (*e && !(*e == '+' && e != s))
                e++;
            n = (size_t)(e - s);
            if (*e == '+')
            {
                if (r2__tui_word(s, n, "Ctrl"))
                    *mods |= TUI_MOD_CTRL;
                else if (r2__tui_word(s, n, "Alt"))
                    *mods |= TUI_MOD_ALT;
                else if (r2__tui_word(s, n, "Shift"))
                    *mods |= TUI_MOD_SHIFT;
                else
                    return 0;
                s = e + 1;
                continue;
            }
            /* last word: the key */
            if ((s[0] == 'F' || s[0] == 'f') && n >= 2 && n <= 3 && s[1] >= '1' && s[1] <= '9')
            {
                int f = s[1] - '0';
                if (n == 3)
                    f = f * 10 + (s[2] - '0');
                if (f < 1 || f > 12)
                    return 0;
                *key = (uint16_t)(TUI_KEY_F1 + f - 1);
                return 1;
            }
            for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
            {
                if (r2__tui_word(s, n, names[i].name))
                {
                    *key = names[i].key;
                    *ch = names[i].key == TUI_KEY_RUNE ? ' ' : 0;
                    return 1;
                }
            }
            {
                rune r;
                uint32_t used = utf8_decode((const uint8_t *)s, (uint32_t)n, &r);
                if (used != n)
                    return 0;
                *key = TUI_KEY_RUNE;
                *ch = (*mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) ? r2__tui_lower(r) : r;
                return 1;
            }
        }
    }

    static int r2__tui_shortcut_match(const tui_item_rec *it, const tui_event *ev)
    {
        if (it->key == TUI_KEY_NONE || ev->key != it->key || ev->mods != it->mods)
            return 0;
        if (it->key != TUI_KEY_RUNE)
            return 1;
        return (it->mods ? r2__tui_lower(ev->ch) : ev->ch) == it->ch;
    }

    /* ---------------- recorded commands ---------------- */

    static void r2__tui_put16(uint8_t *p, int v)
    {
        p[0] = (uint8_t)((unsigned)v & 0xFFu);
        p[1] = (uint8_t)(((unsigned)v >> 8) & 0xFFu);
    }

    static int r2__tui_get16(const uint8_t *p)
    {
        return (int)(int16_t)(uint16_t)(p[0] | (p[1] << 8));
    }

    /* Throw away slot's list and pack the others to the front of the pool,
       so the running window records at the end */
    static void r2__tui_cmd_begin(tui *t, int slot)
    {
        uint32_t at = 0;
        t->win[slot].cmd_len = 0;
        for (;;)
        {
            int best = -1, i;
            for (i = 0; i < R2__TUI_SLOTS; i++)
            {
                tui_window *w = &t->win[i];
                if (w->cmd_len && w->cmd_off >= at && (best < 0 || w->cmd_off < t->win[best].cmd_off))
                    best = i;
            }
            if (best < 0)
                break;
            {
                tui_window *w = &t->win[best];
                uint32_t k;
                for (k = 0; k < w->cmd_len; k++)
                    t->cmd_pool[at + k] = t->cmd_pool[w->cmd_off + k];
                w->cmd_off = at;
                at += w->cmd_len;
            }
        }
        t->cmd_used = at;
        t->win[slot].cmd_off = at;
    }

    static uint8_t *r2__tui_cmd(tui *t, uint32_t len)
    {
        tui_window *w;
        uint8_t *p;
        if (t->cur < 0)
            return 0;
        w = &t->win[t->cur];
        if (t->cmd_used + len > R2_TUI_CMD_POOL)
        {
            w->overflow = 1;
            return 0;
        }
        p = t->cmd_pool + t->cmd_used;
        t->cmd_used += len;
        w->cmd_len += len;
        return p;
    }

    R2_TUI_DEF void tui_text(tui *t, int x, int y, s8 str, tui_style s)
    {
        uint32_t len = str.len > 0xFFFFu ? 0xFFFFu : str.len;
        uint8_t *p = r2__tui_cmd(t, 9u + len);
        uint32_t i;
        if (!p)
            return;
        p[0] = R2__TUI_CMD_TEXT;
        r2__tui_put16(p + 1, x + t->org_x);
        r2__tui_put16(p + 3, y + t->org_y);
        r2__tui_put16(p + 5, s);
        r2__tui_put16(p + 7, (int)len);
        for (i = 0; i < len; i++)
            p[9 + i] = str.data[i];
    }

    static int r2__tui_rec_label(tui *t, int x, int y, const char *s, tui_style normal, tui_style hot)
    {
        const char *run = s;
        int cx = x, is_hot = 0;
        for (;; s++)
        {
            int end = *s == '\0' || (s[0] == '#' && s[1] == '#');
            if (*s == '~' || end)
            {
                s8 part = s8_from_bytes(run, (uint32_t)(s - run));
                if (part.len)
                    tui_text(t, cx, y, part, is_hot ? hot : normal);
                cx += tui_text_width(part);
                if (end)
                    break;
                is_hot = !is_hot;
                run = s + 1;
            }
        }
        return cx - x;
    }

    R2_TUI_DEF void tui_fill(tui *t, tui_rect r, rune ch, tui_style s)
    {
        uint8_t *p = r2__tui_cmd(t, 15);
        if (!p)
            return;
        p[0] = R2__TUI_CMD_FILL;
        r2__tui_put16(p + 1, r.x + t->org_x);
        r2__tui_put16(p + 3, r.y + t->org_y);
        r2__tui_put16(p + 5, r.w);
        r2__tui_put16(p + 7, r.h);
        r2__tui_put16(p + 9, s);
        r2__tui_put16(p + 11, (int)(ch & 0xFFFFu));
        r2__tui_put16(p + 13, (int)(ch >> 16));
    }

    R2_TUI_DEF void tui_box(tui *t, tui_rect r, tui_style s, int kind)
    {
        uint8_t *p = r2__tui_cmd(t, 12);
        if (!p)
            return;
        p[0] = R2__TUI_CMD_BOX;
        r2__tui_put16(p + 1, r.x + t->org_x);
        r2__tui_put16(p + 3, r.y + t->org_y);
        r2__tui_put16(p + 5, r.w);
        r2__tui_put16(p + 7, r.h);
        r2__tui_put16(p + 9, s);
        p[11] = (uint8_t)kind;
    }

    R2_TUI_DEF void tui_cursor(tui *t, int x, int y)
    {
        uint8_t *p = r2__tui_cmd(t, 5);
        if (!p)
            return;
        p[0] = R2__TUI_CMD_CURSOR;
        r2__tui_put16(p + 1, x + t->org_x);
        r2__tui_put16(p + 3, y + t->org_y);
    }

    /* limit the following commands to r (inside coordinates); w < 0 lifts it */
    static void r2__tui_clip_cmd(tui *t, int x, int y, int w, int h)
    {
        uint8_t *p = r2__tui_cmd(t, 9);
        if (!p)
            return;
        p[0] = R2__TUI_CMD_CLIP;
        r2__tui_put16(p + 1, x);
        r2__tui_put16(p + 3, y);
        r2__tui_put16(p + 5, w);
        r2__tui_put16(p + 7, h);
    }

    static tui_rect r2__tui_isect(tui_rect a, tui_rect b)
    {
        int x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
        int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
        int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
        tui_rect r = {x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0};
        return r;
    }

    /* inside of a window on screen */
    static tui_rect r2__tui_client(const tui *t, int slot)
    {
        tui_rect f = t->win[slot].frame;
        if (slot == R2__TUI_DESKTOP)
            return t->desk;
        f.x += 1;
        f.y += 1;
        f.w -= 2;
        f.h -= 2;
        if (f.w < 0)
            f.w = 0;
        if (f.h < 0)
            f.h = 0;
        return f;
    }

    static tui_style r2__tui_body(const tui *t, int slot)
    {
        if (slot == R2__TUI_DESKTOP)
            return t->theme->desktop;
        return (t->win[slot].flags & TUI_WIN_DIALOG) ? t->theme->dialog_body : t->theme->body;
    }

    /* Draw slot's recorded commands; returns 1 and sets *cx,*cy if it placed the cursor */
    static int r2__tui_replay(tui *t, int slot, int *cx, int *cy)
    {
        tui_window *w = &t->win[slot];
        tui_rect c = r2__tui_client(t, slot);
        tui_style body = r2__tui_body(t, slot);
        const uint8_t *p = t->cmd_pool + w->cmd_off;
        const uint8_t *end = p + w->cmd_len;
        int has_cursor = 0;
        tui_rect cr = c;
        while (p < end)
        {
            int x = c.x + r2__tui_get16(p + 1), y = c.y + r2__tui_get16(p + 3);
            tui_clip(t, cr);
            switch (p[0])
            {
            case R2__TUI_CMD_TEXT:
            {
                tui_style s = (tui_style)r2__tui_get16(p + 5);
                uint32_t len = (uint32_t)(uint16_t)r2__tui_get16(p + 7);
                tui_draw_text(t, x, y, s8_from_bytes(p + 9, len), s == TUI_DEFAULT ? body : s);
                p += 9 + len;
                break;
            }
            case R2__TUI_CMD_FILL:
            {
                tui_style s = (tui_style)r2__tui_get16(p + 9);
                tui_rect r = {x, y, r2__tui_get16(p + 5), r2__tui_get16(p + 7)};
                rune ch = (rune)(uint16_t)r2__tui_get16(p + 11) | ((rune)(uint16_t)r2__tui_get16(p + 13) << 16);
                tui_draw_fill(t, r, ch, s == TUI_DEFAULT ? body : s);
                p += 15;
                break;
            }
            case R2__TUI_CMD_BOX:
            {
                tui_style s = (tui_style)r2__tui_get16(p + 9);
                tui_rect r = {x, y, r2__tui_get16(p + 5), r2__tui_get16(p + 7)};
                tui_draw_box(t, r, s == TUI_DEFAULT ? body : s, p[11]);
                p += 12;
                break;
            }
            case R2__TUI_CMD_CLIP:
            {
                tui_rect r = {x, y, r2__tui_get16(p + 5), r2__tui_get16(p + 7)};
                cr = r.w < 0 ? c : r2__tui_isect(c, r);
                p += 9;
                break;
            }
            case R2__TUI_CMD_CURSOR:
                if (x >= cr.x && y >= cr.y && x < cr.x + cr.w && y < cr.y + cr.h)
                {
                    *cx = x;
                    *cy = y;
                    has_cursor = 1;
                }
                p += 5;
                break;
            default:
                p = end;
            }
        }
        tui_clip_reset(t);
        return has_cursor;
    }

    /* ---------------- windows ---------------- */

    static int r2__tui_slot(const tui *t, const tui_window *w)
    {
        if (!w || w < t->win || w >= t->win + R2__TUI_SLOTS)
            return -1;
        return (int)(w - t->win);
    }

    static int r2__tui_is_modal(const tui *t, int slot)
    {
        return slot >= 0 && t->win[slot].used && (t->win[slot].flags & TUI_WIN_MODAL);
    }

    /* topmost modal window, or -1 */
    static int r2__tui_modal(const tui *t)
    {
        int i;
        for (i = (int)t->nz - 1; i >= 0; i--)
            if (r2__tui_is_modal(t, t->z[i]))
                return t->z[i];
        return -1;
    }

    static void r2__tui_z_remove(tui *t, int slot)
    {
        int i, k = 0;
        for (i = 0; i < (int)t->nz; i++)
            if (t->z[i] != slot)
                t->z[k++] = t->z[i];
        t->nz = (uint8_t)k;
    }

    /* on top of the floating windows (but under any modal unless it is one) */
    static void r2__tui_raise(tui *t, int slot)
    {
        int at, i;
        r2__tui_z_remove(t, slot);
        at = (int)t->nz;
        if (!r2__tui_is_modal(t, slot))
            while (at > 0 && r2__tui_is_modal(t, t->z[at - 1]))
                at--;
        for (i = (int)t->nz; i > at; i--)
            t->z[i] = t->z[i - 1];
        t->z[at] = (uint8_t)slot;
        t->nz++;
    }

    static void r2__tui_focus(tui *t, int slot)
    {
        if (slot >= 0 && t->win[slot].hidden)
        {
            t->win[slot].hidden = 0; /* focusing a minimized window restores it */
            t->layout_dirty = 1;
        }
        if (slot >= 0 && t->win[slot].used && !t->win[slot].dock)
            r2__tui_raise(t, slot);
        if (slot == t->focus)
            return;
        if (t->focus >= 0 && t->win[t->focus].used)
            t->win[t->focus].dirty = 1;
        t->focus = slot;
        if (t->rec_bar == t->bar_win)
            t->bar_stale = 1; /* a controller is still declaring into it */
        else
            t->bar_win->nmenus = t->bar_win->nitems = 0;
        if (slot >= 0)
            t->win[slot].dirty = 1;
        t->compose_dirty = 1;
    }

    /* best window to focus after the focused one goes away */
    static int r2__tui_next_focus(const tui *t)
    {
        int i;
        for (i = (int)t->nz - 1; i >= 0; i--)
            if (t->win[t->z[i]].used && !t->win[t->z[i]].dead && !t->win[t->z[i]].hidden)
                return t->z[i];
        for (i = 2; i < R2__TUI_SLOTS; i++)
            if (t->win[i].used && !t->win[i].dead && !t->win[i].hidden)
                return i;
        return -1;
    }

    static void r2__tui_finish_close(tui *t, int slot)
    {
        tui_window *w = &t->win[slot];
        w->used = 0;
        w->dead = 0;
        w->cmd_len = 0;
        w->number = 0;
        r2__tui_z_remove(t, slot);
        if (slot == R2__TUI_MSGBOX && t->mb_state == R2__TUI_MB_OPEN)
            r2__tui_mb_idle(t);
        if (t->focus == slot)
        {
            t->focus = -1;
            r2__tui_focus(t, r2__tui_next_focus(t));
        }
        if (t->moving && t->focus != slot)
            t->moving = 0;
        t->layout_dirty = 1;
        t->compose_dirty = 1;
    }

    R2_TUI_DEF void tui_window_close(tui *t, tui_window *w)
    {
        int slot = r2__tui_slot(t, w);
        if (slot < 0 || !w->used)
            return;
        if (slot == t->cur)
            w->dead = 1; /* finish when its controller returns */
        else
            r2__tui_finish_close(t, slot);
    }

    static int r2__tui_open_slot(tui *t, int slot, const char *title, tui_rect r, unsigned flags, tui_ui_fn ui,
                                 void *user)
    {
        tui_window *w = &t->win[slot];
        int i, n;
        uint8_t taken[10] = {0};
        if (t->layout_dirty || t->work.w == 0)
            r2__tui_layout(t);
        for (i = 0; i < (int)sizeof(*w); i++)
            ((uint8_t *)w)[i] = 0;
        w->used = 1;
        w->dirty = 1;
        w->flags = (uint16_t)flags;
        w->ui = ui;
        w->user = user;
        r2__tui_copy(w->title, sizeof(w->title), title);
        /* lowest free window number 1..9 */
        for (i = 2; i < R2__TUI_SLOTS; i++)
            if (i != slot && t->win[i].used && t->win[i].number < 10)
                taken[t->win[i].number] = 1;
        for (n = 1; slot >= 2 && n < 10; n++)
            if (!taken[n])
            {
                w->number = (uint8_t)n;
                break;
            }
        /* size and place */
        if (r.w <= 0 || r.h <= 0)
        {
            r.w = t->work.w > 50 ? 50 : t->work.w;
            r.h = t->work.h > 14 ? 14 : t->work.h;
        }
        if (flags & TUI_WIN_DIALOG)
        {
            r.x = t->desk.x + (t->desk.w - r.w) / 2;
            r.y = t->desk.y + (t->desk.h - r.h) / 2;
        }
        else if (r.x < 0 || r.y < 0)
        {
            int k = (w->number ? w->number - 1 : 0) % 8;
            r.x = t->work.x + 2 * k;
            r.y = t->work.y + k;
        }
        w->rect = r;
        w->frame = r;
        r2__tui_raise(t, slot);
        r2__tui_focus(t, slot);
        t->layout_dirty = 1;
        return slot;
    }

    R2_TUI_DEF tui_window *tui_window_open(tui *t, const char *title, tui_rect r, unsigned flags, tui_ui_fn ui,
                                           void *user)
    {
        int i;
        for (i = 2; i < R2__TUI_SLOTS; i++)
            if (!t->win[i].used)
                return &t->win[r2__tui_open_slot(t, i, title, r, flags, ui, user)];
        return 0;
    }

    R2_TUI_DEF void tui_window_dock(tui *t, tui_window *w, int side, int size)
    {
        int slot = r2__tui_slot(t, w);
        if (slot < 2 || !w->used || (w->flags & TUI_WIN_MODAL))
            return;
        if (side == TUI_DOCK_NONE)
        {
            if (w->dock)
                r2__tui_raise(t, slot);
        }
        else
            r2__tui_z_remove(t, slot);
        w->dock = (uint8_t)side;
        if (size > 0)
            w->dock_size = size;
        w->zoomed = 0;
        w->dirty = 1;
        t->layout_dirty = 1;
        t->compose_dirty = 1;
    }

    R2_TUI_DEF void tui_window_focus(tui *t, tui_window *w)
    {
        int slot = r2__tui_slot(t, w);
        int modal = r2__tui_modal(t);
        if (slot < 0 || !w->used || (modal >= 0 && slot != modal))
            return;
        r2__tui_focus(t, slot);
    }

    R2_TUI_DEF void tui_window_invalidate(tui *t, tui_window *w)
    {
        if (w && w->used)
            w->dirty = 1;
        (void)t;
    }

    R2_TUI_DEF void tui_window_set_title(tui *t, tui_window *w, const char *title)
    {
        r2__tui_copy(w->title, sizeof(w->title), title);
        t->compose_dirty = 1;
    }

    R2_TUI_DEF tui_window *tui_focused_window(tui *t)
    {
        return t->focus >= 0 ? &t->win[t->focus] : 0;
    }

    R2_TUI_DEF void tui_set_desktop(tui *t, tui_ui_fn ui, void *user)
    {
        tui_window *w = &t->win[R2__TUI_DESKTOP];
        w->used = ui != 0;
        w->ui = ui;
        w->user = user;
        w->dirty = 1;
        w->cmd_len = 0;
        t->compose_dirty = 1;
    }

    R2_TUI_DEF void tui_set_theme(tui *t, const tui_theme *theme)
    {
        int i;
        t->theme = theme ? theme : &r2__tui_theme_borland;
        for (i = 0; i < R2__TUI_SLOTS; i++)
            t->win[i].dirty = t->win[i].used; /* widgets record colours when they run */
        t->compose_dirty = 1;
    }

    R2_TUI_DEF int tui_set_prefix(tui *t, const char *shortcut)
    {
        uint16_t key;
        uint8_t mods;
        rune ch;
        t->chord = 0;
        if (!shortcut || !*shortcut)
        {
            t->prefix_key = TUI_KEY_NONE;
            t->prefix_name[0] = '\0';
            t->compose_dirty = 1;
            return 1;
        }
        if (!r2__tui_parse_shortcut(shortcut, &key, &mods, &ch))
            return 0;
        t->prefix_key = key;
        t->prefix_mods = mods;
        t->prefix_ch = ch;
        r2__tui_copy(t->prefix_name, sizeof(t->prefix_name), shortcut);
        t->compose_dirty = 1;
        return 1;
    }

    R2_TUI_DEF tui_rect tui_work_area(const tui *t)
    {
        return t->work;
    }

    /* ---------------- layout ---------------- */

    static int r2__tui_clampi(int v, int lo, int hi)
    {
        return v < lo ? lo : v > hi ? hi : v;
    }

    static void r2__tui_layout(tui *t)
    {
        int side, i, lw = 0, rw = 0, bh = 0;
        int count[4] = {0, 0, 0, 0};
        t->desk.x = 0;
        t->desk.y = 1;
        t->desk.w = t->cols;
        t->desk.h = t->rows > 2 ? t->rows - 2 : 0;
        for (i = 2; i < R2__TUI_SLOTS; i++)
        {
            tui_window *w = &t->win[i];
            if (!w->used || !w->dock || w->hidden)
                continue;
            count[w->dock]++;
            if (w->dock == TUI_DOCK_LEFT && w->dock_size > lw)
                lw = w->dock_size;
            if (w->dock == TUI_DOCK_RIGHT && w->dock_size > rw)
                rw = w->dock_size;
            if (w->dock == TUI_DOCK_BOTTOM && w->dock_size > bh)
                bh = w->dock_size;
        }
        if (count[TUI_DOCK_LEFT])
            lw = r2__tui_clampi(lw ? lw : 20, 6, t->desk.w / 3);
        if (count[TUI_DOCK_RIGHT])
            rw = r2__tui_clampi(rw ? rw : 20, 6, t->desk.w / 3);
        if (count[TUI_DOCK_BOTTOM])
            bh = r2__tui_clampi(bh ? bh : 6, 3, t->desk.h / 2);
        t->work.x = t->desk.x + lw;
        t->work.y = t->desk.y;
        t->work.w = t->desk.w - lw - rw;
        t->work.h = t->desk.h - bh;
        if (t->work.w < 0)
            t->work.w = 0;
        if (t->work.h < 0)
            t->work.h = 0;

        /* docked windows share their side equally */
        for (side = TUI_DOCK_LEFT; side <= TUI_DOCK_BOTTOM; side++)
        {
            int k = 0, n = count[side];
            for (i = 2; i < R2__TUI_SLOTS && n; i++)
            {
                tui_window *w = &t->win[i];
                tui_rect f;
                if (!w->used || w->dock != side || w->hidden)
                    continue;
                if (side == TUI_DOCK_BOTTOM)
                {
                    int span = t->work.w;
                    f.x = t->work.x + span * k / n;
                    f.w = t->work.x + span * (k + 1) / n - f.x;
                    f.y = t->desk.y + t->desk.h - bh;
                    f.h = bh;
                }
                else
                {
                    f.x = side == TUI_DOCK_LEFT ? t->desk.x : t->desk.x + t->desk.w - rw;
                    f.w = side == TUI_DOCK_LEFT ? lw : rw;
                    f.y = t->desk.y + t->desk.h * k / n;
                    f.h = t->desk.y + t->desk.h * (k + 1) / n - f.y;
                }
                if (f.w != w->frame.w || f.h != w->frame.h)
                    w->dirty = 1;
                w->frame = f;
                k++;
            }
        }

        /* floating windows stay inside the work area (dialogs: the desktop) */
        for (i = 1; i < R2__TUI_SLOTS; i++)
        {
            tui_window *w = &t->win[i];
            tui_rect area = (w->flags & TUI_WIN_DIALOG) ? t->desk : t->work;
            tui_rect f;
            if (!w->used || w->dock)
                continue;
            if (w->zoomed)
                w->rect = area;
            f = w->rect;
            f.w = r2__tui_clampi(f.w, 1, area.w > 1 ? area.w : 1);
            f.h = r2__tui_clampi(f.h, 1, area.h > 1 ? area.h : 1);
            f.x = r2__tui_clampi(f.x, area.x, area.x + area.w - f.w);
            f.y = r2__tui_clampi(f.y, area.y, area.y + area.h - f.h);
            w->rect = f;
            if (f.w != w->frame.w || f.h != w->frame.h)
                w->dirty = 1;
            w->frame = f;
        }
        t->layout_dirty = 0;
        t->compose_dirty = 1;
    }

    /* ---------------- controllers ---------------- */

    static void r2__tui_run(tui *t, int slot, const tui_event *ev)
    {
        tui_window *w = &t->win[slot];
        tui_menubar *bar = 0;
        int chosen;
        if (!w->used || !w->ui)
        {
            w->dirty = 0;
            return;
        }
        if (slot == t->focus)
            bar = t->bar_win;
        else if (slot == R2__TUI_DESKTOP)
            bar = t->bar_desk;
        if (bar)
        {
            bar->nmenus = bar->nitems = 0;
            bar->ntext = 0;
            bar->open = -1;
            bar->full = 0;
        }
        chosen = t->menu_act >= 0;
        t->rec_bar = bar;
        t->item_seq = 0;
        t->item_seq_on = 0;
        t->cur = slot;
        t->cur_ev = ev;
        t->ev_used = 0;
        w->status[0] = '\0';
        w->overflow = 0;
        w->dirty = 0;
        w->runs++;
        t->nwid = 0;
        t->lay_x = t->lay_y = t->lay_lx = t->lay_ly = t->lay_lw = t->lay_same = 0;
        t->org_x = t->org_y = 0;
        t->sv_on = 0;
        t->frame++;
        r2__tui_cmd_begin(t, slot);
        w->ui(t, w, w->user);
        r2__tui_settle_focus(t, slot);
        w->closing = 0;
        /* an answer its asker didn't collect is dropped */
        if (t->mb_state == R2__TUI_MB_ANSWERED && t->mb_owner == slot)
            r2__tui_mb_idle(t);
        t->cur = -1;
        t->cur_ev = 0;
        t->rec_bar = 0;
        if (t->bar_stale)
        {
            t->bar_win->nmenus = t->bar_win->nitems = 0;
            t->bar_stale = 0;
        }
        if (t->pal_open)
            r2__tui_pal_refresh(t, 1);
        t->compose_dirty = 1;
        /* A key or a chosen menu item may have changed things this run
           already declared or drew before handling it (menu items enabled,
           the status line, ...): run once more, so it all shows the new state. */
        if ((ev && ev->type == TUI_EV_KEY) || chosen)
            w->dirty = 1;
        if (w->dead)
            r2__tui_finish_close(t, slot);
    }

    R2_TUI_DEF void tui_size(tui *t, int *w, int *h)
    {
        tui_rect c = {0, 0, 0, 0};
        if (t->cur >= 0)
            c = r2__tui_client(t, t->cur);
        if (w)
            *w = c.w;
        if (h)
            *h = c.h;
    }

    R2_TUI_DEF const tui_event *tui_key(tui *t)
    {
        return (t->cur_ev && !t->ev_used && t->cur_ev->type == TUI_EV_KEY) ? t->cur_ev : 0;
    }

    R2_TUI_DEF void tui_use_key(tui *t)
    {
        t->ev_used = 1;
    }

    R2_TUI_DEF int tui_focused(tui *t)
    {
        return t->cur >= 0 && t->cur == t->focus;
    }

    R2_TUI_DEF void tui_status(tui *t, const char *text)
    {
        if (t->cur >= 0)
            r2__tui_copy(t->win[t->cur].status, R2_TUI_STATUS_MAX, text);
    }

    R2_TUI_DEF int tui_closing(tui *t)
    {
        return t->cur >= 0 && t->win[t->cur].closing;
    }

    /* ---------------- menus (declared by controllers) ---------------- */

    static uint16_t r2__tui_bar_text(tui_menubar *bar, const char *s)
    {
        uint16_t at = bar->ntext;
        size_t n = 0;
        while (s && s[n])
            n++;
        if (at + n + 1 > R2_TUI_MENU_TEXT)
            return R2__TUI_SEP;
        for (size_t i = 0; i < n; i++)
            bar->text[at + i] = s[i];
        bar->text[at + n] = '\0';
        bar->ntext = (uint16_t)(at + n + 1);
        return at;
    }

    R2_TUI_DEF void tui_menu_begin(tui *t, const char *title)
    {
        tui_menubar *bar = t->rec_bar;
        uint16_t off;
        t->item_seq_on = 1;
        if (!bar || bar->full)
            return;
        off = bar->nmenus < R2_TUI_MENU_MAX ? r2__tui_bar_text(bar, title) : R2__TUI_SEP;
        if (off == R2__TUI_SEP)
        {
            bar->full = 1;
            return;
        }
        bar->menu[bar->nmenus].title = off;
        bar->menu[bar->nmenus].first = bar->nitems;
        bar->menu[bar->nmenus].count = 0;
        bar->open = (int8_t)bar->nmenus;
        bar->nmenus++;
    }

    R2_TUI_DEF void tui_menu_end(tui *t)
    {
        t->item_seq_on = 0;
        if (t->rec_bar)
            t->rec_bar->open = -1;
    }

    static int r2__tui_add_item(tui *t, const char *label, const char *shortcut, unsigned flags, int sep)
    {
        tui_menubar *bar = t->rec_bar;
        tui_item_rec *it;
        int idx;
        if (!bar || bar->full || bar->open < 0)
            return -1;
        if (bar->nitems >= R2_TUI_MENU_ITEMS)
        {
            bar->full = 1;
            return -1;
        }
        idx = bar->nitems;
        it = &bar->item[idx];
        it->flags = (uint8_t)flags;
        it->key = TUI_KEY_NONE;
        it->mods = 0;
        it->ch = 0;
        it->shortcut = R2__TUI_SEP;
        if (sep)
            it->label = R2__TUI_SEP;
        else
        {
            it->label = r2__tui_bar_text(bar, label);
            if (it->label == R2__TUI_SEP)
            {
                bar->full = 1;
                return -1;
            }
            if (shortcut && *shortcut)
            {
                it->shortcut = r2__tui_bar_text(bar, shortcut);
                if (it->shortcut == R2__TUI_SEP)
                {
                    bar->full = 1;
                    return -1;
                }
                r2__tui_parse_shortcut(shortcut, &it->key, &it->mods, &it->ch);
            }
        }
        bar->nitems++;
        bar->menu[bar->open].count++;
        return idx;
    }

    R2_TUI_DEF int tui_menu_item_flags(tui *t, const char *label, const char *shortcut, unsigned flags)
    {
        /* chosen items are identified by their position in the run (which
           is also their index in the bar, as long as the bar holds them) */
        int seq;
        if (!t->item_seq_on)
            return 0; /* not inside tui_menu_begin / end */
        seq = t->item_seq++;
        r2__tui_add_item(t, label, shortcut, flags, 0);
        return seq == t->menu_act && !(flags & TUI_ITEM_DISABLED);
    }

    R2_TUI_DEF int tui_menu_item(tui *t, const char *label, const char *shortcut)
    {
        return tui_menu_item_flags(t, label, shortcut, 0);
    }

    R2_TUI_DEF void tui_menu_separator(tui *t)
    {
        if (!t->item_seq_on)
            return;
        t->item_seq++;
        r2__tui_add_item(t, 0, 0, 0, 1);
    }

    /* ---------------- the menu bar (drawing and keys) ---------------- */

    /* the menus on the bar right now: the focused window's, or the desktop's */
    static tui_menubar *r2__tui_bar(const tui *t, int *owner)
    {
        if (t->focus >= 0 && t->bar_win->nmenus > 0)
        {
            if (owner)
                *owner = t->focus;
            return t->bar_win;
        }
        if (owner)
            *owner = t->win[R2__TUI_DESKTOP].used ? R2__TUI_DESKTOP : -1;
        return t->bar_desk;
    }

    enum
    {
        R2__TUI_SYS_MOVE,
        R2__TUI_SYS_ZOOM,
        R2__TUI_SYS_MIN,
        R2__TUI_SYS_NEXT,
        R2__TUI_SYS_CLOSE,
        R2__TUI_SYS_SEP1,
        R2__TUI_SYS_DOCK_LEFT,
        R2__TUI_SYS_DOCK_RIGHT,
        R2__TUI_SYS_DOCK_BOTTOM,
        R2__TUI_SYS_FLOAT,
        R2__TUI_SYS_SEP2,
        R2__TUI_SYS_WINDOWS /* then one entry per window */
    };

    static const char *const r2__tui_sys_label[] = {"~M~ove/Size", "~Z~oom", "M~i~nimize", "~N~ext", "~C~lose", 0,
                                                    "Dock ~L~eft", "Dock ~R~ight", "Dock ~B~ottom", "~F~loat", 0};
    /* the key shown beside each system menu entry: the prefix chord, or the F-key without a prefix */
    static const char *const r2__tui_sys_chord[] = {"Enter", "\xE2\x86\x91", "\xE2\x86\x93", "Tab", "Bksp"};
    static const char *const r2__tui_sys_fkey[] = {"Ctrl+F5", "F5", 0, "F6", "Alt+F3"};

    static const char *r2__tui_sys_key(const tui *t, int item, char *buf, size_t cap)
    {
        size_t n = 0, k;
        if (item < 0 || item > R2__TUI_SYS_CLOSE)
            return 0;
        if (!t->prefix_key)
            return r2__tui_sys_fkey[item];
        for (k = 0; t->prefix_name[k] && n + 1 < cap; k++)
            buf[n++] = t->prefix_name[k];
        if (n + 1 < cap)
            buf[n++] = ' ';
        for (k = 0; r2__tui_sys_chord[item][k] && n + 1 < cap; k++)
            buf[n++] = r2__tui_sys_chord[item][k];
        buf[n] = '\0';
        return buf;
    }

    /* system menu entries: fills slots[] with window slots for the window list */
    static int r2__tui_sys_count(const tui *t, int *slots)
    {
        int i, n = 0;
        for (i = 2; i < R2__TUI_SLOTS; i++)
            if (t->win[i].used && t->win[i].number)
                slots[n++] = i;
        return R2__TUI_SYS_WINDOWS + n;
    }

    static int r2__tui_sys_disabled(const tui *t, int item)
    {
        const tui_window *w = t->focus >= 0 ? &t->win[t->focus] : 0;
        int user = w && t->focus >= 2 && !(w->flags & (TUI_WIN_MODAL | TUI_WIN_DIALOG));
        switch (item)
        {
        case R2__TUI_SYS_MOVE: return !user;
        case R2__TUI_SYS_ZOOM: return !user || w->dock;
        case R2__TUI_SYS_MIN: return !user;
        case R2__TUI_SYS_NEXT: return 0;
        case R2__TUI_SYS_CLOSE: return !user || (w->flags & TUI_WIN_NO_CLOSE);
        case R2__TUI_SYS_DOCK_LEFT: return !user || w->dock == TUI_DOCK_LEFT;
        case R2__TUI_SYS_DOCK_RIGHT: return !user || w->dock == TUI_DOCK_RIGHT;
        case R2__TUI_SYS_DOCK_BOTTOM: return !user || w->dock == TUI_DOCK_BOTTOM;
        case R2__TUI_SYS_FLOAT: return !user || !w->dock;
        default: return 0;
        }
    }

    /* number of entries in menu m (0 = system) and whether entry i is selectable */
    static int r2__tui_menu_count(const tui *t, int m)
    {
        int slots[R2__TUI_SLOTS];
        if (m == 0)
            return r2__tui_sys_count(t, slots);
        return r2__tui_bar(t, 0)->menu[m - 1].count;
    }

    static int r2__tui_selectable(const tui *t, int m, int i)
    {
        if (m == 0)
            return i != R2__TUI_SYS_SEP1 && i != R2__TUI_SYS_SEP2 && !r2__tui_sys_disabled(t, i);
        {
            const tui_menubar *bar = r2__tui_bar(t, 0);
            const tui_item_rec *it = &bar->item[bar->menu[m - 1].first + i];
            return it->label != R2__TUI_SEP && !(it->flags & TUI_ITEM_DISABLED);
        }
    }

    static void r2__tui_menu_step(tui *t, int dir)
    {
        int n = r2__tui_menu_count(t, t->menu_idx), k;
        for (k = 0; k < n; k++)
        {
            t->menu_sel = (t->menu_sel + dir + n) % n;
            if (r2__tui_selectable(t, t->menu_idx, t->menu_sel))
                return;
        }
    }

    static void r2__tui_menu_open(tui *t, int m)
    {
        int n = 1 + r2__tui_bar(t, 0)->nmenus;
        t->menu_open = 1;
        t->menu_idx = (m % n + n) % n;
        t->menu_sel = -1;
        r2__tui_menu_step(t, 1);
        t->compose_dirty = 1;
    }

    static void r2__tui_menu_close(tui *t)
    {
        t->menu_open = 0;
        t->compose_dirty = 1;
    }

    static void r2__tui_start_move(tui *t)
    {
        tui_window *w;
        if (t->focus < 2)
            return;
        w = &t->win[t->focus];
        if (w->zoomed)
        {
            w->zoomed = 0;
            w->rect = w->saved;
            t->layout_dirty = 1;
        }
        t->moving = 1;
        t->move_saved = w->rect;
        t->move_saved_dock = w->dock_size;
        t->compose_dirty = 1;
    }

    static void r2__tui_zoom(tui *t)
    {
        tui_window *w;
        if (t->focus < 2)
            return;
        w = &t->win[t->focus];
        if (w->dock || (w->flags & (TUI_WIN_NO_RESIZE | TUI_WIN_DIALOG)))
            return;
        if (w->zoomed)
            w->rect = w->saved;
        else
            w->saved = w->rect;
        w->zoomed = !w->zoomed;
        t->layout_dirty = 1;
    }

    static void r2__tui_minimize(tui *t)
    {
        tui_window *w;
        if (t->focus < 2)
            return;
        w = &t->win[t->focus];
        if (w->flags & (TUI_WIN_MODAL | TUI_WIN_DIALOG))
            return;
        w->hidden = 1;
        t->layout_dirty = 1;
        r2__tui_focus(t, r2__tui_next_focus(t));
    }

    static void r2__tui_request_close(tui *t)
    {
        tui_window *w;
        if (t->focus < 2)
            return;
        w = &t->win[t->focus];
        if (w->flags & TUI_WIN_NO_CLOSE)
            return;
        if (w->flags & TUI_WIN_ASK_CLOSE)
        {
            w->closing = 1;
            w->dirty = 1;
        }
        else
            r2__tui_finish_close(t, t->focus);
    }

    /* focus the next (dir 1) or previous window by number */
    static void r2__tui_cycle(tui *t, int dir)
    {
        int best = -1, best_key = 0, i;
        int cur = (t->focus >= 2) ? t->win[t->focus].number * 16 + t->focus : -1;
        for (i = 2; i < R2__TUI_SLOTS; i++)
        {
            int key;
            if (!t->win[i].used || i == t->focus || t->win[i].hidden)
                continue;
            key = t->win[i].number * 16 + i;
            /* distance going round in direction dir */
            {
                int d = dir > 0 ? key - cur : cur - key;
                if (d <= 0)
                    d += 100000;
                if (best < 0 || d < best_key)
                {
                    best = i;
                    best_key = d;
                }
            }
        }
        if (best >= 0)
            r2__tui_focus(t, best);
    }

    static void r2__tui_sys_do(tui *t, int item)
    {
        int slots[R2__TUI_SLOTS];
        tui_window *w = t->focus >= 0 ? &t->win[t->focus] : 0;
        switch (item)
        {
        case R2__TUI_SYS_MOVE: r2__tui_start_move(t); break;
        case R2__TUI_SYS_ZOOM: r2__tui_zoom(t); break;
        case R2__TUI_SYS_MIN: r2__tui_minimize(t); break;
        case R2__TUI_SYS_NEXT: r2__tui_cycle(t, 1); break;
        case R2__TUI_SYS_CLOSE: r2__tui_request_close(t); break;
        case R2__TUI_SYS_DOCK_LEFT: tui_window_dock(t, w, TUI_DOCK_LEFT, 0); break;
        case R2__TUI_SYS_DOCK_RIGHT: tui_window_dock(t, w, TUI_DOCK_RIGHT, 0); break;
        case R2__TUI_SYS_DOCK_BOTTOM: tui_window_dock(t, w, TUI_DOCK_BOTTOM, 0); break;
        case R2__TUI_SYS_FLOAT: tui_window_dock(t, w, TUI_DOCK_NONE, 0); break;
        default:
            if (item >= R2__TUI_SYS_WINDOWS && item < r2__tui_sys_count(t, slots))
                r2__tui_focus(t, slots[item - R2__TUI_SYS_WINDOWS]);
        }
    }

    /* run the owner of the bar so its tui_menu_item for item idx returns 1 */
    static void r2__tui_activate(tui *t, int idx)
    {
        int owner;
        r2__tui_bar(t, &owner);
        if (owner < 0)
            return;
        t->menu_act = idx;
        r2__tui_run(t, owner, 0);
        t->menu_act = -1;
    }

    static void r2__tui_menu_choose(tui *t)
    {
        int m = t->menu_idx, sel = t->menu_sel;
        if (sel < 0 || !r2__tui_selectable(t, m, sel))
            return;
        r2__tui_menu_close(t);
        if (m == 0)
            r2__tui_sys_do(t, sel);
        else
            r2__tui_activate(t, r2__tui_bar(t, 0)->menu[m - 1].first + sel);
    }

    /* menu whose title has hot letter c (0 is the system menu), or -1 */
    static int r2__tui_menu_by_hot(const tui *t, rune c)
    {
        const tui_menubar *bar = r2__tui_bar(t, 0);
        int m;
        for (m = 0; m < bar->nmenus; m++)
            if (r2__tui_hot(bar->text + bar->menu[m].title) == r2__tui_lower(c))
                return m + 1;
        return -1;
    }

    static const char *r2__tui_item_label(const tui *t, int m, int i)
    {
        if (m == 0)
        {
            int slots[R2__TUI_SLOTS];
            r2__tui_sys_count(t, slots);
            return i < R2__TUI_SYS_WINDOWS ? r2__tui_sys_label[i] : t->win[slots[i - R2__TUI_SYS_WINDOWS]].title;
        }
        {
            const tui_menubar *bar = r2__tui_bar(t, 0);
            const tui_item_rec *it = &bar->item[bar->menu[m - 1].first + i];
            return it->label == R2__TUI_SEP ? 0 : bar->text + it->label;
        }
    }

    static void r2__tui_menu_key(tui *t, const tui_event *ev)
    {
        int n = 1 + r2__tui_bar(t, 0)->nmenus;
        if (ev->type != TUI_EV_KEY)
            return;
        switch (ev->key)
        {
        case TUI_KEY_ESC: r2__tui_menu_close(t); return;
        case TUI_KEY_LEFT: r2__tui_menu_open(t, t->menu_idx - 1 + n); return;
        case TUI_KEY_RIGHT: r2__tui_menu_open(t, t->menu_idx + 1); return;
        case TUI_KEY_UP: r2__tui_menu_step(t, -1); t->compose_dirty = 1; return;
        case TUI_KEY_DOWN: r2__tui_menu_step(t, 1); t->compose_dirty = 1; return;
        case TUI_KEY_HOME: t->menu_sel = -1; r2__tui_menu_step(t, 1); t->compose_dirty = 1; return;
        case TUI_KEY_END: t->menu_sel = 0; r2__tui_menu_step(t, -1); t->compose_dirty = 1; return;
        case TUI_KEY_ENTER: r2__tui_menu_choose(t); return;
        case TUI_KEY_F1 + 9: r2__tui_menu_close(t); return;
        case TUI_KEY_RUNE:
        {
            int i, count = r2__tui_menu_count(t, t->menu_idx);
            if (ev->mods & TUI_MOD_ALT)
            {
                int m = ev->ch == ' ' ? 0 : r2__tui_menu_by_hot(t, ev->ch);
                if (m >= 0)
                    r2__tui_menu_open(t, m);
                return;
            }
            for (i = 0; i < count; i++)
            {
                const char *label = r2__tui_item_label(t, t->menu_idx, i);
                rune hot = 0;
                if (t->menu_idx == 0 && i >= R2__TUI_SYS_WINDOWS)
                {
                    int slots[R2__TUI_SLOTS];
                    r2__tui_sys_count(t, slots);
                    hot = (rune)('0' + t->win[slots[i - R2__TUI_SYS_WINDOWS]].number);
                }
                else if (label)
                    hot = r2__tui_hot(label);
                if (hot && hot == r2__tui_lower(ev->ch) && r2__tui_selectable(t, t->menu_idx, i))
                {
                    t->menu_sel = i;
                    r2__tui_menu_choose(t);
                    return;
                }
            }
            return;
        }
        default:
            return;
        }
    }

    /* ---------------- move / size mode ---------------- */

    static void r2__tui_move_key(tui *t, const tui_event *ev)
    {
        tui_window *w = &t->win[t->focus];
        int shift = (ev->mods & TUI_MOD_SHIFT) != 0;
        int dx = 0, dy = 0;
        if (ev->type != TUI_EV_KEY)
            return;
        switch (ev->key)
        {
        case TUI_KEY_ENTER:
            t->moving = 0;
            t->compose_dirty = 1;
            return;
        case TUI_KEY_ESC:
            w->rect = t->move_saved;
            w->dock_size = t->move_saved_dock;
            t->moving = 0;
            t->layout_dirty = 1;
            return;
        case TUI_KEY_LEFT: dx = -1; break;
        case TUI_KEY_RIGHT: dx = 1; break;
        case TUI_KEY_UP: dy = -1; break;
        case TUI_KEY_DOWN: dy = 1; break;
        default: return;
        }
        if (w->dock)
        {
            int cur = w->dock == TUI_DOCK_BOTTOM ? w->frame.h : w->frame.w;
            int d = w->dock == TUI_DOCK_LEFT ? dx : w->dock == TUI_DOCK_RIGHT ? -dx : -dy;
            if (d)
                w->dock_size = cur + d < 3 ? 3 : cur + d;
        }
        else if (shift)
        {
            if (w->flags & TUI_WIN_NO_RESIZE)
                return;
            w->rect.w = w->rect.w + dx < 12 ? 12 : w->rect.w + dx;
            w->rect.h = w->rect.h + dy < 3 ? 3 : w->rect.h + dy;
        }
        else
        {
            w->rect.x += dx;
            w->rect.y += dy;
        }
        t->layout_dirty = 1;
    }

    /* ---------------- message box ---------------- */

    static uint32_t r2__tui_hash(const char *s, int owner)
    {
        uint32_t h = 0x811C9DC5u ^ (uint32_t)(owner + 7);
        for (; s && *s; s++)
            h = (h ^ (uint8_t)*s) * 0x01000193u;
        return h;
    }

    /* the message box is free again: re-run whoever was told to wait */
    static void r2__tui_mb_idle(tui *t)
    {
        int i;
        t->mb_state = R2__TUI_MB_IDLE;
        for (i = 0; i < R2__TUI_SLOTS; i++)
            if ((t->mb_waiting >> i) & 1u)
                t->win[i].dirty = t->win[i].used;
        t->mb_waiting = 0;
    }

    static const unsigned r2__tui_mb_flag[4] = {TUI_MB_OK, TUI_MB_YES, TUI_MB_NO, TUI_MB_CANCEL};
    static const char *const r2__tui_mb_label[4] = {"~O~K", "~Y~es", "~N~o", "~C~ancel"};

    /* Greedy word wrap: calls back with each line (start, length in bytes) */
    static int r2__tui_wrap(const char *text, int width, uint32_t *starts, uint32_t *lens, int max_lines)
    {
        s8 str = s8_from_cstr(text);
        uint32_t line = 0, at = 0, last_space = 0, has_space = 0;
        int lines = 0, w = 0;
        rune r;
        while (lines < max_lines)
        {
            uint32_t before = at;
            int more = s8_next_rune(str, &at, &r);
            if (!more || r == '\n' || (w + tui_rune_width(r) > width && w > 0))
            {
                uint32_t end = before;
                if (more && r != '\n' && has_space)
                    end = last_space; /* break at the last space */
                starts[lines] = line;
                lens[lines] = end - line;
                lines++;
                if (!more)
                    break;
                line = (r == '\n') ? at : (has_space ? last_space + 1 : before);
                at = line;
                w = 0;
                has_space = 0;
                continue;
            }
            if (r == ' ')
            {
                last_space = before;
                has_space = 1;
            }
            w += tui_rune_width(r);
        }
        return lines;
    }

    static void r2__tui_mb_ui(tui *t, tui_window *w, void *user)
    {
        uint32_t starts[16], lens[16];
        int cw, ch, lines, i, nb = 0, order[4], x, total = 0;
        const tui_event *ev = tui_key(t);
        (void)user;
        tui_size(t, &cw, &ch);
        for (i = 0; i < 4; i++)
            if (t->mb_buttons & r2__tui_mb_flag[i])
                order[nb++] = i;
        if (ev)
        {
            int chosen = -1;
            tui_use_key(t);
            if (ev->key == TUI_KEY_LEFT || (ev->key == TUI_KEY_TAB && (ev->mods & TUI_MOD_SHIFT)))
                t->mb_focus = (uint8_t)((t->mb_focus + nb - 1) % nb);
            else if (ev->key == TUI_KEY_RIGHT || ev->key == TUI_KEY_TAB)
                t->mb_focus = (uint8_t)((t->mb_focus + 1) % nb);
            else if (ev->key == TUI_KEY_ENTER || (ev->key == TUI_KEY_RUNE && ev->ch == ' ' && !ev->mods))
                chosen = order[t->mb_focus];
            else if (ev->key == TUI_KEY_ESC)
            {
                chosen = order[nb - 1];
                for (i = 0; i < nb; i++)
                    if (order[i] == 3)
                        chosen = 3;
            }
            else if (ev->key == TUI_KEY_RUNE)
                for (i = 0; i < nb; i++)
                    if (r2__tui_hot(r2__tui_mb_label[order[i]]) == r2__tui_lower(ev->ch))
                        chosen = order[i];
            if (chosen >= 0)
            {
                t->mb_result = r2__tui_mb_flag[chosen];
                t->mb_state = R2__TUI_MB_ANSWERED;
                if (t->mb_owner >= 0 && t->win[t->mb_owner].used)
                    t->win[t->mb_owner].dirty = 1;
                tui_window_close(t, w);
                return;
            }
        }
        lines = r2__tui_wrap(t->mb_text, cw - 2, starts, lens, 16);
        for (i = 0; i < lines; i++)
            tui_text(t, 1, i, s8_from_bytes(t->mb_text + starts[i], lens[i]), TUI_DEFAULT);
        for (i = 0; i < nb; i++)
            total += r2__tui_label_width(r2__tui_mb_label[order[i]]) + 4 + (i ? 2 : 0);
        x = (cw - total) / 2;
        for (i = 0; i < nb; i++)
        {
            const char *label = r2__tui_mb_label[order[i]];
            int lw = r2__tui_label_width(label);
            tui_style s = i == t->mb_focus ? t->theme->button_focus : t->theme->button;
            tui_fill(t, (tui_rect){x, ch - 1, lw + 4, 1}, ' ', s);
            tui_text(t, x, ch - 1, S8("["), s);
            tui_text(t, x + lw + 3, ch - 1, S8("]"), s);
            r2__tui_rec_label(t, x + 2, ch - 1, label, s, t->theme->button_hot);
            if (i == t->mb_focus)
                tui_cursor(t, x + 2, ch - 1);
            x += lw + 6;
        }
    }

    R2_TUI_DEF unsigned tui_message(tui *t, const char *title, const char *text, unsigned buttons)
    {
        uint32_t starts[16], lens[16];
        uint32_t hash = r2__tui_hash(title, t->cur);
        int lines, width = 0, i, bw = 0, tw;
        if (t->mb_state == R2__TUI_MB_ANSWERED && t->mb_hash == hash && t->mb_owner == t->cur && t->mb_kind == 0)
        {
            r2__tui_mb_idle(t);
            return t->mb_result;
        }
        if (t->mb_state != R2__TUI_MB_IDLE)
        {
            /* busy (asking, or holding an answer for someone else) */
            if (t->cur >= 0)
                t->mb_waiting |= 1u << t->cur;
            return 0;
        }
        buttons &= TUI_MB_OK | TUI_MB_YES | TUI_MB_NO | TUI_MB_CANCEL;
        if (!buttons)
            buttons = TUI_MB_OK;
        r2__tui_copy(t->mb_text, sizeof(t->mb_text), text);
        t->mb_buttons = (uint8_t)buttons;
        t->mb_focus = 0;
        t->mb_kind = 0;
        t->mb_hash = hash;
        t->mb_owner = t->cur;
        t->mb_state = R2__TUI_MB_OPEN;
        /* size: wrap at 46 columns (less on small screens) */
        tw = t->desk.w - 8 > 46 ? 46 : t->desk.w - 8;
        if (tw < 10)
            tw = 10;
        lines = r2__tui_wrap(t->mb_text, tw, starts, lens, 16);
        for (i = 0; i < lines; i++)
        {
            int lw = tui_text_width(s8_from_bytes(t->mb_text + starts[i], lens[i]));
            if (lw > width)
                width = lw;
        }
        for (i = 0; i < 4; i++)
            if (buttons & r2__tui_mb_flag[i])
                bw += r2__tui_label_width(r2__tui_mb_label[i]) + 6;
        if (bw > width)
            width = bw;
        if (r2__tui_label_width(title) + 4 > width)
            width = r2__tui_label_width(title) + 4;
        r2__tui_open_slot(t, R2__TUI_MSGBOX, title, (tui_rect){0, 0, width + 4, lines + 4},
                          TUI_WIN_DIALOG | TUI_WIN_MODAL | TUI_WIN_NO_RESIZE, r2__tui_mb_ui, 0);
        return 0;
    }

    /* ---------------- widgets ---------------- */

    enum
    {
        R2__TUI_W_NONE,
        R2__TUI_W_BUTTON,
        R2__TUI_W_CHECK,
        R2__TUI_W_RADIO,
        R2__TUI_W_TEXT,
        R2__TUI_W_LIST,
        R2__TUI_W_SCROLL
    };

    /* widgets that take plain letters as input (so hot letters need Alt) */
    static int r2__tui_eats_letters(uint8_t kind)
    {
        return kind == R2__TUI_W_TEXT || kind == R2__TUI_W_LIST;
    }

    /* a widget's id: its label (all of it, "##id" too) in this window; the
       same label twice in one run gets a different id the second time */
    static uint32_t r2__tui_wid(tui *t, const char *label)
    {
        uint32_t h = 0x811C9DC5u ^ ((uint32_t)(t->cur + 1) * 0x9E3779B9u);
        int i;
        for (; label && *label; label++)
            h = (h ^ (uint8_t)*label) * 0x01000193u;
        for (i = 0; i < t->nwid; i++)
            if (t->wid[i] == h || h == 0)
            {
                h = h * 31u + 7u;
                i = -1;
            }
        return h;
    }

    /* note a focusable widget; 1 if it has the keyboard */
    static int r2__tui_wreg(tui *t, uint32_t id, uint8_t kind)
    {
        if (t->cur < 0)
            return 0;
        if (t->nwid < R2_TUI_MAX_WIDGETS)
        {
            t->wid[t->nwid] = id;
            t->wkind[t->nwid] = kind;
            t->nwid++;
        }
        return t->cur == t->focus && t->win[t->cur].focus_id == id;
    }

    /* move focus to a widget (it is drawn focused on the next run) */
    static void r2__tui_wfocus(tui *t, uint32_t id, uint8_t kind)
    {
        tui_window *w = &t->win[t->cur];
        if (w->focus_id != id)
        {
            w->focus_id = id;
            w->focus_kind = kind;
            w->dirty = 1;
        }
    }

    /* the label's hot letter was typed: Alt+letter always, the letter alone
       unless the focused widget takes letters */
    static int r2__tui_hot_hit(tui *t, const char *label)
    {
        const tui_event *ev = tui_key(t);
        rune hot = r2__tui_hot(label);
        if (!ev || !hot || ev->key != TUI_KEY_RUNE || t->cur != t->focus)
            return 0;
        if (!(ev->mods == TUI_MOD_ALT || (ev->mods == 0 && !r2__tui_eats_letters(t->win[t->cur].focus_kind))))
            return 0;
        if (r2__tui_lower(ev->ch) != hot)
            return 0;
        tui_use_key(t);
        return 1;
    }

    static tui_wstate *r2__tui_state(tui *t, uint32_t id)
    {
        int i, oldest = 0;
        for (i = 0; i < R2_TUI_STATE_SLOTS; i++)
            if (t->wstate[i].id == id)
            {
                t->wstate[i].touched = t->frame;
                return &t->wstate[i];
            }
        for (i = 1; i < R2_TUI_STATE_SLOTS; i++)
            if (t->wstate[i].touched < t->wstate[oldest].touched)
                oldest = i;
        t->wstate[oldest].id = id;
        t->wstate[oldest].a = t->wstate[oldest].b = 0;
        t->wstate[oldest].touched = t->frame;
        return &t->wstate[oldest];
    }

    /* After a controller ran: Tab / Shift+Tab move focus, and focus always
       sits on a widget that exists. A change re-runs the window so it draws. */
    static void r2__tui_settle_focus(tui *t, int slot)
    {
        tui_window *w = &t->win[slot];
        int i, at = -1, n = t->nwid;
        const tui_event *ev = t->cur_ev;
        if (n == 0)
        {
            w->focus_id = 0;
            w->focus_kind = R2__TUI_W_NONE;
            return;
        }
        for (i = 0; i < n; i++)
            if (t->wid[i] == w->focus_id)
                at = i;
        if (slot == t->focus && ev && !t->ev_used && ev->type == TUI_EV_KEY && ev->key == TUI_KEY_TAB &&
            (ev->mods == 0 || ev->mods == TUI_MOD_SHIFT))
        {
            t->ev_used = 1;
            if (ev->mods == TUI_MOD_SHIFT)
                at = at <= 0 ? n - 1 : at - 1;
            else
                at = (at + 1) % n;
        }
        else if (at < 0)
            at = 0;
        if (w->focus_id != t->wid[at])
        {
            w->focus_id = t->wid[at];
            w->focus_kind = t->wkind[at];
            w->dirty = 1;
        }
        else
            w->focus_kind = t->wkind[at];
    }

    /* where the next widget goes (and remember it for tui_same_line) */
    static void r2__tui_place(tui *t, int w, int h, int *x, int *y)
    {
        if (t->lay_same)
        {
            *x = t->lay_lx + t->lay_lw + 1;
            *y = t->lay_ly;
            t->lay_same = 0;
        }
        else
        {
            *x = t->lay_x;
            *y = t->lay_y;
        }
        t->lay_lx = *x;
        t->lay_ly = *y;
        t->lay_lw = w;
        if (*y + h > t->lay_y)
            t->lay_y = *y + h;
    }

    R2_TUI_DEF void tui_at(tui *t, int x, int y)
    {
        t->lay_x = x;
        t->lay_y = y;
        t->lay_same = 0;
    }

    R2_TUI_DEF void tui_same_line(tui *t)
    {
        t->lay_same = 1;
    }

    R2_TUI_DEF void tui_space(tui *t, int lines)
    {
        t->lay_y += lines;
    }

    /* the window's background with a theme foreground */
    static tui_style r2__tui_on_body(const tui *t, tui_style fg)
    {
        tui_style body = t->cur >= 0 ? r2__tui_body(t, t->cur) : t->theme->body;
        return TUI_STYLE(TUI_STYLE_FG(fg), TUI_STYLE_BG(body), TUI_STYLE_ATTR(fg));
    }

    /* record the start of text that fits in max columns */
    static void r2__tui_text_fit(tui *t, int x, int y, s8 str, int max, tui_style s)
    {
        uint32_t at = 0, end = 0;
        int w = 0;
        rune r;
        while (s8_next_rune(str, &at, &r))
        {
            int rw = r2__tui_is_control(r) ? 1 : tui_rune_width(r);
            if (w + rw > max)
                break;
            w += rw;
            end = at;
        }
        if (end)
            tui_text(t, x, y, s8_slice(str, 0, end), s);
    }

    R2_TUI_DEF void tui_label(tui *t, const char *text)
    {
        int x, y;
        r2__tui_place(t, r2__tui_label_width(text), 1, &x, &y);
        r2__tui_rec_label(t, x, y, text, TUI_DEFAULT, r2__tui_on_body(t, t->theme->hot));
    }

    R2_TUI_DEF int tui_button(tui *t, const char *label)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_BUTTON);
        int lw = r2__tui_label_width(label), x, y, pressed = 0;
        const tui_event *ev = tui_key(t);
        tui_style s;
        if (focused && ev && !ev->mods && (ev->key == TUI_KEY_ENTER || (ev->key == TUI_KEY_RUNE && ev->ch == ' ')))
        {
            tui_use_key(t);
            pressed = 1;
        }
        else if (r2__tui_hot_hit(t, label))
        {
            pressed = 1;
            r2__tui_wfocus(t, id, R2__TUI_W_BUTTON);
        }
        r2__tui_place(t, lw + 4, 1, &x, &y);
        s = focused ? t->theme->button_focus : t->theme->button;
        tui_fill(t, (tui_rect){x, y, lw + 4, 1}, ' ', s);
        tui_text(t, x, y, S8("["), s);
        tui_text(t, x + lw + 3, y, S8("]"), s);
        r2__tui_rec_label(t, x + 2, y, label, s,
                          TUI_STYLE(TUI_STYLE_FG(t->theme->button_hot), TUI_STYLE_BG(s), 0));
        return pressed;
    }

    /* "[X] label" / "(•) label" */
    static void r2__tui_mark_label(tui *t, int x, int y, const char *open_close, rune mark, const char *label,
                                   int focused)
    {
        tui_style s = focused ? r2__tui_on_body(t, t->theme->focus) : TUI_DEFAULT;
        uint8_t m[4];
        tui_text(t, x, y, s8_from_bytes(open_close, 1), s);
        tui_text(t, x + 1, y, s8_from_bytes(m, utf8_encode(mark, m)), s);
        tui_text(t, x + 2, y, s8_from_bytes(open_close + 1, 1), s);
        r2__tui_rec_label(t, x + 4, y, label, s, r2__tui_on_body(t, t->theme->hot));
        if (focused)
            tui_cursor(t, x + 1, y);
    }

    R2_TUI_DEF int tui_checkbox(tui *t, const char *label, int *value)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_CHECK);
        int x, y, hit = 0;
        const tui_event *ev = tui_key(t);
        if (focused && ev && !ev->mods && ev->key == TUI_KEY_RUNE && ev->ch == ' ')
        {
            tui_use_key(t);
            hit = 1;
        }
        else if (r2__tui_hot_hit(t, label))
        {
            hit = 1;
            r2__tui_wfocus(t, id, R2__TUI_W_CHECK);
        }
        if (hit)
            *value = !*value;
        r2__tui_place(t, r2__tui_label_width(label) + 4, 1, &x, &y);
        r2__tui_mark_label(t, x, y, "[]", *value ? 'X' : ' ', label, focused);
        return hit;
    }

    R2_TUI_DEF int tui_radio(tui *t, const char *label, int *value, int option)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_RADIO);
        int x, y, hit = 0;
        const tui_event *ev = tui_key(t);
        if (focused && ev && !ev->mods && ev->key == TUI_KEY_RUNE && ev->ch == ' ')
        {
            tui_use_key(t);
            hit = 1;
        }
        else if (r2__tui_hot_hit(t, label))
        {
            hit = 1;
            r2__tui_wfocus(t, id, R2__TUI_W_RADIO);
        }
        hit = hit && *value != option;
        if (hit)
            *value = option;
        r2__tui_place(t, r2__tui_label_width(label) + 4, 1, &x, &y);
        r2__tui_mark_label(t, x, y, "()", *value == option ? 0x2022 : ' ', label, focused);
        return hit;
    }

    /* byte offset of the rune before / after at */
    static uint32_t r2__tui_prev(const char *s, uint32_t at)
    {
        if (at == 0)
            return 0;
        at--;
        while (at > 0 && ((uint8_t)s[at] & 0xC0u) == 0x80u)
            at--;
        return at;
    }

    static uint32_t r2__tui_next(const char *s, uint32_t len, uint32_t at)
    {
        rune r;
        if (at >= len)
            return len;
        return at + utf8_decode((const uint8_t *)s + at, len - at, &r);
    }

    static uint32_t r2__tui_strlen(const char *s)
    {
        uint32_t n = 0;
        while (s[n])
            n++;
        return n;
    }

    /* edit buf (cap bytes) for one key; returns TUI_CHANGED / TUI_SUBMIT, *used if it took the key */
    static unsigned r2__tui_edit_key(char *buf, size_t cap, int32_t *cursor, const tui_event *ev, int *used)
    {
        uint32_t len = r2__tui_strlen(buf), at = (uint32_t)*cursor, k;
        unsigned res = 0;
        int ctrl = ev->key == TUI_KEY_RUNE && ev->mods == TUI_MOD_CTRL;
        *used = 1;
        if (ev->key == TUI_KEY_LEFT || (ctrl && ev->ch == 'b'))
            at = r2__tui_prev(buf, at);
        else if (ev->key == TUI_KEY_RIGHT || (ctrl && ev->ch == 'f'))
            at = r2__tui_next(buf, len, at);
        else if (ev->key == TUI_KEY_HOME || (ctrl && ev->ch == 'a'))
            at = 0;
        else if (ev->key == TUI_KEY_END || (ctrl && ev->ch == 'e'))
            at = len;
        else if (ev->key == TUI_KEY_ENTER && !ev->mods)
            res = TUI_SUBMIT;
        else if (ev->key == TUI_KEY_BACKSPACE || ev->key == TUI_KEY_DELETE || (ctrl && ev->ch == 'd'))
        {
            uint32_t from = at, to = at;
            if (ev->key == TUI_KEY_BACKSPACE)
                from = r2__tui_prev(buf, at);
            else
                to = r2__tui_next(buf, len, at);
            if (to > from)
            {
                for (k = to; k <= len; k++)
                    buf[from + k - to] = buf[k];
                at = from;
                res = TUI_CHANGED;
            }
        }
        else if (ctrl && ev->ch == 'k')
        {
            if (at < len)
            {
                buf[at] = '\0';
                res = TUI_CHANGED;
            }
        }
        else if (ev->key == TUI_KEY_RUNE && !(ev->mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && !r2__tui_is_control(ev->ch))
        {
            uint8_t enc[4];
            uint32_t n = utf8_encode(ev->ch, enc);
            if (len + n < cap)
            {
                for (k = len + 1; k-- > at;)
                    buf[k + n] = buf[k];
                for (k = 0; k < n; k++)
                    buf[at + k] = (char)enc[k];
                at += n;
                res = TUI_CHANGED;
            }
        }
        else
            *used = 0;
        *cursor = (int32_t)at;
        return res;
    }

    R2_TUI_DEF unsigned tui_input(tui *t, const char *label, char *buf, size_t cap, int w)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_TEXT);
        tui_wstate *st = r2__tui_state(t, id);
        const tui_event *ev = tui_key(t);
        uint32_t len = r2__tui_strlen(buf), at, start, end;
        unsigned res = 0;
        int x, y, col = 0, c = 0, used;
        tui_style s;
        s8 str;
        rune r;
        if (w < 2)
            w = 2;
        if (r2__tui_hot_hit(t, label))
            r2__tui_wfocus(t, id, R2__TUI_W_TEXT);
        /* the text may have changed under us: keep the cursor on a character */
        if ((uint32_t)st->a > len)
            st->a = (int32_t)len;
        while (st->a > 0 && ((uint8_t)buf[st->a] & 0xC0u) == 0x80u)
            st->a--;
        if (focused && ev)
        {
            res = r2__tui_edit_key(buf, cap, &st->a, ev, &used);
            if (used)
                tui_use_key(t);
        }
        /* scroll sideways so the cursor shows */
        str = s8_from_cstr(buf);
        col = tui_text_width(s8_slice(str, 0, (uint32_t)st->a));
        if (col < st->b)
            st->b = col;
        if (col > st->b + w - 1)
            st->b = col - (w - 1);
        /* the visible part: whole characters from column b */
        start = end = str.len;
        for (at = 0;;)
        {
            uint32_t before = at;
            int rw;
            if (!s8_next_rune(str, &at, &r))
                break;
            rw = r2__tui_is_control(r) ? 1 : tui_rune_width(r);
            if (c >= st->b && start == str.len)
                start = before;
            if (c + rw > st->b + w)
            {
                end = before;
                break;
            }
            c += rw;
        }
        if (start > end)
            start = end;
        r2__tui_place(t, w, 1, &x, &y);
        s = focused ? t->theme->input_focus : t->theme->input;
        tui_fill(t, (tui_rect){x, y, w, 1}, ' ', s);
        if (end > start)
            tui_text(t, x + tui_text_width(s8_slice(str, 0, start)) - st->b, y, s8_slice(str, start, end - start), s);
        if (focused)
            tui_cursor(t, x + col - st->b, y);
        return res;
    }

    R2_TUI_DEF unsigned tui_list(tui *t, const char *label, const char *const *items, int count, int *selected, int w,
                                 int h)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_LIST);
        tui_wstate *st = r2__tui_state(t, id);
        const tui_event *ev = tui_key(t);
        int x, y, i, sel = *selected, bar = count > h, tw;
        unsigned res = 0;
        if (w < 3)
            w = 3;
        if (h < 1)
            h = 1;
        if (r2__tui_hot_hit(t, label))
            r2__tui_wfocus(t, id, R2__TUI_W_LIST);
        if (focused && ev && count > 0)
        {
            int used = 1;
            if (ev->key == TUI_KEY_UP)
                sel--;
            else if (ev->key == TUI_KEY_DOWN)
                sel++;
            else if (ev->key == TUI_KEY_PGUP)
                sel -= h - 1 > 0 ? h - 1 : 1;
            else if (ev->key == TUI_KEY_PGDN)
                sel += h - 1 > 0 ? h - 1 : 1;
            else if (ev->key == TUI_KEY_HOME)
                sel = 0;
            else if (ev->key == TUI_KEY_END)
                sel = count - 1;
            else if (ev->key == TUI_KEY_ENTER && !ev->mods)
                res |= TUI_SUBMIT;
            else if (ev->key == TUI_KEY_RUNE && !ev->mods && ev->ch > ' ')
            {
                /* jump to the next item starting with that letter */
                for (i = 1; i <= count; i++)
                {
                    int k = (sel + i) % count;
                    rune first = 0;
                    if (items[k])
                        utf8_decode((const uint8_t *)items[k], 4, &first);
                    if (items[k] && items[k][0] && r2__tui_lower(first) == r2__tui_lower(ev->ch))
                    {
                        sel = k;
                        break;
                    }
                }
            }
            else
                used = 0;
            if (used)
                tui_use_key(t);
        }
        if (sel >= count)
            sel = count - 1;
        if (sel < 0)
            sel = 0;
        if (count > 0 && sel != *selected)
        {
            *selected = sel;
            res |= TUI_CHANGED;
        }
        /* keep the selection in view */
        if (sel < st->a)
            st->a = sel;
        if (sel >= st->a + h)
            st->a = sel - h + 1;
        if (st->a > (count > h ? count - h : 0))
            st->a = count > h ? count - h : 0;
        if (st->a < 0)
            st->a = 0;
        r2__tui_place(t, w, h, &x, &y);
        tui_fill(t, (tui_rect){x, y, w, h}, ' ', t->theme->list);
        tw = w - 2 - (bar ? 1 : 0);
        for (i = 0; i < h && st->a + i < count; i++)
        {
            int k = st->a + i;
            if (k == sel)
            {
                tui_style s = focused ? t->theme->list_sel_focus : t->theme->list_sel;
                tui_fill(t, (tui_rect){x, y + i, w - (bar ? 1 : 0), 1}, ' ', s);
                r2__tui_text_fit(t, x + 1, y + i, s8_from_cstr(items[k] ? items[k] : ""), tw, s);
            }
            else
                r2__tui_text_fit(t, x + 1, y + i, s8_from_cstr(items[k] ? items[k] : ""), tw, t->theme->list);
        }
        if (bar)
        {
            int thumb = (h - 1) * st->a / (count - h);
            for (i = 0; i < h; i++)
                tui_text(t, x + w - 1, y + i, i == thumb ? S8("\xE2\x96\x88") : S8("\xE2\x96\x91"), t->theme->scrollbar);
        }
        return res;
    }

    R2_TUI_DEF void tui_scroll_begin(tui *t, const char *label, int w, int h, int content_h)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_SCROLL);
        tui_wstate *st = r2__tui_state(t, id);
        const tui_event *ev = tui_key(t);
        int max = content_h > h ? content_h - h : 0, x, y;
        if (t->sv_on)
            return; /* no nesting */
        if (r2__tui_hot_hit(t, label))
            r2__tui_wfocus(t, id, R2__TUI_W_SCROLL);
        if (focused && ev)
        {
            int used = 1, page = h - 1 > 0 ? h - 1 : 1;
            if (ev->key == TUI_KEY_UP)
                st->a--;
            else if (ev->key == TUI_KEY_DOWN)
                st->a++;
            else if (ev->key == TUI_KEY_PGUP)
                st->a -= page;
            else if (ev->key == TUI_KEY_PGDN || (ev->key == TUI_KEY_RUNE && ev->ch == ' ' && !ev->mods))
                st->a += page;
            else if (ev->key == TUI_KEY_HOME)
                st->a = 0;
            else if (ev->key == TUI_KEY_END)
                st->a = max;
            else
                used = 0;
            if (used)
                tui_use_key(t);
        }
        if (st->a > max)
            st->a = max;
        if (st->a < 0)
            st->a = 0;
        r2__tui_place(t, w, h, &x, &y);
        t->sv_on = 1;
        t->sv_x = x;
        t->sv_y = y;
        t->sv_w = w;
        t->sv_h = h;
        t->sv_content = content_h;
        t->sv_scroll = st->a;
        t->sv_focused = focused;
        t->sv_lx = t->lay_x;
        t->sv_ly = t->lay_y;
        r2__tui_clip_cmd(t, x, y, w - 1, h);
        t->org_x = x;
        t->org_y = y - st->a;
        t->lay_x = t->lay_y = 0;
        t->lay_same = 0;
    }

    R2_TUI_DEF void tui_scroll_end(tui *t)
    {
        int i, x, y, h;
        if (!t->sv_on)
            return;
        t->sv_on = 0;
        t->org_x = t->org_y = 0;
        r2__tui_clip_cmd(t, 0, 0, -1, -1);
        x = t->sv_x;
        y = t->sv_y;
        h = t->sv_h;
        t->lay_x = t->sv_lx;
        t->lay_y = t->sv_ly;
        t->lay_same = 0;
        if (t->sv_content > h)
        {
            int max = t->sv_content - h;
            int thumb = (h - 1) * t->sv_scroll / max;
            tui_style s = t->sv_focused ? t->theme->list_sel_focus : t->theme->scrollbar;
            for (i = 0; i < h; i++)
                tui_text(t, x + t->sv_w - 1, y + i, i == thumb ? S8("\xE2\x96\x88") : S8("\xE2\x96\x91"),
                         i == thumb ? s : t->theme->scrollbar);
        }
    }

    /* ---------------- input box ---------------- */

    static void r2__tui_ib_ui(tui *t, tui_window *w, void *user)
    {
        int cw, ch, ok, cancel = 0;
        unsigned r;
        const tui_event *ev;
        (void)user;
        tui_size(t, &cw, &ch);
        if (t->ib_fresh)
        {
            /* start with the cursor at the end of the text */
            r2__tui_state(t, r2__tui_wid(t, "##r2_input_box"))->a = (int32_t)r2__tui_strlen(t->mb_edit);
            t->ib_fresh = 0;
        }
        tui_at(t, 1, 0);
        tui_label(t, t->mb_text);
        tui_at(t, 1, 1);
        r = tui_input(t, "##r2_input_box", t->mb_edit, sizeof(t->mb_edit), cw - 2);
        tui_at(t, (cw - 17) / 2, ch - 1); /* "[ OK ] [ Cancel ]" */
        ok = tui_button(t, "~O~K");
        tui_same_line(t);
        cancel = tui_button(t, "~C~ancel");
        ev = tui_key(t);
        if (ev && ev->key == TUI_KEY_ESC)
        {
            tui_use_key(t);
            cancel = 1;
        }
        if ((r & TUI_SUBMIT) || ok || cancel)
        {
            t->mb_result = cancel && !ok && !(r & TUI_SUBMIT) ? TUI_MB_CANCEL : TUI_MB_OK;
            t->mb_state = R2__TUI_MB_ANSWERED;
            if (t->mb_owner >= 0 && t->win[t->mb_owner].used)
                t->win[t->mb_owner].dirty = 1;
            tui_window_close(t, w);
        }
    }

    R2_TUI_DEF unsigned tui_input_box(tui *t, const char *title, const char *prompt, char *buf, size_t cap)
    {
        uint32_t hash = r2__tui_hash(title, t->cur) ^ 0x5A5A5A5Au;
        int width;
        if (t->mb_state == R2__TUI_MB_ANSWERED && t->mb_hash == hash && t->mb_owner == t->cur &&
            t->mb_kind == 1)
        {
            unsigned r = t->mb_result;
            if (r == TUI_MB_OK)
                r2__tui_copy(buf, cap, t->mb_edit);
            r2__tui_mb_idle(t);
            return r;
        }
        if (t->mb_state != R2__TUI_MB_IDLE)
        {
            if (t->cur >= 0)
                t->mb_waiting |= 1u << t->cur;
            return 0;
        }
        r2__tui_copy(t->mb_text, sizeof(t->mb_text), prompt);
        r2__tui_copy(t->mb_edit, cap < sizeof(t->mb_edit) ? cap : sizeof(t->mb_edit), buf);
        t->mb_kind = 1;
        t->mb_hash = hash;
        t->mb_owner = t->cur;
        t->mb_state = R2__TUI_MB_OPEN;
        t->ib_fresh = 1;
        width = r2__tui_label_width(prompt) + 4;
        if (width < 40)
            width = 40;
        if (r2__tui_label_width(title) + 8 > width)
            width = r2__tui_label_width(title) + 8;
        if (width > t->desk.w - 4)
            width = t->desk.w - 4;
        r2__tui_open_slot(t, R2__TUI_MSGBOX, title, (tui_rect){0, 0, width, 6},
                          TUI_WIN_DIALOG | TUI_WIN_MODAL | TUI_WIN_NO_RESIZE, r2__tui_ib_ui, 0);
        return 0;
    }

    /* ---------------- command palette ---------------- */

    enum
    {
        R2__TUI_PAL_WIN,    /* an item of the focused window's menus */
        R2__TUI_PAL_DESK,   /* an item of the desktop's menus */
        R2__TUI_PAL_SYS,    /* a system menu command */
        R2__TUI_PAL_WINDOW  /* a window to go to */
    };

    /* append s to buf[n..], dropping ~ marks if plain; returns the new length */
    static size_t r2__tui_append(char *buf, size_t n, size_t cap, const char *s, int plain)
    {
        for (; s && *s && n + 1 < cap; s++)
            if (!plain || *s != '~')
                buf[n++] = *s;
        buf[n] = '\0';
        return n;
    }

    /* the text shown and searched for an entry; *key gets its shortcut (or 0) */
    static void r2__tui_pal_text(const tui *t, const tui_pal_entry *e, char *buf, size_t cap, const char **key,
                                 char *kbuf, size_t kcap)
    {
        size_t n = 0;
        *key = 0;
        buf[0] = '\0';
        if (e->kind == R2__TUI_PAL_WIN || e->kind == R2__TUI_PAL_DESK)
        {
            const tui_menubar *bar = e->kind == R2__TUI_PAL_WIN ? t->bar_win : t->bar_desk;
            const tui_item_rec *it = &bar->item[e->index];
            int m;
            for (m = 0; m < bar->nmenus; m++)
                if (e->index >= bar->menu[m].first && e->index < bar->menu[m].first + bar->menu[m].count)
                {
                    n = r2__tui_append(buf, n, cap, bar->text + bar->menu[m].title, 1);
                    n = r2__tui_append(buf, n, cap, " \xE2\x80\xBA ", 0);
                    break;
                }
            r2__tui_append(buf, n, cap, bar->text + it->label, 1);
            if (it->shortcut != R2__TUI_SEP)
                *key = bar->text + it->shortcut;
        }
        else if (e->kind == R2__TUI_PAL_SYS)
        {
            n = r2__tui_append(buf, n, cap, "Window \xE2\x80\xBA ", 0);
            r2__tui_append(buf, n, cap, r2__tui_sys_label[e->index], 1);
            *key = r2__tui_sys_key(t, e->index, kbuf, kcap);
        }
        else
        {
            const tui_window *w = &t->win[e->index];
            n = r2__tui_append(buf, n, cap, w->hidden ? "Show window \xE2\x80\xBA " : "Go to window \xE2\x80\xBA ", 0);
            r2__tui_append(buf, n, cap, w->title, 0);
            if (t->prefix_key && w->number && kcap > 4)
            {
                size_t k = r2__tui_append(kbuf, 0, kcap - 2, t->prefix_name, 0);
                kbuf[k++] = ' ';
                kbuf[k++] = (char)('0' + w->number);
                kbuf[k] = '\0';
                *key = kbuf;
            }
        }
    }

    /* text -> lower case runes (ASCII folding), at most cap */
    static int r2__tui_runes(const char *text, rune *out, int cap)
    {
        s8 str = s8_from_cstr(text);
        uint32_t at = 0;
        rune r;
        int n = 0;
        while (n < cap && s8_next_rune(str, &at, &r))
            out[n++] = r2__tui_lower(r);
        return n;
    }

    static int r2__tui_word_start(rune before)
    {
        return before == ' ' || before == '/' || before == '-' || before == '_' || before == '.' ||
               before == '(' || before == 0x203Au;
    }

    /*
     * Fuzzy match: q must appear in c in order (not necessarily together).
     * Higher scores for letters at the start or at word starts and for runs
     * of consecutive letters; lower for gaps and long texts. Returns -1 if q
     * does not match. Tries every place the first letter appears, matching
     * greedily from there, and keeps the best. marks (if given) gets 1 for
     * each matched rune.
     */
    static int r2__tui_fuzzy(const rune *q, int qn, const rune *c, int cn, uint8_t *marks)
    {
        uint8_t pos[32], best_pos[32];
        int best = -1, s, k;
        if (qn <= 0)
            return 10000;
        if (qn > 32)
            qn = 32;
        for (s = 0; s < cn; s++)
        {
            int i, score = 10000 - cn / 4 - s / 2;
            if (c[s] != q[0])
                continue;
            for (k = 0, i = s; i < cn && k < qn; i++)
                if (c[i] == q[k])
                    pos[k++] = (uint8_t)i;
            if (k < qn)
                break; /* no later start can match either */
            for (k = 0; k < qn; k++)
            {
                int p = pos[k];
                score += 16;
                if (p == 0)
                    score += 32;
                else if (r2__tui_word_start(c[p - 1]))
                    score += 24;
                if (k > 0)
                    score += (p == pos[k - 1] + 1) ? 24 : -(p - pos[k - 1] - 1);
            }
            if (score > best)
            {
                best = score;
                for (k = 0; k < qn; k++)
                    best_pos[k] = pos[k];
            }
        }
        if (best >= 0 && marks)
            for (k = 0; k < qn; k++)
                marks[best_pos[k]] = 1;
        return best;
    }

    static int r2__tui_pal_same(const tui *t, const tui_pal_entry *a, const tui_pal_entry *b)
    {
        char ta[128], tb[128], ka[32], kb[32];
        const char *k1, *k2;
        size_t i;
        r2__tui_pal_text(t, a, ta, sizeof(ta), &k1, ka, sizeof(ka));
        r2__tui_pal_text(t, b, tb, sizeof(tb), &k2, kb, sizeof(kb));
        for (i = 0; ta[i] && ta[i] == tb[i]; i++)
            ;
        return ta[i] == tb[i];
    }

    static void r2__tui_pal_add(tui *t, uint8_t kind, int index, const rune *q, int qn)
    {
        char text[128], kbuf[32];
        const char *key;
        rune c[128];
        tui_pal_entry e;
        int cn, score, i;
        if (t->pal_n >= R2_TUI_PALETTE_MAX)
            return;
        e.kind = kind;
        e.spare = 0;
        e.index = (uint16_t)index;
        r2__tui_pal_text(t, &e, text, sizeof(text), &key, kbuf, sizeof(kbuf));
        cn = r2__tui_runes(text, c, 128);
        score = r2__tui_fuzzy(q, qn, c, cn, 0);
        if (score < 0)
            return;
        e.score = (int16_t)(score > 32767 ? 32767 : score);
        /* keep sorted by score; ties stay in the order they were added */
        i = t->pal_n++;
        while (i > 0 && t->pal[i - 1].score < e.score)
        {
            t->pal[i] = t->pal[i - 1];
            i--;
        }
        t->pal[i] = e;
    }

    static void r2__tui_pal_refresh(tui *t, int keep_sel)
    {
        int sel = t->pal_sel, top = t->pal_top;
        rune q[32];
        int qn = r2__tui_runes(t->pal_query, q, 32), i, k;
        int has_win = t->focus >= 0 && t->bar_win->nmenus > 0;
        t->pal_n = 0;
        if (has_win)
            for (i = 0; i < t->bar_win->nitems; i++)
            {
                const tui_item_rec *it = &t->bar_win->item[i];
                if (it->label != R2__TUI_SEP && !(it->flags & TUI_ITEM_DISABLED))
                    r2__tui_pal_add(t, R2__TUI_PAL_WIN, i, q, qn);
            }
        if (t->win[R2__TUI_DESKTOP].used)
            for (i = 0; i < t->bar_desk->nitems; i++)
            {
                const tui_item_rec *it = &t->bar_desk->item[i];
                tui_pal_entry d = {R2__TUI_PAL_DESK, 0, (uint16_t)i, 0}, w = {R2__TUI_PAL_WIN, 0, 0, 0};
                int dup = 0;
                if (it->label == R2__TUI_SEP || (it->flags & TUI_ITEM_DISABLED))
                    continue;
                /* the same command in the window's menus wins */
                for (k = 0; has_win && k < t->bar_win->nitems && !dup; k++)
                {
                    w.index = (uint16_t)k;
                    dup = t->bar_win->item[k].label != R2__TUI_SEP && r2__tui_pal_same(t, &d, &w);
                }
                if (!dup)
                    r2__tui_pal_add(t, R2__TUI_PAL_DESK, i, q, qn);
            }
        for (i = 0; i < R2__TUI_SYS_WINDOWS; i++)
            if (r2__tui_selectable(t, 0, i))
                r2__tui_pal_add(t, R2__TUI_PAL_SYS, i, q, qn);
        for (i = 2; i < R2__TUI_SLOTS; i++)
            if (t->win[i].used && i != t->focus)
                r2__tui_pal_add(t, R2__TUI_PAL_WINDOW, i, q, qn);
        t->pal_sel = keep_sel ? (sel < t->pal_n ? sel : t->pal_n - 1) : 0;
        t->pal_top = keep_sel ? top : 0;
        if (t->pal_sel < 0)
            t->pal_sel = 0;
        if (t->pal_top > t->pal_sel)
            t->pal_top = t->pal_sel;
        t->compose_dirty = 1;
    }

    static int r2__tui_pal_rows(const tui *t)
    {
        int r = t->rows - 7;
        return r > 12 ? 12 : r < 1 ? 1 : r;
    }

    R2_TUI_DEF void tui_palette(tui *t)
    {
        t->menu_open = 0;
        t->chord = 0;
        t->pal_open = 1;
        t->pal_query[0] = '\0';
        t->pal_n = t->pal_sel = t->pal_top = 0;
        /* inside a controller the menus are half declared: list after the run */
        if (t->cur < 0)
            r2__tui_pal_refresh(t, 0);
    }

    static void r2__tui_pal_close(tui *t)
    {
        t->pal_open = 0;
        t->compose_dirty = 1;
    }

    static void r2__tui_pal_run(tui *t)
    {
        tui_pal_entry e;
        if (t->pal_n == 0)
            return;
        e = t->pal[t->pal_sel];
        r2__tui_pal_close(t);
        switch (e.kind)
        {
        case R2__TUI_PAL_WIN:
            if (t->focus >= 0 && e.index < t->bar_win->nitems)
            {
                t->menu_act = e.index;
                r2__tui_run(t, t->focus, 0);
                t->menu_act = -1;
            }
            break;
        case R2__TUI_PAL_DESK:
            if (t->win[R2__TUI_DESKTOP].used && e.index < t->bar_desk->nitems)
            {
                t->menu_act = e.index;
                r2__tui_run(t, R2__TUI_DESKTOP, 0);
                t->menu_act = -1;
            }
            break;
        case R2__TUI_PAL_SYS:
            r2__tui_sys_do(t, e.index);
            break;
        default:
            if (t->win[e.index].used)
                r2__tui_focus(t, e.index);
        }
    }

    static int r2__tui_shortcut_is(uint16_t key, uint8_t mods, rune ch, const tui_event *ev)
    {
        tui_item_rec it;
        it.key = key;
        it.mods = mods;
        it.ch = ch;
        return key != TUI_KEY_NONE && ev->type == TUI_EV_KEY && r2__tui_shortcut_match(&it, ev);
    }

    static void r2__tui_pal_key(tui *t, const tui_event *ev)
    {
        int rows = r2__tui_pal_rows(t);
        int ctrl = ev->key == TUI_KEY_RUNE && ev->mods == TUI_MOD_CTRL;
        if (ev->type != TUI_EV_KEY)
            return;
        if (ev->key == TUI_KEY_ESC || (ctrl && ev->ch == 'g') ||
            r2__tui_shortcut_is(t->pal_key, t->pal_mods, t->pal_ch, ev))
        {
            r2__tui_pal_close(t);
            return;
        }
        if (ev->key == TUI_KEY_ENTER)
        {
            r2__tui_pal_run(t);
            return;
        }
        if (ev->key == TUI_KEY_UP || (ctrl && ev->ch == 'p') || (ev->key == TUI_KEY_TAB && ev->mods == TUI_MOD_SHIFT))
            t->pal_sel--;
        else if (ev->key == TUI_KEY_DOWN || (ctrl && ev->ch == 'n') || (ev->key == TUI_KEY_TAB && !ev->mods))
            t->pal_sel++;
        else if (ev->key == TUI_KEY_PGUP)
            t->pal_sel -= rows;
        else if (ev->key == TUI_KEY_PGDN)
            t->pal_sel += rows;
        else if (ev->key == TUI_KEY_BACKSPACE || (ctrl && ev->ch == 'u'))
        {
            size_t n = 0;
            while (t->pal_query[n])
                n++;
            if (ctrl)
                n = 0;
            else if (n > 0)
            {
                n--;
                while (n > 0 && ((uint8_t)t->pal_query[n] & 0xC0u) == 0x80u)
                    n--;
            }
            t->pal_query[n] = '\0';
            r2__tui_pal_refresh(t, 0);
            return;
        }
        else if (ev->key == TUI_KEY_RUNE && !(ev->mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && !r2__tui_is_control(ev->ch))
        {
            size_t n = 0;
            while (t->pal_query[n])
                n++;
            if (n + 5 < sizeof(t->pal_query))
            {
                n += utf8_encode(ev->ch, (uint8_t *)t->pal_query + n);
                t->pal_query[n] = '\0';
            }
            r2__tui_pal_refresh(t, 0);
            return;
        }
        else
            return;
        if (t->pal_sel >= t->pal_n)
            t->pal_sel = t->pal_n - 1;
        if (t->pal_sel < 0)
            t->pal_sel = 0;
        if (t->pal_sel < t->pal_top)
            t->pal_top = t->pal_sel;
        if (t->pal_sel >= t->pal_top + rows)
            t->pal_top = t->pal_sel - rows + 1;
        t->compose_dirty = 1;
    }

    /* the palette, under the menu bar; sets the cursor position in the query */
    static void r2__tui_draw_palette(tui *t, int *cx, int *cy)
    {
        const tui_theme *th = t->theme;
        int rows = r2__tui_pal_rows(t);
        int shown = t->pal_n < rows ? t->pal_n : rows;
        int w = t->cols - 4 > 70 ? 70 : t->cols - 4;
        int i, qx, qw;
        rune q[32];
        int qn = r2__tui_runes(t->pal_query, q, 32);
        tui_rect r;
        if (w < 16 || t->rows < 6)
            return;
        if (shown < 1)
            shown = 1;
        r.w = w;
        r.h = shown + 4;
        r.x = (t->cols - w) / 2;
        r.y = 1;
        tui_draw_fill(t, r, ' ', th->menu);
        tui_draw_box(t, r, th->menu, TUI_BOX_SINGLE);
        tui_draw_text(t, r.x + (w - 8) / 2, r.y, S8(" Search "), th->menu);
        /* the query */
        tui_draw_text(t, r.x + 2, r.y + 1, S8(">"), th->menu_hot);
        qx = r.x + 4;
        tui_clip(t, (tui_rect){qx, r.y + 1, w - 6, 1});
        qw = tui_draw_text(t, qx, r.y + 1, s8_from_cstr(t->pal_query), th->menu);
        tui_clip_reset(t);
        *cx = qx + (qw < w - 6 ? qw : w - 7);
        *cy = r.y + 1;
        tui_draw_rune(t, r.x, r.y + 2, 0x251C, th->menu);
        for (i = 1; i < w - 1; i++)
            tui_draw_rune(t, r.x + i, r.y + 2, 0x2500, th->menu);
        tui_draw_rune(t, r.x + w - 1, r.y + 2, 0x2524, th->menu);
        if (t->pal_n == 0)
            tui_draw_text(t, r.x + 2, r.y + 3, S8("No matches"), th->menu_disabled);
        for (i = 0; i < shown && t->pal_top + i < t->pal_n; i++)
        {
            const tui_pal_entry *e = &t->pal[t->pal_top + i];
            int sel = t->pal_top + i == t->pal_sel;
            tui_style s = sel ? th->menu_sel : th->menu;
            tui_style h = sel ? th->menu_sel_hot : th->menu_hot;
            char text[128], kbuf[32];
            const char *key;
            rune c[128];
            uint8_t marks[128] = {0};
            int y = r.y + 3 + i, x = r.x + 2, k = 0, kw = 0, cn;
            uint32_t at = 0;
            rune ch;
            s8 str;
            r2__tui_pal_text(t, e, text, sizeof(text), &key, kbuf, sizeof(kbuf));
            cn = r2__tui_runes(text, c, 128);
            r2__tui_fuzzy(q, qn, c, cn, marks);
            tui_draw_fill(t, (tui_rect){r.x + 1, y, w - 2, 1}, ' ', s);
            if (key)
            {
                kw = tui_text_width(s8_from_cstr(key));
                tui_draw_text(t, r.x + w - 2 - kw, y, s8_from_cstr(key), s);
            }
            tui_clip(t, (tui_rect){r.x + 2, y, w - 4 - (kw ? kw + 2 : 0), 1});
            str = s8_from_cstr(text);
            while (s8_next_rune(str, &at, &ch))
            {
                x += tui_draw_rune(t, x, y, ch, (k < 128 && marks[k]) ? h : s);
                k++;
            }
            tui_clip_reset(t);
        }
        tui_draw_shadow(t, (tui_rect){r.x + r.w, r.y + 1, 2, r.h});
        tui_draw_shadow(t, (tui_rect){r.x + 2, r.y + r.h, r.w, 1});
    }

    R2_TUI_DEF int tui_set_palette_key(tui *t, const char *shortcut)
    {
        uint16_t key;
        uint8_t mods;
        rune ch;
        if (!shortcut || !*shortcut)
        {
            t->pal_key = TUI_KEY_NONE;
            t->pal_name[0] = '\0';
            t->compose_dirty = 1;
            return 1;
        }
        if (!r2__tui_parse_shortcut(shortcut, &key, &mods, &ch))
            return 0;
        t->pal_key = key;
        t->pal_mods = mods;
        t->pal_ch = ch;
        r2__tui_copy(t->pal_name, sizeof(t->pal_name), shortcut);
        t->compose_dirty = 1;
        return 1;
    }

    /* ---------------- composing the screen ---------------- */

    static void r2__tui_draw_window(tui *t, int slot)
    {
        tui_window *w = &t->win[slot];
        const tui_theme *th = t->theme;
        tui_rect f = w->frame;
        int active = slot == t->focus;
        int dialog = (w->flags & TUI_WIN_DIALOG) != 0;
        tui_style fs = (active && t->moving) ? th->frame_move
                       : dialog              ? th->dialog_frame
                       : active              ? th->frame_active
                                             : th->frame;
        tui_style body = r2__tui_body(t, slot);
        int tw, room, x;
        if (f.w < 2 || f.h < 2)
            return;
        /* the frame sits on the window's background */
        fs = TUI_STYLE(TUI_STYLE_FG(fs), TUI_STYLE_BG(body), TUI_STYLE_ATTR(fs));
        tui_draw_fill(t, f, ' ', body);
        tui_draw_box(t, f, fs, active ? TUI_BOX_DOUBLE : TUI_BOX_SINGLE);

        /* title, centred, clipped between the corner markers */
        room = f.w - 10;
        tw = r2__tui_label_width(w->title);
        if (room > 0 && w->title[0])
        {
            if (tw > room)
                tw = room;
            x = f.x + (f.w - tw - 2) / 2;
            tui_clip(t, (tui_rect){x, f.y, tw + 2, 1});
            tui_draw_rune(t, x, f.y, ' ', fs);
            tui_draw_text(t, x + 1, f.y, s8_from_cstr(w->title), fs);
            tui_draw_rune(t, x + 1 + tw, f.y, ' ', fs);
            tui_clip_reset(t);
        }
        if (active && f.w >= 8 && !(w->flags & TUI_WIN_NO_CLOSE) && slot >= 2)
        {
            tui_draw_text(t, f.x + 2, f.y, S8("[\xE2\x96\xA0]"), fs);
        }
        if (w->number && f.w >= 8)
            tui_draw_rune(t, f.x + f.w - 4, f.y, (rune)('0' + w->number), fs);
        if (w->overflow)
            tui_draw_rune(t, f.x + f.w - 2, f.y + f.h - 1, '!', TUI_STYLE(TUI_BRIGHT_RED, TUI_STYLE_BG(fs), TUI_BOLD));
    }

    static void r2__tui_draw_status(tui *t)
    {
        const tui_theme *th = t->theme;
        const char *text;
        int y = t->rows - 1;
        if (t->rows < 3)
            return;
        tui_draw_fill(t, (tui_rect){0, y, t->cols, 1}, ' ', th->status);
        if (t->moving)
            text = t->win[t->focus].dock ? " ~Arrows~ Size  ~Enter~ Done  ~Esc~ Cancel"
                                         : " ~Arrows~ Move  ~Shift+Arrows~ Size  ~Enter~ Done  ~Esc~ Cancel";
        else if (t->pal_open)
            text = " ~\xE2\x86\x91\xE2\x86\x93~ Choose  ~Enter~ Run  ~Esc~ Close  Type to search";
        else if (t->chord)
            text = 0;
        else if (t->menu_open)
            text = " ~\xE2\x86\x90\xE2\x86\x92~ Menus  ~\xE2\x86\x91\xE2\x86\x93~ Items  ~Enter~ Choose  ~Esc~ Close";
        else if (t->focus >= 0 && t->win[t->focus].status[0])
            text = t->win[t->focus].status;
        else if (t->focus < 0 && t->win[R2__TUI_DESKTOP].used && t->win[R2__TUI_DESKTOP].status[0])
            text = t->win[R2__TUI_DESKTOP].status;
        else
            text = 0;
        if (text)
        {
            r2__tui_label(t, 0, y, text, th->status, th->status_hot);
            return;
        }
        {
            /* the prefix is waiting, or the default hints: both name the keys */
            int x = 1;
            if (t->chord)
            {
                x += tui_draw_text(t, x, y, s8_from_cstr(t->prefix_name), th->status_hot);
                r2__tui_label(t, x, y,
                              " ~letter~ Menu  ~1-9~ Window  ~/~ Search  ~Space~ System  ~\xE2\x86\x91\xE2\x86\x93~ Max/Min  "
                              "~\xE2\x86\x90\xE2\x86\x92~ Switch  ~Enter~ Move  ~Bksp~ Close  ~Esc~ Cancel",
                              th->status, th->status_hot);
                return;
            }
            if (t->prefix_key)
            {
                x += tui_draw_text(t, x, y, s8_from_cstr(t->prefix_name), th->status_hot);
                x += r2__tui_label(t, x, y, " Commands  ", th->status, th->status_hot);
            }
            if (t->pal_key)
            {
                x += tui_draw_text(t, x, y, s8_from_cstr(t->pal_name), th->status_hot);
                x += r2__tui_label(t, x, y, " Search  ", th->status, th->status_hot);
            }
            r2__tui_label(t, x, y,
                          t->prefix_key ? "~F10~ Menu  ~F6~ Next  ~F5~ Zoom  ~Alt+F3~ Close"
                                        : "~F10~ Menu  ~F6~ Next  ~F5~ Zoom  ~Ctrl+F5~ Move  ~Alt+F3~ Close",
                          th->status, th->status_hot);
        }
    }

    static void r2__tui_draw_menus(tui *t)
    {
        const tui_theme *th = t->theme;
        const tui_menubar *bar = r2__tui_bar(t, 0);
        int m, x = 1, n = 1 + bar->nmenus;
        int modal = r2__tui_modal(t) >= 0;
        tui_draw_fill(t, (tui_rect){0, 0, t->cols, 1}, ' ', th->menu);
        for (m = 0; m < n; m++)
        {
            const char *title = m == 0 ? "\xE2\x89\xA1" : bar->text + bar->menu[m - 1].title;
            int sel = t->menu_open && t->menu_idx == m;
            tui_style s = modal ? th->menu_disabled : sel ? th->menu_sel : th->menu;
            tui_style h = modal ? th->menu_disabled : sel ? th->menu_sel_hot : th->menu_hot;
            int w = r2__tui_label_width(title);
            t->menu_x[m] = x;
            tui_draw_rune(t, x, 0, ' ', s);
            r2__tui_label(t, x + 1, 0, title, s, h);
            tui_draw_rune(t, x + 1 + w, 0, ' ', s);
            x += w + 2;
        }
        if (!t->menu_open)
            return;
        /* the open menu */
        {
            int count = r2__tui_menu_count(t, t->menu_idx), i, width = 0, key_w = 0;
            int slots[R2__TUI_SLOTS];
            tui_rect r;
            r2__tui_sys_count(t, slots);
            for (i = 0; i < count; i++)
            {
                const char *label = r2__tui_item_label(t, t->menu_idx, i);
                const char *key = 0;
                char kbuf[32];
                int lw = label ? r2__tui_label_width(label) : 0;
                if (t->menu_idx == 0 && i >= R2__TUI_SYS_WINDOWS)
                    lw += 2;
                if (t->menu_idx == 0 && i < R2__TUI_SYS_WINDOWS)
                    key = r2__tui_sys_key(t, i, kbuf, sizeof(kbuf));
                else if (t->menu_idx > 0)
                {
                    const tui_item_rec *it = &bar->item[bar->menu[t->menu_idx - 1].first + i];
                    key = it->shortcut == R2__TUI_SEP ? 0 : bar->text + it->shortcut;
                }
                if (lw > width)
                    width = lw;
                if (key && r2__tui_label_width(key) > key_w)
                    key_w = r2__tui_label_width(key);
            }
            r.w = width + (key_w ? key_w + 3 : 0) + 6;
            r.h = count + 2;
            r.x = t->menu_x[t->menu_idx] - 1;
            if (r.x + r.w > t->cols)
                r.x = t->cols - r.w;
            if (r.x < 0)
                r.x = 0;
            r.y = 1;
            tui_draw_fill(t, r, ' ', th->menu);
            tui_draw_box(t, r, th->menu, TUI_BOX_SINGLE);
            for (i = 0; i < count; i++)
            {
                const char *label = r2__tui_item_label(t, t->menu_idx, i);
                int y = r.y + 1 + i;
                int sel = i == t->menu_sel;
                int ok = r2__tui_selectable(t, t->menu_idx, i);
                int checked = 0;
                const char *key = 0;
                char kbuf[32];
                tui_style s = !ok ? th->menu_disabled : sel ? th->menu_sel : th->menu;
                tui_style h = !ok ? th->menu_disabled : sel ? th->menu_sel_hot : th->menu_hot;
                if (t->menu_idx > 0)
                {
                    const tui_item_rec *it = &bar->item[bar->menu[t->menu_idx - 1].first + i];
                    checked = (it->flags & TUI_ITEM_CHECKED) != 0;
                    key = it->shortcut == R2__TUI_SEP ? 0 : bar->text + it->shortcut;
                }
                else if (i < R2__TUI_SYS_WINDOWS)
                    key = r2__tui_sys_key(t, i, kbuf, sizeof(kbuf));
                if (!label)
                {
                    int x2;
                    tui_draw_rune(t, r.x, y, 0x251C, th->menu);
                    for (x2 = r.x + 1; x2 < r.x + r.w - 1; x2++)
                        tui_draw_rune(t, x2, y, 0x2500, th->menu);
                    tui_draw_rune(t, r.x + r.w - 1, y, 0x2524, th->menu);
                    continue;
                }
                tui_draw_fill(t, (tui_rect){r.x + 1, y, r.w - 2, 1}, ' ', s);
                if (checked)
                    tui_draw_rune(t, r.x + 2, y, 0x221A, s);
                if (t->menu_idx == 0 && i >= R2__TUI_SYS_WINDOWS)
                {
                    tui_window *w = &t->win[slots[i - R2__TUI_SYS_WINDOWS]];
                    tui_draw_rune(t, r.x + 3, y, (rune)('0' + w->number), h);
                    tui_clip(t, (tui_rect){r.x + 5, y, r.w - 7, 1});
                    tui_draw_text(t, r.x + 5, y, s8_from_cstr(label), s);
                    tui_clip_reset(t);
                }
                else
                    r2__tui_label(t, r.x + 3, y, label, s, h);
                if (key)
                    tui_draw_text(t, r.x + r.w - 2 - r2__tui_label_width(key), y, s8_from_cstr(key), s);
            }
            tui_draw_shadow(t, (tui_rect){r.x + r.w, r.y + 1, 2, r.h});
            tui_draw_shadow(t, (tui_rect){r.x + 2, r.y + r.h, r.w, 1});
        }
    }

    static void r2__tui_compose(tui *t)
    {
        int i, cx = 0, cy = 0, has_cursor = 0;
        tui_clip_reset(t);
        tui_draw_fill(t, t->desk, t->theme->desktop_rune, t->theme->desktop);
        if (t->win[R2__TUI_DESKTOP].used)
            r2__tui_replay(t, R2__TUI_DESKTOP, &cx, &cy);
        for (i = 2; i < R2__TUI_SLOTS; i++)
        {
            if (!t->win[i].used || !t->win[i].dock || t->win[i].hidden)
                continue;
            r2__tui_draw_window(t, i);
            if (r2__tui_replay(t, i, &cx, &cy) && i == t->focus)
                has_cursor = 1;
        }
        for (i = 0; i < (int)t->nz; i++)
        {
            int slot = t->z[i];
            tui_window *w = &t->win[slot];
            int x = 0, y = 0;
            if (!w->used || w->hidden)
                continue;
            r2__tui_draw_window(t, slot);
            if (r2__tui_replay(t, slot, &x, &y) && slot == t->focus)
            {
                has_cursor = 1;
                cx = x;
                cy = y;
            }
            if (!(w->flags & TUI_WIN_NO_SHADOW))
            {
                tui_draw_shadow(t, (tui_rect){w->frame.x + w->frame.w, w->frame.y + 1, 2, w->frame.h});
                tui_draw_shadow(t, (tui_rect){w->frame.x + 2, w->frame.y + w->frame.h, w->frame.w, 1});
            }
        }
        r2__tui_draw_menus(t);
        if (t->pal_open)
        {
            r2__tui_draw_palette(t, &cx, &cy);
            has_cursor = 1;
        }
        r2__tui_draw_status(t);
        tui_set_cursor(t, cx, cy, has_cursor && !t->menu_open && !t->moving);
        t->compose_dirty = 0;
    }

    /* ---------------- input routing ---------------- */

    static int r2__tui_key_is(const tui_event *ev, uint16_t key, uint8_t mods)
    {
        return ev->type == TUI_EV_KEY && ev->key == key && ev->mods == mods;
    }

    static int r2__tui_is_prefix(const tui *t, const tui_event *ev)
    {
        tui_item_rec it;
        it.key = t->prefix_key;
        it.mods = t->prefix_mods;
        it.ch = t->prefix_ch;
        return ev->type == TUI_EV_KEY && r2__tui_shortcut_match(&it, ev);
    }

    /* the key after the command prefix */
    static void r2__tui_chord_key(tui *t, const tui_event *ev)
    {
        t->chord = 0;
        t->compose_dirty = 1;
        if (ev->type != TUI_EV_KEY)
            return;
        if (r2__tui_is_prefix(t, ev))
        {
            /* the prefix twice: the window gets it */
            if (t->focus >= 0)
                r2__tui_run(t, t->focus, ev);
            else if (t->win[R2__TUI_DESKTOP].used)
                r2__tui_run(t, R2__TUI_DESKTOP, ev);
            return;
        }
        switch (ev->key)
        {
        case TUI_KEY_UP:
            if (t->focus >= 2 && !t->win[t->focus].zoomed)
                r2__tui_zoom(t);
            return;
        case TUI_KEY_DOWN:
            if (t->focus >= 2 && t->win[t->focus].zoomed)
                r2__tui_zoom(t);
            else
                r2__tui_minimize(t);
            return;
        case TUI_KEY_RIGHT: r2__tui_cycle(t, 1); return;
        case TUI_KEY_LEFT: r2__tui_cycle(t, -1); return;
        case TUI_KEY_TAB: r2__tui_cycle(t, (ev->mods & TUI_MOD_SHIFT) ? -1 : 1); return;
        case TUI_KEY_ENTER: r2__tui_start_move(t); return;
        case TUI_KEY_BACKSPACE:
        case TUI_KEY_DELETE: r2__tui_request_close(t); return;
        case TUI_KEY_RUNE:
            if (ev->mods & ~TUI_MOD_SHIFT)
                return;
            if (ev->ch == ' ')
                r2__tui_menu_open(t, 0);
            else if (ev->ch == '/')
                tui_palette(t);
            else if (ev->ch >= '1' && ev->ch <= '9')
            {
                int i;
                for (i = 2; i < R2__TUI_SLOTS; i++)
                    if (t->win[i].used && t->win[i].number == ev->ch - '0')
                        r2__tui_focus(t, i);
            }
            else
            {
                int m = r2__tui_menu_by_hot(t, ev->ch);
                if (m >= 0)
                    r2__tui_menu_open(t, m);
            }
            return;
        default:
            return; /* Esc or anything else: cancel */
        }
    }

    static void r2__tui_handle(tui *t, const tui_event *ev)
    {
        int modal = r2__tui_modal(t);
        if (ev->type == TUI_EV_RESIZE)
        {
            int cols = ev->x, rows = ev->y;
            while ((uint32_t)cols * (uint32_t)rows > t->cap_cells && rows > 1)
                rows--;
            tui_resize(t, cols, rows);
            return;
        }
        if (ev->type != TUI_EV_KEY)
            return;
        if (t->pal_open)
        {
            r2__tui_pal_key(t, ev);
            return;
        }
        if (t->menu_open)
        {
            r2__tui_menu_key(t, ev);
            return;
        }
        if (t->moving)
        {
            r2__tui_move_key(t, ev);
            return;
        }
        if (modal >= 0)
        {
            if (t->focus != modal)
                r2__tui_focus(t, modal);
            r2__tui_run(t, modal, ev);
            return;
        }
        if (t->chord)
        {
            r2__tui_chord_key(t, ev);
            return;
        }
        if (t->prefix_key && r2__tui_is_prefix(t, ev))
        {
            t->chord = 1;
            t->chord_time = t->now;
            t->compose_dirty = 1;
            return;
        }
        if (r2__tui_shortcut_is(t->pal_key, t->pal_mods, t->pal_ch, ev))
        {
            tui_palette(t);
            return;
        }
        /* window manager keys */
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 9, 0))
        {
            r2__tui_menu_open(t, r2__tui_bar(t, 0)->nmenus ? 1 : 0);
            return;
        }
        if (r2__tui_key_is(ev, TUI_KEY_RUNE, TUI_MOD_ALT))
        {
            int m = ev->ch == ' ' ? 0 : r2__tui_menu_by_hot(t, ev->ch);
            if (m >= 0)
            {
                r2__tui_menu_open(t, m);
                return;
            }
            if (ev->ch >= '1' && ev->ch <= '9')
            {
                int i;
                for (i = 2; i < R2__TUI_SLOTS; i++)
                    if (t->win[i].used && t->win[i].number == ev->ch - '0')
                        r2__tui_focus(t, i);
                return;
            }
        }
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 5, 0))
        {
            r2__tui_cycle(t, 1);
            return;
        }
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 5, TUI_MOD_SHIFT))
        {
            r2__tui_cycle(t, -1);
            return;
        }
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 4, TUI_MOD_CTRL))
        {
            r2__tui_start_move(t);
            return;
        }
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 4, 0))
        {
            r2__tui_zoom(t);
            return;
        }
        if (r2__tui_key_is(ev, TUI_KEY_F1 + 2, TUI_MOD_ALT))
        {
            r2__tui_request_close(t);
            return;
        }
        /* menu shortcuts */
        {
            const tui_menubar *bar = r2__tui_bar(t, 0);
            int i;
            for (i = 0; i < bar->nitems; i++)
            {
                const tui_item_rec *it = &bar->item[i];
                if (it->label != R2__TUI_SEP && !(it->flags & TUI_ITEM_DISABLED) && r2__tui_shortcut_match(it, ev))
                {
                    r2__tui_activate(t, i);
                    return;
                }
            }
        }
        /* the focused window, or the desktop */
        if (t->focus >= 0)
            r2__tui_run(t, t->focus, ev);
        else if (t->win[R2__TUI_DESKTOP].used)
            r2__tui_run(t, R2__TUI_DESKTOP, ev);
    }

    /* run what needs running (a controller may open windows or change
       things, so go round a few times) */
    static void r2__tui_settle(tui *t)
    {
        int pass, i;
        for (pass = 0; pass < 4; pass++)
        {
            int ran = 0;
            if (t->layout_dirty)
                r2__tui_layout(t);
            for (i = 0; i < R2__TUI_SLOTS; i++)
            {
                if (t->win[i].used && t->win[i].dirty)
                {
                    r2__tui_run(t, i, 0);
                    ran = 1;
                }
            }
            if (!ran)
                break;
        }
        if (t->layout_dirty)
            r2__tui_layout(t);
    }

    R2_TUI_DEF int tui_update(tui *t, uint32_t now_ms)
    {
        tui_event ev;
        t->now = now_ms;
        tui_step(t, now_ms);
        if (t->chord && (uint32_t)(now_ms - t->chord_time) >= R2_TUI_PREFIX_MS)
        {
            t->chord = 0; /* the prefix waited long enough */
            t->compose_dirty = 1;
        }
        /* Settle after every key: run what needs running before the next
           key is handled, so keys that arrive together (a serial line, a
           paste, typing ahead) behave exactly like keys typed one by one,
           e.g. a key that opens a dialog, then keys for the dialog. */
        r2__tui_settle(t);
        while (tui_poll(t, &ev))
        {
            r2__tui_handle(t, &ev);
            r2__tui_settle(t);
        }
        if (!t->compose_dirty)
            return 0;
        r2__tui_compose(t);
        return 1;
    }

#endif /* R2_TUI_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* R2_TUI_H */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Rob Rohan
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
