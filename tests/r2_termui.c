#include "../r2_termui.h"
#include <stdio.h>
#include <unistd.h>
#include "../r2_unit.h"

static const char *test_simple_test(void)
{
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    sleep(1);
    // printf(ESC_ERASE_SCREEN);
    printf(ESC_CURSOR_POS, 10, 10);
    unsigned int pixel_color = 34; // blue
    printf(ESC_SET_ATTRIBUTE_MODE_1, pixel_color);
    printf("%s", "Hello World\n");
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n\n\n");
    printf(ESC_SHOW_CURSOR);
    sleep(1);
    r2_assert("display things", 1 == 1);
    return 0;
}

static const char *r2_termui_test(void)
{
    r2_run_test(test_simple_test);
    return 0;
}
