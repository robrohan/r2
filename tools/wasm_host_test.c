/*
    Drives tools/wasm_check.c's exports natively, the way
    examples/wasm/index.html does: keys in, bytes out, then reads the
    library's cells. `make test_wasm` runs it under ASan / UBSan.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define memset wc_memset
#define memcpy wc_memcpy
#define memmove wc_memmove
#define memcmp wc_memcmp
#include "wasm_check.c"
#undef memset
#undef memcpy
#undef memmove
#undef memcmp

static uint32_t now = 1000;
static size_t total;
static void drain(void) { int n; while ((n = r2_render()) > 0) total += (size_t)n; }
static void keys(const char *k, size_t n)
{
    while (n) {
        size_t c = n > 256 ? 256 : n;
        memcpy(r2_in(), k, c);
        r2_feed((int)c, now);
        k += c; n -= c;
    }
    r2_update(now);
    now += 100;
    r2_update(now); /* lets a lone Esc time out */
    drain();
}
#define K(s) keys(s, sizeof(s) - 1)
static int has(const char *text)
{
    for (int y = 0; y < t.rows; y++) {
        char row[1024]; size_t n = 0;
        for (int x = 0; x < t.cols; x++) {
            uint32_t r = TUI_CELL_RUNE(r2_cell(x, y));
            if (r != TUI_RUNE_CONT) n += utf8_encode(r ? r : ' ', (uint8_t *)row + n);
        }
        row[n] = 0;
        if (strstr(row, text)) return 1;
    }
    return 0;
}
static int fails;
#define CHECK(what, cond) do { int ok_ = (cond); printf("%-44s %s\n", what, ok_ ? "ok" : "FAIL"); fails += !ok_; } while (0)

int main(void)
{
    CHECK("init 110x30", r2_init(110, 30) == 0);
    r2_update(now); drain();
    CHECK("first frame sent", total > 1000);
    CHECK("editor open with text", has("r2_tui in wasm32"));
    CHECK("widgets window open, beside the editor", has("Widgets") && TUI_CELL_RUNE(r2_cell(68, 2)) != 0x2591);
    CHECK("status shows line/col", has("Line 1  Col 1"));
    CHECK("width export", r2_width(0x4E2D) == 2 && r2_width('a') == 1 && r2_width(0x301) == 0);
    K("\x05" "!"); CHECK("typing edits (Ctrl+E, !)", has("imported.!"));
    K("\x1f"); CHECK("Ctrl+/ undoes", !has("imported.!"));
    K("\x13" "Emacs"); CHECK("Ctrl+S isearch prompt", has("I-search: Emacs"));
    K("\r"); CHECK("Enter ends search", has("Line 3"));
    K("\x1b" "[20~"); CHECK("F9 focuses Widgets: its status", has("Alt+letter"));
    K("\x1b" "n\x01\x0b" "Rob"); CHECK("Alt+N to name, type", has("Rob"));
    K("\x1b" "s\x1b" "g"); CHECK("shout + greet: message", has("HELLO, Rob!!!"));
    K("\r"); CHECK("OK closes it", !has("HELLO, Rob"));
    K("\x1b" "c"); CHECK("Close closes Widgets", !has("S~h~out") && !has("Shout it"));
    K("\x1b" "[20~"); CHECK("F9 opens it again", has("Shout it"));
    K("\x1d" "about\r");
CHECK("palette runs About", has("This page is the terminal"));
    K("\r");
    CHECK("resize 140x45", r2_resize(140, 45) == 0);
    r2_update(now); drain();
    CHECK("redrawn at the new size", has("Editor") && has("F9~") == 0 && has("Widgets"));
    CHECK("too big for the arena fails", r2_resize(400, 200) != 0);
    /* random keys: nothing breaks (ASan / UBSan), the screen stays sane */
    srand(7);
    static const char *const special[] = {"\x1b[A", "\x1b[B", "\x1b[C", "\x1b[D", "\r", "\t", "\x1b[Z", "\x7f",
                                          "\x1b", "\x1b[3~", "\x1b[5~", "\x1b[6~", "\x1b[21~", "\x1b[19~", "\x1c",
                                          "\x1d", "\x13", "\x12", "\x00", "\x17", "\x19", "\x1f", "\x07", "\x11"};
    for (int i = 0; i < 20000; i++) {
        char k[8]; size_t n;
        int c = rand() % 4;
        if (c == 0) { const char *s = special[rand() % 24]; n = s[0] ? strlen(s) : 1; memcpy(k, s, n); }
        else if (c == 1) { k[0] = (char)(1 + rand() % 26); n = 1; }
        else { k[0] = (char)(' ' + rand() % 95); n = 1; }
        keys(k, n);
        if (i % 5000 == 4999) r2_resize(60 + rand() % 100, 15 + rand() % 45);
    }
    K("\x1b"); K("\x1b"); K("\x07");
    CHECK("20000 random keys: still a menu bar", TUI_CELL_RUNE(r2_cell(2, 0)) == 0x2261);
    printf("%s, %zu bytes rendered in all\n", fails ? "FAILED" : "PASSED", total);
    return fails != 0;
}
