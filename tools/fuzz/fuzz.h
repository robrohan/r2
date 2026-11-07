/*
    Shared by the libFuzzer harnesses in this folder (make fuzz).

    The input harnesses read the fuzz data as a stream of terminal bytes
    with 0xFF as an escape, so the fuzzer controls how the bytes are split
    between tui_feed calls and how much time passes between them:

        0xFF 0xFF   a literal 0xFF byte
        0xFF 0xFE   send a size query (so a CSI row;col R is an answer)
        0xFF n      end of this chunk, then n * 4 ms pass
        anything    a byte of the current chunk

    A chunk also ends at the end of the data (with 100 ms passing, so a
    lone Esc times out).
*/
#ifndef R2_FUZZ_H
#define R2_FUZZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* a broken invariant: report it and abort, which libFuzzer saves as a crash */
#define FUZZ_CHECK(what, cond)                                                     \
    do                                                                             \
    {                                                                              \
        if (!(cond))                                                               \
        {                                                                          \
            fprintf(stderr, "%s:%d: invariant broken: %s\n", __FILE__, __LINE__, what); \
            abort();                                                               \
        }                                                                          \
    } while (0)

#define FUZZ_CHUNK_MAX 256

typedef struct fuzz_stream
{
    const uint8_t *data;
    size_t size, at;
} fuzz_stream;

enum
{
    FUZZ_END,   /* no more data */
    FUZZ_BYTES, /* feed chunk[0 .. n), then let ms pass */
    FUZZ_QUERY  /* send a size query */
};

/* the next thing to do; chunk holds FUZZ_CHUNK_MAX bytes */
static int fuzz_next(fuzz_stream *s, uint8_t *chunk, size_t *n, uint32_t *ms)
{
    *n = 0;
    *ms = 0;
    if (s->at >= s->size)
        return FUZZ_END;
    while (s->at < s->size && *n < FUZZ_CHUNK_MAX)
    {
        uint8_t b = s->data[s->at++];
        if (b != 0xFF)
        {
            chunk[(*n)++] = b;
            continue;
        }
        if (s->at >= s->size)
            break;
        b = s->data[s->at++];
        if (b == 0xFF)
            chunk[(*n)++] = 0xFF;
        else if (b == 0xFE)
        {
            if (*n == 0)
                return FUZZ_QUERY;
            s->at -= 2; /* feed what we have first, then the query */
            return FUZZ_BYTES;
        }
        else
        {
            *ms = (uint32_t)b * 4;
            return FUZZ_BYTES;
        }
    }
    *ms = s->at >= s->size ? 100 : 0;
    return FUZZ_BYTES;
}

#ifdef R2_TUI_H
/* drain tui_render; the output must be well formed UTF-8 (escape sequences
   are ASCII and every cell is encoded from a rune). Checked as a whole: a
   rune may be split between two tui_render calls. */
static void fuzz_render(tui *t)
{
    static uint8_t out[1 << 16];
    size_t n, len = 0;
    while ((n = tui_render(t, out + len, sizeof(out) - len)) > 0)
    {
        len += n;
        FUZZ_CHECK("one frame fits in 64KB", sizeof(out) - len >= R2_TUI_RENDER_MIN);
    }
    FUZZ_CHECK("render drained", !tui_render_pending(t));
    FUZZ_CHECK("render output is UTF-8", s8_is_valid_utf8(s8_from_bytes(out, (uint32_t)len)));
}
#endif

#endif /* R2_FUZZ_H */
