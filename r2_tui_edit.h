/* r2_tui_edit - v0.1 - public domain multi-line text editor widget for r2_tui
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    A text editor as an r2_tui widget: call tui_editor() from a window's
    controller, like tui_input(). The text lives in a gap buffer in memory
    you provide (fixed size, no allocation), and the editor keeps the line
    count, the cursor's line and the first line on screen up to date as
    you edit, so nothing rescans the whole text on a key press.

    Do this:
       #define R2_TUI_EDIT_IMPLEMENTATION
    before you include this file in the *same* C file that has
    R2_TUI_IMPLEMENTATION (the editor uses r2_tui's internals).

        static char text[32 * 1024];
        static tui_edit ed;
        tui_edit_init(&ed, text, sizeof(text));
        tui_edit_set(&ed, S8("hello\nworld"));

        static void editor_window(tui *t, tui_window *w, void *user)
        {
            int cw, ch;
            tui_size(t, &cw, &ch);
            tui_at(t, 0, 0);
            if (tui_editor(t, "##ed", &ed, cw, ch) & TUI_CHANGED) ...
        }

    Keys: arrows, Home/End, PgUp/PgDn, Ctrl+Home/End (top/bottom),
    Ctrl+Left/Right (words), Enter, Tab, Backspace, Delete; and Emacs:
    Ctrl+A/E (line start/end), Ctrl+B/F, Ctrl+P/N, Ctrl+D, Ctrl+K (kill to
    the end of the line, or join it with the next). Shift+Tab leaves the
    editor for the next widget back.

    Esc then a key is Meta+key (as Emacs does in a terminal, so it works
    where Alt / Option doesn't): Esc f / Esc b words, Esc < / Esc > top and
    bottom, Esc v page up; Alt+key works too where the terminal sends it.

    Selection: Ctrl+Space sets the mark and moving extends the region
    (Emacs); Shift+movement selects too, and then a plain move clears it
    and typing replaces it (CUA). Ctrl+G or Esc Esc clears it.

    Undo (give the editor history memory with tui_edit_undo_init): Ctrl+/
    (the same key as Ctrl+_ in a terminal) or Ctrl+Z undoes; Esc Ctrl+/
    redoes (Emacs' C-M-_). Typing undoes up to 20 characters at a time,
    anything else one key at a time.

    Search (Emacs isearch): Ctrl+S forward, Ctrl+R backward; type to find,
    Ctrl+S / Ctrl+R again for the next match (again after a failure wraps
    around; on an empty search, the last one); Backspace shortens it, Enter
    or Esc stays, Ctrl+G goes back, any other key stops and does its job.
    An all-lower-case search ignores case. Esc % (M-%) is query replace:
    asks what (Enter alone: the last search) and with what (Enter alone:
    nothing, i.e. delete the matches), then y / Space replace, n / Backspace skip,
    ! replace the rest, . replace and stop, q / Enter / Esc stop.
    tui_edit_prompt() gives the line to show while searching.

    Clipboard (give the editor a tui_clipboard, see below): Ctrl+W kill (cut) the
    region, Ctrl+Y yank (paste), Esc w copy; Ctrl+K, Esc d and
    Esc Backspace kill into it too, and kills in a row join up. Borland
    keys: Shift+Del, Ctrl+Ins, Shift+Ins.

    Tabs show to the next multiple of e->tab columns (4 by default); wide
    and combining characters are handled like everywhere in r2_tui.

FEATURES
    Everything below is on unless you turn it off, so a small target can
    build a minimal editor. Define before including:

        R2_TUI_EDIT_NO_SELECTION    mark and Shift selection
        R2_TUI_EDIT_NO_CLIPBOARD    kill / yank / cut / copy / paste
        R2_TUI_EDIT_NO_UNDO         undo / redo
        R2_TUI_EDIT_NO_SEARCH       incremental search, query replace
        R2_TUI_EDIT_NO_HIGHLIGHT    per-line colouring callback  (next)
        R2_TUI_EDIT_MINIMAL         all of the above off

    The features marked (next) are not written yet.

CHANGES
    v0.4  incremental search (Ctrl+S / Ctrl+R) and query replace (Esc %)
    v0.3  undo / redo (Ctrl+/ or Ctrl+_ or Ctrl+Z; Esc Ctrl+/ redoes)
    v0.2  selection (mark, Shift), clipboard and kills, Esc as Meta
    v0.1  gap buffer, movement, editing, scrolling, line numbers

LICENSE
    See r2_tui.h for license information (this file is released under
    the same terms).
*/
#ifndef R2_TUI_EDIT_H
#define R2_TUI_EDIT_H

#include "r2_tui.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef R2_TUI_EDIT_MINIMAL
#ifndef R2_TUI_EDIT_NO_SELECTION
#define R2_TUI_EDIT_NO_SELECTION
#endif
#ifndef R2_TUI_EDIT_NO_CLIPBOARD
#define R2_TUI_EDIT_NO_CLIPBOARD
#endif
#ifndef R2_TUI_EDIT_NO_UNDO
#define R2_TUI_EDIT_NO_UNDO
#endif
#ifndef R2_TUI_EDIT_NO_SEARCH
#define R2_TUI_EDIT_NO_SEARCH
#endif
#ifndef R2_TUI_EDIT_NO_HIGHLIGHT
#define R2_TUI_EDIT_NO_HIGHLIGHT
#endif
#endif

/* longest search / replace text */
#ifndef R2_TUI_EDIT_QUERY
#define R2_TUI_EDIT_QUERY 64
#endif

/* editor flags */
#define TUI_EDIT_LINE_NUMBERS 1u /* a gutter with line numbers */
#define TUI_EDIT_READ_ONLY 2u    /* move around, no changes */
#define TUI_EDIT_AUTO_INDENT 4u  /* Enter copies the line's leading spaces / tabs */
#define TUI_EDIT_SPACES 8u       /* Tab inserts spaces to the next tab stop */

    /** A clipboard (kill buffer): memory you provide, shareable between editors. */
    typedef struct tui_clipboard
    {
        char *buf;
        uint32_t cap, len;
    } tui_clipboard;

    typedef struct tui_edit
    {
        char *buf;          /* caller's memory: text before the gap, gap, text after */
        uint32_t cap;
        uint32_t gap_start, gap_end;
        uint32_t cursor;    /* byte offset in the text */
        uint32_t line;      /* the cursor's line, from 0 */
        uint32_t lines;     /* number of lines (newlines + 1) */
        uint32_t top;       /* byte offset of the first line on screen */
        uint32_t top_line;  /* and its number */
        uint32_t left;      /* first column on screen */
        int32_t goal;       /* column Up / Down aim for; -1: the cursor's */
        uint32_t flags;     /* TUI_EDIT_* */
        uint8_t tab;        /* tab width */
        uint8_t modified;   /* set by any change; clear it yourself */
        uint8_t mark_on;    /* a region is selected: [mark, cursor) in either order */
        uint8_t mark_shift; /* it was made with Shift (CUA rules) */
        uint8_t meta;       /* Esc was pressed: the next key is Meta+key */
        uint8_t last_kill;  /* the last key killed text (the next kill joins it) */
        uint32_t mark;
        tui_clipboard *clip;     /* where kills and copies go; 0 for none */
        char *ubuf;         /* undo history: records [0, upos) can be undone, [upos, ulen) redone */
        uint32_t ucap, ulen, upos;
        uint32_t ugroup;    /* changes with the same group undo together */
        uint8_t undoing;    /* applying history: don't record it */
        uint8_t typing;     /* the last key typed a character (typing groups up) */
        uint8_t typed;      /* characters typed in this group */
#ifndef R2_TUI_EDIT_NO_SEARCH
        uint8_t search;     /* searching or replacing (R2__ED_SEARCH_*) */
        uint8_t failing, wrapped;
        uint32_t search_from;      /* the cursor when the search began */
        uint32_t match, match_len; /* the match on screen */
        uint32_t replaced;         /* how many the last query replace changed */
        char query[R2_TUI_EDIT_QUERY], last[R2_TUI_EDIT_QUERY], with[R2_TUI_EDIT_QUERY];
#endif
    } tui_edit;

    /** Use buf (cap bytes) for the text; starts empty. */
    R2_TUI_DEF void tui_edit_init(tui_edit *e, char *buf, uint32_t cap);
    /** Replace all the text (cursor to the start). 0 if it doesn't fit. */
    R2_TUI_DEF int tui_edit_set(tui_edit *e, s8 text);
    /** Length of the text in bytes. */
    R2_TUI_DEF uint32_t tui_edit_len(const tui_edit *e);
    /** Copy the text into out (NUL terminated, cut to cap - 1 bytes); returns the bytes copied. */
    R2_TUI_DEF uint32_t tui_edit_get(const tui_edit *e, char *out, uint32_t cap);
    /** Insert at the cursor (the cursor moves past it). 0 if it doesn't fit. */
    R2_TUI_DEF int tui_edit_insert(tui_edit *e, s8 text);
    /** Delete bytes [from, to) of the text. */
    R2_TUI_DEF void tui_edit_delete(tui_edit *e, uint32_t from, uint32_t to);
    /** Move the cursor to a byte offset (snapped back to a character start). */
    R2_TUI_DEF void tui_edit_goto(tui_edit *e, uint32_t offset);
    /** Display column of the cursor (tabs and wide characters counted). */
    R2_TUI_DEF uint32_t tui_edit_column(const tui_edit *e);
    /** Use buf (cap bytes) as a clipboard; set ed.clip = &clip to use it. */
    R2_TUI_DEF void tui_clipboard_init(tui_clipboard *c, char *buf, uint32_t cap);
    /** 1 and the region's byte range if something is selected. */
    R2_TUI_DEF int tui_edit_selection(const tui_edit *e, uint32_t *from, uint32_t *to);
    /** Copy / cut the region to e->clip (for menus); paste e->clip at the cursor (0 if it doesn't fit). */
    R2_TUI_DEF void tui_edit_copy(tui_edit *e);
    R2_TUI_DEF void tui_edit_cut(tui_edit *e);
    R2_TUI_DEF int tui_edit_paste(tui_edit *e);
    /** Keep undo history in buf (cap bytes); when it fills, the oldest goes. */
    R2_TUI_DEF void tui_edit_undo_init(tui_edit *e, char *buf, uint32_t cap);
    /** Undo / redo one step; 1 if there was something to undo / redo. */
    R2_TUI_DEF int tui_edit_undo(tui_edit *e);
    R2_TUI_DEF int tui_edit_redo(tui_edit *e);
    /** Start a new undo step (so the next changes don't undo with the last ones). */
    R2_TUI_DEF void tui_edit_undo_group(tui_edit *e);
    /** Start an incremental search (what Ctrl+S / Ctrl+R do), for menus. */
    R2_TUI_DEF void tui_edit_search(tui_edit *e, int backward);
    /** Find text from the cursor and select it; 1 if found. Smart case. */
    R2_TUI_DEF int tui_edit_find(tui_edit *e, s8 text, int backward);
    /** Start a query replace (what Esc % does), for menus. */
    R2_TUI_DEF void tui_edit_replace(tui_edit *e);
    /** The line to show while searching or replacing (a status line); 0 if none. */
    R2_TUI_DEF int tui_edit_prompt(const tui_edit *e, char *buf, size_t cap);
    /**
     * The editor widget, w x h cells at the layout cursor. Handles the keys
     * above while it has focus. Returns TUI_CHANGED when the text changed.
     */
    R2_TUI_DEF unsigned tui_editor(tui *t, const char *label, tui_edit *e, int w, int h);

/* ================================================================== */
#ifdef R2_TUI_EDIT_IMPLEMENTATION

#ifndef R2_TUI_IMPLEMENTATION
#error "r2_tui_edit.h: R2_TUI_EDIT_IMPLEMENTATION needs R2_TUI_IMPLEMENTATION in the same file"
#endif

    /* ---------------- the gap buffer ---------------- */

    static uint32_t r2__ed_gap(const tui_edit *e)
    {
        return e->gap_end - e->gap_start;
    }

    R2_TUI_DEF uint32_t tui_edit_len(const tui_edit *e)
    {
        return e->cap - r2__ed_gap(e);
    }

    /* byte i of the text */
    static uint8_t r2__ed_at(const tui_edit *e, uint32_t i)
    {
        return (uint8_t)e->buf[i < e->gap_start ? i : i + r2__ed_gap(e)];
    }

    static void r2__ed_move_gap(tui_edit *e, uint32_t pos)
    {
        if (pos < e->gap_start)
        {
            uint32_t n = e->gap_start - pos;
            while (n--)
                e->buf[--e->gap_end] = e->buf[--e->gap_start];
        }
        else
        {
            uint32_t n = pos - e->gap_start;
            while (n--)
                e->buf[e->gap_start++] = e->buf[e->gap_end++];
        }
    }

    R2_TUI_DEF void tui_edit_init(tui_edit *e, char *buf, uint32_t cap)
    {
        uint8_t *p = (uint8_t *)e;
        uint32_t i;
        for (i = 0; i < sizeof(*e); i++)
            p[i] = 0;
        e->buf = buf;
        e->cap = cap;
        e->gap_end = cap;
        e->lines = 1;
        e->goal = -1;
        e->tab = 4;
    }

    static uint32_t r2__ed_newlines(const tui_edit *e, uint32_t from, uint32_t to)
    {
        uint32_t n = 0;
        for (; from < to; from++)
            n += r2__ed_at(e, from) == '\n';
        return n;
    }

    static uint32_t r2__ed_line_start(const tui_edit *e, uint32_t off)
    {
        while (off > 0 && r2__ed_at(e, off - 1) != '\n')
            off--;
        return off;
    }

    static uint32_t r2__ed_line_end(const tui_edit *e, uint32_t off)
    {
        uint32_t len = tui_edit_len(e);
        while (off < len && r2__ed_at(e, off) != '\n')
            off++;
        return off;
    }

    /* the rune at off (U+FFFD for bad bytes); returns its length in bytes */
    static uint32_t r2__ed_rune(const tui_edit *e, uint32_t off, rune *r)
    {
        uint8_t b[4];
        uint32_t len = tui_edit_len(e), n = 0;
        while (n < 4 && off + n < len)
        {
            b[n] = r2__ed_at(e, off + n);
            n++;
        }
        if (n == 0)
        {
            *r = 0;
            return 0;
        }
        return utf8_decode(b, n, r);
    }

    static uint32_t r2__ed_next(const tui_edit *e, uint32_t off)
    {
        rune r;
        return off + r2__ed_rune(e, off, &r);
    }

    /* the start of the character before off: back over up to 3 continuation
       bytes, but only if decoding from there really ends at off (so bad bytes
       step one at a time, the same as going forward) */
    static uint32_t r2__ed_prev(const tui_edit *e, uint32_t off)
    {
        uint32_t at, k = 0;
        if (off == 0)
            return 0;
        at = off - 1;
        while (at > 0 && k < 3 && (r2__ed_at(e, at) & 0xC0u) == 0x80u)
        {
            at--;
            k++;
        }
        return r2__ed_next(e, at) == off ? at : off - 1;
    }

    /* columns rune r takes when it starts at column col */
    static uint32_t r2__ed_width(const tui_edit *e, rune r, uint32_t col)
    {
        if (r == '\t')
            return e->tab - col % e->tab;
        if (r < 0x20u || (r >= 0x7Fu && r < 0xA0u))
            return 1; /* drawn as ? */
        return (uint32_t)tui_rune_width(r);
    }

    static uint32_t r2__ed_col_of(const tui_edit *e, uint32_t off)
    {
        uint32_t at = r2__ed_line_start(e, off), col = 0;
        while (at < off)
        {
            rune r;
            uint32_t n = r2__ed_rune(e, at, &r);
            col += r2__ed_width(e, r, col);
            at += n;
        }
        return col;
    }

    R2_TUI_DEF uint32_t tui_edit_column(const tui_edit *e)
    {
        return r2__ed_col_of(e, e->cursor);
    }

    /* offset in the line starting at start that is at (or just before) column goal */
    static uint32_t r2__ed_off_at_col(const tui_edit *e, uint32_t start, uint32_t goal)
    {
        uint32_t at = start, col = 0, len = tui_edit_len(e);
        while (at < len)
        {
            rune r;
            uint32_t n = r2__ed_rune(e, at, &r), w;
            if (r == '\n')
                break;
            w = r2__ed_width(e, r, col);
            if (col + w > goal || (w == 0 && col == goal && at > start))
                break;
            col += w;
            at += n;
        }
        /* don't stop between a character and its combining marks */
        while (at < len)
        {
            rune r;
            uint32_t n = r2__ed_rune(e, at, &r);
            if (r == '\n' || r2__ed_width(e, r, col) != 0)
                break;
            at += n;
        }
        return at;
    }

    R2_TUI_DEF void tui_edit_goto(tui_edit *e, uint32_t offset)
    {
        uint32_t len = tui_edit_len(e);
        uint32_t at;
        if (offset > len)
            offset = len;
        /* snap to a character start, stepping the way forward movement does */
        at = r2__ed_line_start(e, offset);
        while (at < offset && r2__ed_next(e, at) <= offset)
            at = r2__ed_next(e, at);
        offset = at;
        if (offset > e->cursor)
            e->line += r2__ed_newlines(e, e->cursor, offset);
        else
            e->line -= r2__ed_newlines(e, offset, e->cursor);
        e->cursor = offset;
    }

    static void r2__ed_record(tui_edit *e, uint8_t kind, uint32_t pos, uint32_t len);
#define R2__ED_INSERT 1
#define R2__ED_DELETE 2

    R2_TUI_DEF int tui_edit_insert(tui_edit *e, s8 text)
    {
        uint32_t i, k = 0, at = e->cursor;
        if (text.len > r2__ed_gap(e))
            return 0;
        r2__ed_move_gap(e, e->cursor);
        for (i = 0; i < text.len; i++)
        {
            e->buf[e->gap_start++] = (char)text.data[i];
            k += text.data[i] == '\n';
        }
        if (e->cursor < e->top)
        {
            e->top += text.len;
            e->top_line += k;
        }
        if (e->mark > e->cursor)
            e->mark += text.len;
        e->cursor += text.len;
        e->line += k;
        e->lines += k;
        e->modified = 1;
        if (text.len)
            r2__ed_record(e, R2__ED_INSERT, at, text.len);
        return 1;
    }

    R2_TUI_DEF void tui_edit_delete(tui_edit *e, uint32_t from, uint32_t to)
    {
        uint32_t len = tui_edit_len(e), n, k;
        if (to > len)
            to = len;
        if (from >= to)
            return;
        n = to - from;
        k = r2__ed_newlines(e, from, to);
        r2__ed_record(e, R2__ED_DELETE, from, n); /* while the text is still there */
        /* the cursor and the top line keep their places in the text */
        if (e->cursor >= to)
        {
            e->cursor -= n;
            e->line -= k;
        }
        else if (e->cursor > from)
        {
            e->line -= r2__ed_newlines(e, from, e->cursor);
            e->cursor = from;
        }
        if (e->mark >= to)
            e->mark -= n;
        else if (e->mark > from)
            e->mark = from;
        if (e->top >= to)
        {
            e->top -= n;
            e->top_line -= k;
        }
        else if (e->top > from)
        {
            e->top_line -= r2__ed_newlines(e, from, e->top);
            e->top = from; /* fixed to a line start below */
        }
        r2__ed_move_gap(e, from);
        e->gap_end += n;
        e->lines -= k;
        e->top = r2__ed_line_start(e, e->top);
        e->modified = 1;
    }

    R2_TUI_DEF int tui_edit_set(tui_edit *e, s8 text)
    {
        if (text.len > e->cap)
            return 0;
        e->gap_start = 0;
        e->gap_end = e->cap;
        e->cursor = e->line = e->top = e->top_line = e->left = e->mark = 0;
        e->mark_on = e->meta = e->last_kill = 0;
        e->lines = 1;
        e->goal = -1;
#ifndef R2_TUI_EDIT_NO_SEARCH
        e->search = 0; /* no search */
#endif
        e->undoing = 1; /* a new text has no history */
        tui_edit_insert(e, text);
        e->undoing = 0;
        e->ulen = e->upos = 0;
        tui_edit_goto(e, 0);
        e->modified = 0;
        return 1;
    }

    R2_TUI_DEF uint32_t tui_edit_get(const tui_edit *e, char *out, uint32_t cap)
    {
        uint32_t len = tui_edit_len(e), i;
        if (cap == 0)
            return 0;
        if (len > cap - 1)
            len = cap - 1;
        for (i = 0; i < len; i++)
            out[i] = (char)r2__ed_at(e, i);
        out[len] = '\0';
        return len;
    }

    /* ---------------- moving ---------------- */

    static int r2__ed_word(rune r)
    {
        return r == '_' || (r >= '0' && r <= '9') || (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || r >= 0x80u;
    }

    static uint32_t r2__ed_word_right(const tui_edit *e, uint32_t at)
    {
        uint32_t len = tui_edit_len(e);
        rune r;
        while (at < len && (r2__ed_rune(e, at, &r), !r2__ed_word(r)))
            at = r2__ed_next(e, at);
        while (at < len && (r2__ed_rune(e, at, &r), r2__ed_word(r)))
            at = r2__ed_next(e, at);
        return at;
    }

    static uint32_t r2__ed_word_left(const tui_edit *e, uint32_t at)
    {
        rune r;
        while (at > 0 && (r2__ed_rune(e, r2__ed_prev(e, at), &r), !r2__ed_word(r)))
            at = r2__ed_prev(e, at);
        while (at > 0 && (r2__ed_rune(e, r2__ed_prev(e, at), &r), r2__ed_word(r)))
            at = r2__ed_prev(e, at);
        return at;
    }

    /* move by lines (negative: up), keeping the goal column */
    static void r2__ed_vertical(tui_edit *e, int dir, uint32_t count)
    {
        uint32_t start = r2__ed_line_start(e, e->cursor), len = tui_edit_len(e);
        if (e->goal < 0)
            e->goal = (int32_t)r2__ed_col_of(e, e->cursor);
        while (count--)
        {
            if (dir < 0)
            {
                if (start == 0)
                    break;
                start = r2__ed_line_start(e, start - 1);
            }
            else
            {
                uint32_t end = r2__ed_line_end(e, start);
                if (end >= len)
                    break;
                start = end + 1;
            }
        }
        tui_edit_goto(e, r2__ed_off_at_col(e, start, (uint32_t)e->goal));
    }

    /* ---------------- selection and clipboard ---------------- */

    R2_TUI_DEF void tui_clipboard_init(tui_clipboard *c, char *buf, uint32_t cap)
    {
        c->buf = buf;
        c->cap = cap;
        c->len = 0;
    }

    R2_TUI_DEF int tui_edit_selection(const tui_edit *e, uint32_t *from, uint32_t *to)
    {
#ifndef R2_TUI_EDIT_NO_SELECTION
        if (e->mark_on && e->mark != e->cursor)
        {
            *from = e->mark < e->cursor ? e->mark : e->cursor;
            *to = e->mark < e->cursor ? e->cursor : e->mark;
            return 1;
        }
#endif
        (void)e;
        *from = *to = 0;
        return 0;
    }

    /* text [from, to) into the clipboard: mode 0 replaces, 1 appends, 2 prepends.
       Whatever doesn't fit is left off, at a character boundary. */
    static void r2__ed_clip_put(tui_edit *e, uint32_t from, uint32_t to, int mode)
    {
#ifndef R2_TUI_EDIT_NO_CLIPBOARD
        tui_clipboard *c = e->clip;
        uint32_t n = to - from, i;
        if (!c || !c->buf || to <= from)
            return;
        if (mode == 0)
            c->len = 0;
        if (n > c->cap - c->len)
        {
            n = c->cap - c->len;
            while (n > 0 && (r2__ed_at(e, from + n) & 0xC0u) == 0x80u)
                n--;
        }
        if (mode == 2)
        {
            for (i = c->len; i-- > 0;)
                c->buf[i + n] = c->buf[i];
            for (i = 0; i < n; i++)
                c->buf[i] = (char)r2__ed_at(e, from + i);
        }
        else
            for (i = 0; i < n; i++)
                c->buf[c->len + i] = (char)r2__ed_at(e, from + i);
        c->len += n;
#else
        (void)e;
        (void)from;
        (void)to;
        (void)mode;
#endif
    }

    R2_TUI_DEF void tui_edit_copy(tui_edit *e)
    {
        uint32_t from, to;
        if (tui_edit_selection(e, &from, &to))
            r2__ed_clip_put(e, from, to, 0);
        e->mark_on = 0;
    }

    R2_TUI_DEF void tui_edit_cut(tui_edit *e)
    {
        uint32_t from, to;
        tui_edit_undo_group(e);
        if (!(e->flags & TUI_EDIT_READ_ONLY) && tui_edit_selection(e, &from, &to))
        {
            r2__ed_clip_put(e, from, to, 0);
            tui_edit_delete(e, from, to);
        }
        e->mark_on = 0;
    }

    R2_TUI_DEF int tui_edit_paste(tui_edit *e)
    {
#ifndef R2_TUI_EDIT_NO_CLIPBOARD
        if (e->clip && e->clip->buf && !(e->flags & TUI_EDIT_READ_ONLY))
        {
            e->mark_on = 0;
            tui_edit_undo_group(e);
            return tui_edit_insert(e, s8_from_bytes(e->clip->buf, e->clip->len));
        }
#endif
        (void)e;
        return 0;
    }

    /* ---------------- undo ----------------

    History is a log of records, oldest first:
        kind (1) | pos (4) | len (4) | group (4) | the text (len) | size (4)
    The size at the end lets undo walk backwards. An insert records the text
    it put in, a delete the text it took out. */

#define R2__ED_HDR 13u

    R2_TUI_DEF void r2__ed_put32(char *p, uint32_t v) /* (unused without undo) */
    {
        p[0] = (char)(v & 0xFFu);
        p[1] = (char)((v >> 8) & 0xFFu);
        p[2] = (char)((v >> 16) & 0xFFu);
        p[3] = (char)(v >> 24);
    }

    static uint32_t r2__ed_get32(const char *p)
    {
        const uint8_t *b = (const uint8_t *)p;
        return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    }

    R2_TUI_DEF void tui_edit_undo_init(tui_edit *e, char *buf, uint32_t cap)
    {
        e->ubuf = buf;
        e->ucap = cap;
        e->ulen = e->upos = 0;
    }

    R2_TUI_DEF void tui_edit_undo_group(tui_edit *e)
    {
        e->ugroup++;
        e->typing = 0;
        e->typed = 0;
    }

    static void r2__ed_record(tui_edit *e, uint8_t kind, uint32_t pos, uint32_t len)
    {
#ifndef R2_TUI_EDIT_NO_UNDO
        uint32_t need = R2__ED_HDR + len + 4u, i;
        char *r;
        if (!e->ubuf || e->undoing)
            return;
        e->ulen = e->upos; /* a new change ends redo */
        if (need > e->ucap)
        {
            e->ulen = e->upos = 0; /* too big to keep: the history before it is useless now */
            return;
        }
        /* typing right after typing in the same group: grow that record */
        if (kind == R2__ED_INSERT && e->ulen > 0)
        {
            uint32_t size = r2__ed_get32(e->ubuf + e->ulen - 4), start = e->ulen - size;
            char *last = e->ubuf + start;
            if ((uint8_t)last[0] == R2__ED_INSERT && r2__ed_get32(last + 9) == e->ugroup &&
                r2__ed_get32(last + 1) + r2__ed_get32(last + 5) == pos && e->ulen + len <= e->ucap)
            {
                uint32_t old = r2__ed_get32(last + 5);
                for (i = 0; i < len; i++)
                    e->ubuf[e->ulen - 4 + i] = (char)r2__ed_at(e, pos + i);
                r2__ed_put32(last + 5, old + len);
                e->ulen += len;
                r2__ed_put32(e->ubuf + e->ulen - 4, size + len);
                e->upos = e->ulen;
                return;
            }
        }
        /* make room: drop the oldest steps, whole (undoing part of a step
           would make text that never existed) */
        while (e->ulen > 0 && e->ulen + need > e->ucap)
        {
            uint32_t group = r2__ed_get32(e->ubuf + 9), first = 0;
            while (first < e->ulen && r2__ed_get32(e->ubuf + first + 9) == group)
                first += R2__ED_HDR + r2__ed_get32(e->ubuf + first + 5) + 4u;
            for (i = first; i < e->ulen; i++)
                e->ubuf[i - first] = e->ubuf[i];
            e->ulen -= first;
        }
        r = e->ubuf + e->ulen;
        r[0] = (char)kind;
        r2__ed_put32(r + 1, pos);
        r2__ed_put32(r + 5, len);
        r2__ed_put32(r + 9, e->ugroup);
        for (i = 0; i < len; i++)
            r[R2__ED_HDR + i] = (char)r2__ed_at(e, pos + i);
        r2__ed_put32(r + R2__ED_HDR + len, need);
        e->ulen += need;
        e->upos = e->ulen;
#else
        (void)e;
        (void)kind;
        (void)pos;
        (void)len;
#endif
    }

    /* apply one record forwards (redo) or backwards (undo) */
    static void r2__ed_apply(tui_edit *e, const char *r, int forward)
    {
        uint32_t pos = r2__ed_get32(r + 1), len = r2__ed_get32(r + 5);
        int insert = ((uint8_t)r[0] == R2__ED_INSERT) == forward;
        tui_edit_goto(e, pos);
        if (insert)
            tui_edit_insert(e, s8_from_bytes(r + R2__ED_HDR, len));
        else
            tui_edit_delete(e, pos, pos + len);
    }

    R2_TUI_DEF int tui_edit_undo(tui_edit *e)
    {
        uint32_t group;
        int any = 0;
        if (!e->ubuf || e->upos == 0)
            return 0;
        group = r2__ed_get32(e->ubuf + e->upos - r2__ed_get32(e->ubuf + e->upos - 4) + 9);
        e->undoing = 1;
        while (e->upos > 0)
        {
            uint32_t start = e->upos - r2__ed_get32(e->ubuf + e->upos - 4);
            if (r2__ed_get32(e->ubuf + start + 9) != group)
                break;
            r2__ed_apply(e, e->ubuf + start, 0);
            e->upos = start;
            any = 1;
        }
        e->undoing = 0;
        e->mark_on = 0;
        e->goal = -1;
        tui_edit_undo_group(e);
        return any;
    }

    R2_TUI_DEF int tui_edit_redo(tui_edit *e)
    {
        uint32_t group;
        int any = 0;
        if (!e->ubuf || e->upos >= e->ulen)
            return 0;
        group = r2__ed_get32(e->ubuf + e->upos + 9);
        e->undoing = 1;
        while (e->upos < e->ulen && r2__ed_get32(e->ubuf + e->upos + 9) == group)
        {
            r2__ed_apply(e, e->ubuf + e->upos, 1);
            e->upos += R2__ED_HDR + r2__ed_get32(e->ubuf + e->upos + 5) + 4u;
            any = 1;
        }
        e->undoing = 0;
        e->mark_on = 0;
        e->goal = -1;
        tui_edit_undo_group(e);
        return any;
    }

    /* ---------------- search ---------------- */

    enum
    {
        R2__ED_SEARCH_NONE,
        R2__ED_SEARCH_FWD,  /* incremental, forward */
        R2__ED_SEARCH_BACK, /* incremental, backward */
        R2__ED_ASK_FROM,    /* query replace: asking what to replace */
        R2__ED_ASK_TO,      /* ... and with what */
        R2__ED_REPLACING,   /* ... and going through the matches */
        R2__ED_REPLACED     /* finished: shows how many until the next key */
    };

#ifndef R2_TUI_EDIT_NO_SEARCH
    static size_t r2__ed_strlen(const char *s)
    {
        size_t n = 0;
        while (s[n])
            n++;
        return n;
    }

    /* smart case: a query with no capitals ignores case */
    static int r2__ed_folds(const char *q)
    {
        for (; *q; q++)
            if (*q >= 'A' && *q <= 'Z')
                return 0;
        return 1;
    }

    static int r2__ed_match_at(const tui_edit *e, uint32_t at, const char *q, uint32_t qn, int fold)
    {
        uint32_t i;
        if (at + qn > tui_edit_len(e))
            return 0;
        for (i = 0; i < qn; i++)
        {
            uint8_t a = r2__ed_at(e, at + i), b = (uint8_t)q[i];
            if (fold && a >= 'A' && a <= 'Z')
                a = (uint8_t)(a + 32);
            if (a != b)
                return 0;
        }
        return 1;
    }

    /* the first match starting at or after from (backward: at or before) */
    static int r2__ed_find(const tui_edit *e, const char *q, uint32_t from, int backward, uint32_t *found)
    {
        uint32_t qn = (uint32_t)r2__ed_strlen(q), len = tui_edit_len(e), at;
        int fold = r2__ed_folds(q);
        if (qn == 0 || qn > len)
            return 0;
        if (!backward)
        {
            for (at = from; at + qn <= len; at++)
                if ((r2__ed_at(e, at) & 0xC0u) != 0x80u && r2__ed_match_at(e, at, q, qn, fold))
                {
                    *found = at;
                    return 1;
                }
            return 0;
        }
        at = from < len - qn ? from : len - qn;
        for (;; at--)
        {
            if ((r2__ed_at(e, at) & 0xC0u) != 0x80u && r2__ed_match_at(e, at, q, qn, fold))
            {
                *found = at;
                return 1;
            }
            if (at == 0)
                return 0;
        }
    }

#endif

    R2_TUI_DEF int tui_edit_find(tui_edit *e, s8 text, int backward)
    {
#ifndef R2_TUI_EDIT_NO_SEARCH
        char q[R2_TUI_EDIT_QUERY];
        uint32_t i, at;
        if (text.len == 0 || text.len >= sizeof(q))
            return 0;
        for (i = 0; i < text.len; i++)
            q[i] = (char)text.data[i];
        q[text.len] = '\0';
        {
            /* backward starts before the selection (a previous match), forward after it */
            uint32_t from, to, start = e->cursor;
            if (backward && tui_edit_selection(e, &from, &to))
                start = from;
            if (backward && start == 0)
                return 0;
            if (!r2__ed_find(e, q, backward ? start - 1 : e->cursor, backward, &at))
                return 0;
        }
#ifndef R2_TUI_EDIT_NO_SELECTION
        e->mark = at;
        tui_edit_goto(e, at + text.len);
        e->mark_on = 1;
        e->mark_shift = 1;
#else
        /* nothing to select with: the cursor goes past the match, the way we are going */
        tui_edit_goto(e, backward ? at : at + text.len);
#endif
        return 1;
#else
        (void)e;
        (void)text;
        (void)backward;
        return 0;
#endif
    }

#ifndef R2_TUI_EDIT_NO_SEARCH
    /* show the match: the cursor at its end (forward) or start (backward) */
    static void r2__ed_show_match(tui_edit *e, uint32_t at)
    {
        e->match = at;
        e->match_len = (uint32_t)r2__ed_strlen(e->query);
        e->failing = 0;
        tui_edit_goto(e, e->search == R2__ED_SEARCH_BACK ? at : at + e->match_len);
    }

    /* look for the query again, from the current match (or the next one) */
    static void r2__ed_research(tui_edit *e, int next)
    {
        int back = e->search == R2__ED_SEARCH_BACK;
        uint32_t at, from;
        if (!e->query[0])
        {
            e->failing = 0;
            e->match_len = 0;
            tui_edit_goto(e, e->search_from);
            return;
        }
        if (next && e->failing)
        {
            /* again after a failure: wrap around */
            from = back ? tui_edit_len(e) : 0;
            e->wrapped = 1;
        }
        else if (next)
            from = back ? (e->match ? e->match - 1 : 0) : e->match + 1;
        else
            from = e->match;
        if (r2__ed_find(e, e->query, from, back, &at) && !(next && back && !e->wrapped && at >= e->match && e->match_len))
            r2__ed_show_match(e, at);
        else
            e->failing = 1;
    }
#endif

    R2_TUI_DEF void tui_edit_search(tui_edit *e, int backward)
    {
#ifndef R2_TUI_EDIT_NO_SEARCH
        e->search = backward ? R2__ED_SEARCH_BACK : R2__ED_SEARCH_FWD;
        e->search_from = e->match = e->cursor;
        e->match_len = 0;
        e->failing = e->wrapped = 0;
        e->query[0] = '\0';
        e->mark_on = 0;
#else
        (void)e;
        (void)backward;
#endif
    }

    R2_TUI_DEF void tui_edit_replace(tui_edit *e)
    {
#ifndef R2_TUI_EDIT_NO_SEARCH
        if (e->flags & TUI_EDIT_READ_ONLY)
            return;
        e->search = R2__ED_ASK_FROM;
        e->query[0] = '\0'; /* empty: Enter takes the default, the last search */
        e->with[0] = '\0';
        e->replaced = 0;
        e->mark_on = 0;
#else
        (void)e;
#endif
    }

    R2_TUI_DEF int tui_edit_prompt(const tui_edit *e, char *buf, size_t cap)
    {
#ifndef R2_TUI_EDIT_NO_SEARCH
        size_t n = 0;
        char num[12];
        uint32_t v, k = 0;
        if (!cap)
            return 0;
        buf[0] = '\0';
        switch (e->search)
        {
        case R2__ED_SEARCH_FWD:
        case R2__ED_SEARCH_BACK:
            if (e->failing)
                n = r2__tui_append(buf, n, cap, "Failing ", 0);
            if (e->wrapped)
                n = r2__tui_append(buf, n, cap, "Wrapped ", 0);
            n = r2__tui_append(buf, n, cap, e->search == R2__ED_SEARCH_BACK ? "I-search backward: " : "I-search: ", 0);
            r2__tui_append(buf, n, cap, e->query, 0);
            return 1;
        case R2__ED_REPLACING:
            n = r2__tui_append(buf, n, cap, "Replace \"", 0);
            n = r2__tui_append(buf, n, cap, e->query, 0);
            n = r2__tui_append(buf, n, cap, "\" with \"", 0);
            n = r2__tui_append(buf, n, cap, e->with, 0);
            r2__tui_append(buf, n, cap, "\"? y n ! . q", 0);
            return 1;
        case R2__ED_REPLACED:
            v = e->replaced;
            do
            {
                num[k++] = (char)('0' + v % 10);
                v /= 10;
            } while (v);
            n = r2__tui_append(buf, n, cap, "Replaced ", 0);
            while (k && n + 1 < cap)
                buf[n++] = num[--k];
            buf[n] = '\0';
            r2__tui_append(buf, n, cap, e->replaced == 1 ? " occurrence" : " occurrences", 0);
            return 1;
        default:
            return 0;
        }
#else
        (void)e;
        if (cap)
            buf[0] = '\0';
        return 0;
#endif
    }

#ifndef R2_TUI_EDIT_NO_SEARCH
    /* the next match for query replace (from the cursor); stops when there is none */
    static void r2__ed_replace_next(tui_edit *e)
    {
        uint32_t at;
        if (r2__ed_find(e, e->query, e->cursor, 0, &at))
        {
            e->match = at;
            e->match_len = (uint32_t)r2__ed_strlen(e->query);
            tui_edit_goto(e, at);
        }
        else
            e->search = R2__ED_REPLACED;
    }

    static void r2__ed_replace_one(tui_edit *e)
    {
        uint32_t wl = (uint32_t)r2__ed_strlen(e->with);
        tui_edit_goto(e, e->match);
        tui_edit_delete(e, e->match, e->match + e->match_len);
        if (!tui_edit_insert(e, s8_from_bytes(e->with, wl)))
        {
            e->search = R2__ED_REPLACED; /* full */
            return;
        }
        e->replaced++;
    }

    /* keys while searching / replacing; 1 if the key was taken */
    static int r2__ed_search_key(tui_edit *e, const tui_event *ev, unsigned *res)
    {
        int ctrl = ev->key == TUI_KEY_RUNE && ev->mods == TUI_MOD_CTRL;
        int plain = ev->key == TUI_KEY_RUNE && !(ev->mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && ev->ch >= 0x20u;
        if (e->search == R2__ED_REPLACED)
            e->search = R2__ED_SEARCH_NONE; /* the "Replaced N" note goes */
        if (e->search == R2__ED_SEARCH_FWD || e->search == R2__ED_SEARCH_BACK)
        {
            if (plain)
            {
                size_t n = r2__ed_strlen(e->query);
                uint8_t enc[4];
                uint32_t k = utf8_encode(ev->ch, enc), i;
                if (n + k < sizeof(e->query))
                {
                    for (i = 0; i < k; i++)
                        e->query[n + i] = (char)enc[i];
                    e->query[n + k] = '\0';
                }
                r2__ed_research(e, 0);
                return 1;
            }
            if (ctrl && (ev->ch == 's' || ev->ch == 'r'))
            {
                e->search = ev->ch == 'r' ? R2__ED_SEARCH_BACK : R2__ED_SEARCH_FWD;
                if (!e->query[0])
                    r2__tui_copy(e->query, sizeof(e->query), e->last); /* the last search again */
                r2__ed_research(e, e->match_len != 0 || e->failing);
                return 1;
            }
            if (ev->key == TUI_KEY_BACKSPACE && !ev->mods)
            {
                size_t n = r2__ed_strlen(e->query);
                if (n)
                {
                    n--;
                    while (n > 0 && ((uint8_t)e->query[n] & 0xC0u) == 0x80u)
                        n--;
                    e->query[n] = '\0';
                }
                e->match = e->search_from;
                e->wrapped = 0;
                r2__ed_research(e, 0);
                return 1;
            }
            if (ctrl && ev->ch == 'g')
            {
                e->search = R2__ED_SEARCH_NONE;
                tui_edit_goto(e, e->search_from);
                return 1;
            }
            /* anything else ends the search where it is (Enter / Esc do only that) */
            if (e->query[0])
                r2__tui_copy(e->last, sizeof(e->last), e->query);
            e->search = R2__ED_SEARCH_NONE;
            return (ev->key == TUI_KEY_ENTER || ev->key == TUI_KEY_ESC) && !ev->mods;
        }
        if (e->search == R2__ED_REPLACING)
        {
            rune c = plain ? r2__tui_lower(ev->ch) : 0;
            if (c == 'y' || c == ' ')
            {
                r2__ed_replace_one(e);
                *res = TUI_CHANGED;
                if (e->search == R2__ED_REPLACING)
                    r2__ed_replace_next(e);
            }
            else if (c == 'n' || (ev->key == TUI_KEY_BACKSPACE && !ev->mods) || (ev->key == TUI_KEY_DELETE && !ev->mods))
            {
                tui_edit_goto(e, e->match + e->match_len);
                r2__ed_replace_next(e);
            }
            else if (c == '!')
            {
                while (e->search == R2__ED_REPLACING)
                {
                    r2__ed_replace_one(e);
                    *res = TUI_CHANGED;
                    if (e->search == R2__ED_REPLACING)
                        r2__ed_replace_next(e);
                }
            }
            else if (c == '.')
            {
                r2__ed_replace_one(e);
                *res = TUI_CHANGED;
                e->search = R2__ED_REPLACED;
            }
            else
                e->search = R2__ED_REPLACED; /* q, Enter, Esc, Ctrl+G, anything: stop */
            return 1;
        }
        return 0;
    }
#endif

    /* ---------------- the widget ---------------- */

    /* the key while the editor has focus; returns TUI_CHANGED, *used if it took it */
    static unsigned r2__ed_basic_key(tui_edit *e, const tui_event *ev, int page, int *used)
    {
        uint32_t len = tui_edit_len(e), keep_goal = 0;
        unsigned res = 0;
        int ctrl = ev->mods == TUI_MOD_CTRL, ro = (e->flags & TUI_EDIT_READ_ONLY) != 0;
        int ctrl_rune = ev->key == TUI_KEY_RUNE && ctrl;
        *used = 1;
        switch (ev->key)
        {
        case TUI_KEY_LEFT:
            tui_edit_goto(e, ctrl ? r2__ed_word_left(e, e->cursor) : r2__ed_prev(e, e->cursor));
            break;
        case TUI_KEY_RIGHT:
            tui_edit_goto(e, ctrl ? r2__ed_word_right(e, e->cursor) : r2__ed_next(e, e->cursor));
            break;
        case TUI_KEY_UP:
            r2__ed_vertical(e, -1, 1);
            keep_goal = 1;
            break;
        case TUI_KEY_DOWN:
            r2__ed_vertical(e, 1, 1);
            keep_goal = 1;
            break;
        case TUI_KEY_PGUP:
            r2__ed_vertical(e, -1, (uint32_t)page);
            keep_goal = 1;
            break;
        case TUI_KEY_PGDN:
            r2__ed_vertical(e, 1, (uint32_t)page);
            keep_goal = 1;
            break;
        case TUI_KEY_HOME:
            tui_edit_goto(e, ctrl ? 0 : r2__ed_line_start(e, e->cursor));
            break;
        case TUI_KEY_END:
            tui_edit_goto(e, ctrl ? len : r2__ed_line_end(e, e->cursor));
            break;
        case TUI_KEY_ENTER:
            if (ev->mods || ro)
            {
                *used = 0;
                break;
            }
            {
                char ins[1 + 64];
                uint32_t n = 0, at = r2__ed_line_start(e, e->cursor);
                ins[n++] = '\n';
                if (e->flags & TUI_EDIT_AUTO_INDENT)
                    while (n < sizeof(ins) && at < e->cursor &&
                           (r2__ed_at(e, at) == ' ' || r2__ed_at(e, at) == '\t'))
                        ins[n++] = (char)r2__ed_at(e, at++);
                res = tui_edit_insert(e, s8_from_bytes(ins, n)) ? TUI_CHANGED : 0;
            }
            break;
        case TUI_KEY_TAB:
            if (ev->mods || ro)
            {
                *used = 0; /* Shift+Tab: leave for the previous widget */
                break;
            }
            if (e->flags & TUI_EDIT_SPACES)
            {
                static const char spaces[] = "                                ";
                uint32_t n = e->tab - tui_edit_column(e) % e->tab;
                res = tui_edit_insert(e, s8_from_bytes(spaces, n < 32 ? n : 32)) ? TUI_CHANGED : 0;
            }
            else
                res = tui_edit_insert(e, S8("\t")) ? TUI_CHANGED : 0;
            break;
        case TUI_KEY_BACKSPACE:
            if (!ro && e->cursor > 0)
            {
                tui_edit_delete(e, r2__ed_prev(e, e->cursor), e->cursor);
                res = TUI_CHANGED;
            }
            break;
        case TUI_KEY_DELETE:
            if (!ro && e->cursor < len)
            {
                tui_edit_delete(e, e->cursor, r2__ed_next(e, e->cursor));
                res = TUI_CHANGED;
            }
            break;
        case TUI_KEY_RUNE:
            if (ctrl_rune)
            {
                switch (ev->ch)
                {
                case 'a': tui_edit_goto(e, r2__ed_line_start(e, e->cursor)); break;
                case 'e': tui_edit_goto(e, r2__ed_line_end(e, e->cursor)); break;
                case 'b': tui_edit_goto(e, r2__ed_prev(e, e->cursor)); break;
                case 'f': tui_edit_goto(e, r2__ed_next(e, e->cursor)); break;
                case 'p': r2__ed_vertical(e, -1, 1); keep_goal = 1; break;
                case 'n': r2__ed_vertical(e, 1, 1); keep_goal = 1; break;
                case 'd':
                    if (!ro && e->cursor < len)
                    {
                        tui_edit_delete(e, e->cursor, r2__ed_next(e, e->cursor));
                        res = TUI_CHANGED;
                    }
                    break;
                case 'k':
                    if (!ro && e->cursor < len)
                    {
                        /* to the end of the line, or the newline itself at the end */
                        uint32_t end = r2__ed_line_end(e, e->cursor);
                        tui_edit_delete(e, e->cursor, end > e->cursor ? end : end + 1);
                        res = TUI_CHANGED;
                    }
                    break;
                default: *used = 0;
                }
            }
            else if (!(ev->mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && !ro && ev->ch >= 0x20u &&
                     !(ev->ch >= 0x7Fu && ev->ch < 0xA0u))
            {
                uint8_t enc[4];
                res = tui_edit_insert(e, s8_from_bytes(enc, utf8_encode(ev->ch, enc))) ? TUI_CHANGED : 0;
            }
            else
                *used = 0;
            break;
        default:
            *used = 0;
        }
        if (*used && !keep_goal)
            e->goal = -1;
        return res;
    }

    static int r2__ed_is(const tui_event *ev, uint16_t key, uint8_t mods, rune ch)
    {
        return ev->key == key && ev->mods == mods && (key != TUI_KEY_RUNE || r2__tui_lower(ev->ch) == ch);
    }

    /* Meta, selection and clipboard keys; everything else goes to r2__ed_basic_key */
    static unsigned r2__ed_key(tui_edit *e, const tui_event *in, int page, int *used)
    {
        tui_event ev = *in;
        unsigned res = 0;
        int ro = (e->flags & TUI_EDIT_READ_ONLY) != 0, kill = 0, movement;
        uint32_t from, to;
        *used = 1;

#ifndef R2_TUI_EDIT_NO_SEARCH
        if (e->search && r2__ed_search_key(e, &ev, &res))
            return res;
        if (!e->meta && r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 's'))
        {
            tui_edit_search(e, 0);
            return 0;
        }
        if (!e->meta && r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'r'))
        {
            tui_edit_search(e, 1);
            return 0;
        }
        if ((e->meta || ev.mods == TUI_MOD_ALT) && ev.key == TUI_KEY_RUNE && ev.ch == '%' && !ro)
        {
            e->meta = 0;
            tui_edit_replace(e);
            return 0;
        }
#endif

        /* Esc then a key is Meta+key */
        if (e->meta)
        {
            e->meta = 0;
            if (ev.key == TUI_KEY_ESC)
            {
                e->mark_on = 0; /* Esc Esc: clear the region */
                e->last_kill = 0;
                return 0;
            }
            ev.mods |= TUI_MOD_ALT;
        }
        else if (r2__ed_is(&ev, TUI_KEY_ESC, 0, 0))
        {
            /* always Meta (so Esc w copies an active region, as in Emacs) */
            e->meta = 1;
            return 0;
        }
#ifndef R2_TUI_EDIT_NO_UNDO
        if (r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, '_') || r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'z') ||
            r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL | TUI_MOD_ALT, '_'))
        {
            e->last_kill = 0;
            if (!ro && ((ev.mods & TUI_MOD_ALT) ? tui_edit_redo(e) : tui_edit_undo(e)))
                res = TUI_CHANGED;
            return res;
        }
#endif
        if (r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'g'))
        {
            e->mark_on = 0;
            e->last_kill = 0;
            return 0;
        }

        /* Meta movement is the same as its Ctrl key */
        if (ev.mods & TUI_MOD_ALT && ev.key == TUI_KEY_RUNE)
        {
            uint8_t keep = (uint8_t)(ev.mods & TUI_MOD_SHIFT);
            switch (r2__tui_lower(ev.ch))
            {
            case 'f': ev.key = TUI_KEY_RIGHT; ev.mods = (uint8_t)(TUI_MOD_CTRL | keep); break;
            case 'b': ev.key = TUI_KEY_LEFT; ev.mods = (uint8_t)(TUI_MOD_CTRL | keep); break;
            case '<': ev.key = TUI_KEY_HOME; ev.mods = TUI_MOD_CTRL; break;
            case '>': ev.key = TUI_KEY_END; ev.mods = TUI_MOD_CTRL; break;
            case 'v': ev.key = TUI_KEY_PGUP; ev.mods = keep; break;
            default: break;
            }
        }

        movement = ev.key == TUI_KEY_LEFT || ev.key == TUI_KEY_RIGHT || ev.key == TUI_KEY_UP ||
                   ev.key == TUI_KEY_DOWN || ev.key == TUI_KEY_HOME || ev.key == TUI_KEY_END ||
                   ev.key == TUI_KEY_PGUP || ev.key == TUI_KEY_PGDN ||
                   (ev.key == TUI_KEY_RUNE && (ev.mods & ~TUI_MOD_SHIFT) == TUI_MOD_CTRL &&
                    (ev.ch == 'a' || ev.ch == 'e' || ev.ch == 'b' || ev.ch == 'f' || ev.ch == 'p' || ev.ch == 'n'));
        (void)ro; /* unused when features are compiled out */
        (void)kill;
        (void)movement;
        (void)from;
        (void)to;

#ifndef R2_TUI_EDIT_NO_SELECTION
        if (r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, ' '))
        {
            e->mark = e->cursor;
            e->mark_on = 1;
            e->mark_shift = 0;
            e->last_kill = 0;
            return 0;
        }
        if (movement)
        {
            if (ev.mods & TUI_MOD_SHIFT)
            {
                if (!e->mark_on)
                {
                    e->mark = e->cursor;
                    e->mark_on = 1;
                    e->mark_shift = 1;
                }
                ev.mods &= (uint8_t)~TUI_MOD_SHIFT;
            }
            else if (e->mark_on && e->mark_shift)
                e->mark_on = 0;
        }
#endif

#ifndef R2_TUI_EDIT_NO_CLIPBOARD
        if (e->clip && e->clip->buf)
        {
            int copy = r2__ed_is(&ev, TUI_KEY_INSERT, TUI_MOD_CTRL, 0) || r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_ALT, 'w');
            int cut = r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'w') || r2__ed_is(&ev, TUI_KEY_DELETE, TUI_MOD_SHIFT, 0);
            if (copy)
            {
                tui_edit_copy(e);
                e->last_kill = 0;
                return 0;
            }
            if (cut)
            {
                if (!ro && tui_edit_selection(e, &from, &to))
                {
                    r2__ed_clip_put(e, from, to, e->last_kill ? (to == e->cursor ? 2 : 1) : 0);
                    tui_edit_delete(e, from, to);
                    res = TUI_CHANGED;
                    kill = 1;
                }
                e->mark_on = 0;
                e->last_kill = (uint8_t)kill;
                return res;
            }
            if (!ro && (r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'k') || r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_ALT, 'd') ||
                        r2__ed_is(&ev, TUI_KEY_BACKSPACE, TUI_MOD_ALT, 0)))
            {
                /* kills: to the end of the line (or the newline), or a word either way */
                if (ev.key == TUI_KEY_BACKSPACE)
                {
                    from = r2__ed_word_left(e, e->cursor);
                    to = e->cursor;
                }
                else
                {
                    from = e->cursor;
                    to = ev.ch == 'k' ? r2__ed_line_end(e, e->cursor) : r2__ed_word_right(e, e->cursor);
                    if (to == from && ev.ch == 'k' && to < tui_edit_len(e))
                        to++;
                }
                if (to > from)
                {
                    r2__ed_clip_put(e, from, to, e->last_kill ? (ev.key == TUI_KEY_BACKSPACE ? 2 : 1) : 0);
                    tui_edit_delete(e, from, to);
                    res = TUI_CHANGED;
                }
                e->mark_on = 0;
                e->goal = -1;
                e->last_kill = 1;
                return res;
            }
            if (r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'y') || r2__ed_is(&ev, TUI_KEY_INSERT, TUI_MOD_SHIFT, 0))
            {
#ifndef R2_TUI_EDIT_NO_SELECTION
                if (!ro && e->mark_shift && tui_edit_selection(e, &from, &to))
                    tui_edit_delete(e, from, to);
#endif
                if (tui_edit_paste(e))
                    res = TUI_CHANGED;
                e->mark_on = 0;
                e->goal = -1;
                e->last_kill = 0;
                return res;
            }
        }
#endif

#ifndef R2_TUI_EDIT_NO_SELECTION
        /* editing with a region: a Shift selection is replaced (CUA); either way the mark goes */
        if (!movement && e->mark_on && !ro)
        {
            int types = (ev.key == TUI_KEY_RUNE && !(ev.mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && ev.ch >= 0x20u) ||
                        r2__ed_is(&ev, TUI_KEY_ENTER, 0, 0) || r2__ed_is(&ev, TUI_KEY_TAB, 0, 0);
            int erases = r2__ed_is(&ev, TUI_KEY_BACKSPACE, 0, 0) || r2__ed_is(&ev, TUI_KEY_DELETE, 0, 0) ||
                         r2__ed_is(&ev, TUI_KEY_RUNE, TUI_MOD_CTRL, 'd');
            if (types || erases)
            {
                int had = e->mark_shift && tui_edit_selection(e, &from, &to);
                e->mark_on = 0;
                if (had)
                {
                    tui_edit_delete(e, from, to);
                    res = TUI_CHANGED;
                    if (erases)
                    {
                        e->goal = -1;
                        e->last_kill = 0;
                        return res;
                    }
                }
            }
        }
#endif
        res |= r2__ed_basic_key(e, &ev, page, used);
        e->last_kill = 0;
        return res;
    }

    /* keep the cursor on screen */
    static void r2__ed_scroll(tui_edit *e, uint32_t h, uint32_t tw)
    {
        uint32_t col;
        while (e->line < e->top_line)
        {
            e->top = r2__ed_line_start(e, e->top - 1);
            e->top_line--;
        }
        while (e->line >= e->top_line + h)
        {
            e->top = r2__ed_line_end(e, e->top) + 1;
            e->top_line++;
        }
        col = r2__ed_col_of(e, e->cursor);
        if (col < e->left)
            e->left = col;
        if (col >= e->left + tw)
            e->left = col - tw + 1;
    }

    R2_TUI_DEF unsigned tui_editor(tui *t, const char *label, tui_edit *e, int w, int h)
    {
        uint32_t id = r2__tui_wid(t, label);
        int focused = r2__tui_wreg(t, id, R2__TUI_W_TEXT);
        const tui_event *ev = tui_key(t);
        unsigned res = 0;
        int x, y, row, gutter = 0, tw, used;
        uint32_t off, len = tui_edit_len(e), s0 = 0, s1 = 0;
        tui_style body, sel_style;
        char tmp[512];
        if (w < 4)
            w = 4;
        if (h < 1)
            h = 1;
        if (r2__tui_hot_hit(t, label))
            r2__tui_wfocus(t, id, R2__TUI_W_TEXT);
        if (e->flags & TUI_EDIT_LINE_NUMBERS)
        {
            uint32_t n = e->lines;
            gutter = 2;
            while (n >= 10)
            {
                n /= 10;
                gutter++;
            }
        }
        tw = w - gutter;
        if (tw < 1)
            tw = 1;
#ifndef R2_TUI_EDIT_NO_SEARCH
        /* query replace asks with input boxes (modal: they get the keys) */
        if (e->search == R2__ED_ASK_FROM)
        {
            /* "Replace (default foo):" as Emacs asks: Enter on nothing takes foo */
            char prompt[R2_TUI_EDIT_QUERY + 32];
            size_t n = r2__tui_append(prompt, 0, sizeof(prompt), "~R~eplace", 0);
            unsigned r;
            if (e->last[0])
            {
                n = r2__tui_append(prompt, n, sizeof(prompt), " (default ", 0);
                n = r2__tui_append(prompt, n, sizeof(prompt), e->last, 0);
                n = r2__tui_append(prompt, n, sizeof(prompt), ")", 0);
            }
            r2__tui_append(prompt, n, sizeof(prompt), ":", 0);
            r = tui_input_box(t, "Replace", prompt, e->query, sizeof(e->query));
            if (r == TUI_MB_OK && !e->query[0])
                r2__tui_copy(e->query, sizeof(e->query), e->last);
            if (r == TUI_MB_OK && e->query[0])
            {
                r2__tui_copy(e->last, sizeof(e->last), e->query);
                e->search = R2__ED_ASK_TO;
            }
            else if (r)
                e->search = R2__ED_SEARCH_NONE;
        }
        if (e->search == R2__ED_ASK_TO)
        {
            unsigned r = tui_input_box(t, "Replace with", "~W~ith:", e->with, sizeof(e->with));
            if (r == TUI_MB_OK)
            {
                e->search = R2__ED_REPLACING;
                e->replaced = 0;
                r2__ed_replace_next(e);
            }
            else if (r)
                e->search = R2__ED_SEARCH_NONE;
        }
#endif
        if (focused && ev)
        {
            int typing = ev->key == TUI_KEY_RUNE && !(ev->mods & (TUI_MOD_CTRL | TUI_MOD_ALT)) && !e->meta;
#ifndef R2_TUI_EDIT_NO_SEARCH
            if (e->search)
                typing = 0; /* searching / replacing: each key is its own undo step */
#endif
            if (!(typing && e->typing && e->typed < 20))
                tui_edit_undo_group(e);
            e->typing = (uint8_t)typing;
            if (typing)
                e->typed++;
            res = r2__ed_key(e, ev, h > 1 ? h - 1 : 1, &used);
            if (used)
                tui_use_key(t);
            len = tui_edit_len(e);
        }
        r2__ed_scroll(e, (uint32_t)h, (uint32_t)tw);
        r2__tui_place(t, w, h, &x, &y);
        tui_fill(t, (tui_rect){x, y, w, h}, ' ', TUI_DEFAULT);
        tui_edit_selection(e, &s0, &s1);
#ifndef R2_TUI_EDIT_NO_SEARCH
        if ((e->search == R2__ED_SEARCH_FWD || e->search == R2__ED_SEARCH_BACK || e->search == R2__ED_REPLACING) &&
            e->match_len && !e->failing)
        {
            s0 = e->match;
            s1 = e->match + e->match_len;
        }
#endif
        body = t->cur >= 0 ? r2__tui_body(t, t->cur) : t->theme->body;
        sel_style = TUI_STYLE(TUI_STYLE_FG(body), TUI_STYLE_BG(body), TUI_STYLE_ATTR(body) | TUI_REVERSE);

        off = e->top;
        for (row = 0; row < h; row++)
        {
            uint32_t col = 0, n = 0, at = off;
            int first = 1, seg_sel = 0, seg_x = 0;
            if (e->top_line + (uint32_t)row >= e->lines)
                break;
            if (gutter)
            {
                char num[12];
                uint32_t v = e->top_line + (uint32_t)row + 1, k = 0, i;
                char rev[12];
                while (v)
                {
                    rev[k++] = (char)('0' + v % 10);
                    v /= 10;
                }
                for (i = 0; i < k; i++)
                    num[i] = rev[k - 1 - i];
                tui_text(t, x + gutter - 1 - (int)k, y + row, s8_from_bytes(num, k),
                         r2__tui_on_body(t, t->theme->frame));
            }
            /* the visible columns [left, left + tw) of this line, in runs of
               selected / not selected text */
            while (at < len)
            {
                rune r;
                uint32_t rn = r2__ed_rune(e, at, &r), rw;
                int sel = at >= s0 && at < s1;
                if (r == '\n')
                    break;
                rw = r2__ed_width(e, r, col);
                if (col + rw > e->left + (uint32_t)tw)
                    break;
                if (rw != 0 && sel != seg_sel && n)
                {
                    tui_text(t, x + gutter + seg_x, y + row, s8_from_bytes(tmp, n), seg_sel ? sel_style : TUI_DEFAULT);
                    n = 0;
                }
                if (n == 0 && rw != 0)
                {
                    seg_sel = sel;
                    seg_x = col >= e->left ? (int)(col - e->left) : 0;
                }
                if (col >= e->left || (rw == 0 && !first))
                {
                    if (r == '\t')
                    {
                        uint32_t k;
                        for (k = 0; k < rw && n < sizeof(tmp); k++)
                            tmp[n++] = ' ';
                    }
                    else if (n + rn <= sizeof(tmp))
                    {
                        uint32_t k;
                        for (k = 0; k < rn; k++)
                            tmp[n++] = (char)r2__ed_at(e, at + k);
                    }
                    first = 0;
                }
                else if (col + rw > e->left)
                {
                    /* a tab or wide character cut by the left edge: blanks */
                    uint32_t k;
                    for (k = e->left; k < col + rw && n < sizeof(tmp); k++)
                        tmp[n++] = ' ';
                    first = 0;
                }
                col += rw;
                at += rn;
            }
            if (n)
                tui_text(t, x + gutter + seg_x, y + row, s8_from_bytes(tmp, n), seg_sel ? sel_style : TUI_DEFAULT);
            /* a selected line break shows as one selected blank after the text */
            if (at < len && at >= s0 && at < s1 && col >= e->left && col - e->left < (uint32_t)tw)
                tui_text(t, x + gutter + (int)(col - e->left), y + row, S8(" "), sel_style);
            off = r2__ed_line_end(e, off) + 1;
        }
        if (focused)
            tui_cursor(t, x + gutter + (int)(r2__ed_col_of(e, e->cursor) - e->left), y + (int)(e->line - e->top_line));
        return res;
    }

#endif /* R2_TUI_EDIT_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* R2_TUI_EDIT_H */
