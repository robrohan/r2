/*
    Builds the freestanding headers for wasm32 with clang alone (no libc,
    no emcc): r2_strings.h, r2_quant.h (+ r2_simd.h, scalar path),
    r2_tui.h (+ r2_termui.h) and r2_tui_edit.h. `make wasm` links this
    with wasm-ld, which fails if anything needs a function the module
    doesn't define: that is
    the check that these headers stay usable bare-metal (Chestnut) and in a
    browser. r2_maths.h is left out because it needs libm and stdio.

    The exports are a small C ABI a host (e.g. a JS terminal) could drive:
    write key bytes into r2_in(), call r2_feed / r2_update, then read
    r2_render() bytes from r2_out().
*/
#define R2_STRINGS_IMPLEMENTATION
#include "../r2_strings.h"

#define R2_QUANT_IMPLEMENTATION
#include "../r2_quant.h"

#define R2_TUI_IMPLEMENTATION
#include "../r2_tui.h"

#define R2_TUI_EDIT_IMPLEMENTATION
#include "../r2_tui_edit.h"

#define EXPORT(name) __attribute__((export_name(#name)))

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

static char edit_buf[4096];
static tui_edit ed;

static void hello_ui(tui *tt, tui_window *w, void *user)
{
    int cw, ch;
    (void)w;
    (void)user;
    tui_menu_begin(tt, "~F~ile");
    tui_menu_item(tt, "~A~bout", 0);
    tui_menu_end(tt);
    tui_size(tt, &cw, &ch);
    tui_at(tt, 0, 0);
    tui_editor(tt, "##ed", &ed, cw, ch);
}

EXPORT(r2_init) int r2_init(int cols, int rows)
{
    if (tui_init(&t, arena, sizeof(arena), cols, rows) != 0)
        return -1;
    tui_edit_init(&ed, edit_buf, sizeof(edit_buf));
    tui_edit_set(&ed, S8("r2_tui in wasm32, no libc"));
    tui_window_open(&t, "Hello", (tui_rect){-1, -1, 40, 8}, 0, hello_ui, 0);
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
