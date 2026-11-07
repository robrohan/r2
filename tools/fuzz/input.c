/* the key parser (tui_feed / tui_poll) on arbitrary bytes and timing */
#define R2_TUI_IMPLEMENTATION
#include "../../tui/r2_tui.h"
#include "fuzz.h"

static uint8_t arena[R2_TUI_ARENA_SIZE(80, 25)];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_stream s = {data, size, 0};
    uint8_t chunk[FUZZ_CHUNK_MAX], q[64];
    size_t n;
    uint32_t ms, now = 0;
    int what;
    tui t;
    tui_event ev;
    FUZZ_CHECK("init", tui_init(&t, arena, sizeof(arena), 80, 25) == 0);
    while ((what = fuzz_next(&s, chunk, &n, &ms)) != FUZZ_END)
    {
        if (what == FUZZ_QUERY)
            FUZZ_CHECK("query fits", tui_query_size(&t, q, sizeof(q)) > 0);
        else
        {
            tui_feed(&t, chunk, n, now);
            now += ms;
            tui_step(&t, now);
        }
        while (tui_poll(&t, &ev))
        {
            FUZZ_CHECK("event type", ev.type == TUI_EV_KEY || ev.type == TUI_EV_RESIZE);
            if (ev.type == TUI_EV_KEY)
            {
                FUZZ_CHECK("key in range", ev.key <= TUI_KEY_F12);
                FUZZ_CHECK("rune in range", ev.ch <= R2_RUNE_MAX);
            }
            else
                FUZZ_CHECK("resize is positive", ev.x > 0 && ev.y > 0);
        }
    }
    /* once the Esc timeout has passed nothing is left half parsed */
    tui_step(&t, now + 1000);
    while (tui_poll(&t, &ev))
        ;
    FUZZ_CHECK("input stuck", !tui_input_pending(&t));
    return 0;
}
