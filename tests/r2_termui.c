#include "../r2_termui.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../r2_unit.h"

static const char *test_simple_test(void)
{
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    sleep(1);
    printf(ESC_ERASE_SCREEN);
    printf(ESC_CURSOR_POS, 10, 10);
    unsigned int pixel_color = 34; // blue
    printf(ESC_SET_ATTRIBUTE_MODE_1, pixel_color);
    printf("%s", "Hello World\n");
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n\n\n");
    printf(ESC_SHOW_CURSOR);
    sleep(1);
    printf(ESC_ERASE_SCREEN);
    printf(ESC_CURSOR_POS, 0, 0);
    r2_assert("display things", 1 == 1);
    return 0;
}

/* Draws the 256-colour grayscale ramp (indices 232-255). Each block is two
   spaces wide so the band is visible even in a narrow terminal. */
static const char *test_256_grayscale_gradient(void)
{
    printf("\n  256-colour grayscale (232-255):\n  ");
    for (int i = 232; i <= 255; i++)
    {
        printf(ESC_SET_BG_256, i);
        printf("  ");
    }
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n");
    r2_assert("256 grayscale gradient rendered", 1 == 1);
    return 0;
}

/* Draws a slice through the 256-colour cube: a row of 36 colours sweeping
   from pure red (196) through yellow (226) to pure green (46). */
static const char *test_256_color_gradient(void)
{
    /* Indices 196-231 form a 6x6x6 colour cube in 256-colour mode.
       Stepping along the red-to-green edge gives a visible warm gradient. */
    static const int stops[] = {
        196, 202, 208, 214, 220, 226,   /* red -> yellow */
        154, 118,  82,  46              /* yellow -> green */
    };
    int n = (int)(sizeof stops / sizeof stops[0]);

    printf("\n  256-colour red-yellow-green:\n  ");
    for (int i = 0; i < n; i++)
    {
        printf(ESC_SET_BG_256, stops[i]);
        printf("    ");
    }
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n");
    r2_assert("256 colour gradient rendered", 1 == 1);
    return 0;
}

/* Draws a true-colour red -> blue gradient across 64 steps. */
static const char *test_rgb_red_to_blue(void)
{
    int steps = 64;
    printf("\n  RGB red -> blue (%d steps):\n  ", steps);
    for (int i = 0; i <= steps; i++)
    {
        int r = 255 - (255 * i / steps);
        int b = 255 * i / steps;
        printf(ESC_SET_BG_RGB, r, 0, b);
        printf(" ");
    }
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n");
    r2_assert("RGB red-to-blue gradient rendered", 1 == 1);
    return 0;
}

/* Draws a full-hue rainbow using true colour by sweeping through six
   colour sectors (red->yellow->green->cyan->blue->magenta) with 12 steps
   per sector, 72 blocks total. Uses integer arithmetic only. */
static const char *test_rgb_rainbow(void)
{
    int steps_per_sector = 12;
    int total = steps_per_sector * 6;

    printf("\n  RGB rainbow (%d steps):\n  ", total);
    for (int i = 0; i < total; i++)
    {
        int sector = i / steps_per_sector;
        int t      = (255 * (i % steps_per_sector)) / (steps_per_sector - 1);
        int q      = 255 - t;
        int r = 0, g = 0, b = 0;
        switch (sector)
        {
            case 0: r = 255; g = t;   b = 0;   break; /* red    -> yellow  */
            case 1: r = q;   g = 255; b = 0;   break; /* yellow -> green   */
            case 2: r = 0;   g = 255; b = t;   break; /* green  -> cyan    */
            case 3: r = 0;   g = q;   b = 255; break; /* cyan   -> blue    */
            case 4: r = t;   g = 0;   b = 255; break; /* blue   -> magenta */
            case 5: r = 255; g = 0;   b = q;   break; /* magenta-> red     */
        }
        printf(ESC_SET_BG_RGB, r, g, b);
        printf(" ");
    }
    printf(ESC_SET_ATTRIBUTE_MODE_1, 0);
    printf("\n");
    r2_assert("RGB rainbow gradient rendered", 1 == 1);
    return 0;
}

/* Non-visual checks: the format macros expand to the exact VT100 / ANSI
   sequences (and the compiler checks their %d counts via -Wformat). */
static const char *test_escape_sequences_exact(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), ESC_CURSOR_POS, 12, 34);
    r2_assert("ESC_CURSOR_POS is wrong", strcmp(buf, "\033[12;34H") == 0);
    snprintf(buf, sizeof(buf), ESC_FORCE_CURSOR_POS, 1, 2);
    r2_assert("ESC_FORCE_CURSOR_POS is wrong", strcmp(buf, "\033[1;2f") == 0);
    snprintf(buf, sizeof(buf), ESC_CURSOR_UP, 3);
    r2_assert("ESC_CURSOR_UP is wrong", strcmp(buf, "\033[3A") == 0);
    snprintf(buf, sizeof(buf), ESC_CURSOR_DOWN, 3);
    r2_assert("ESC_CURSOR_DOWN is wrong", strcmp(buf, "\033[3B") == 0);
    snprintf(buf, sizeof(buf), ESC_CURSOR_FORWARD, 3);
    r2_assert("ESC_CURSOR_FORWARD is wrong", strcmp(buf, "\033[3C") == 0);
    snprintf(buf, sizeof(buf), ESC_CURSOR_BACKWARD, 3);
    r2_assert("ESC_CURSOR_BACKWARD is wrong", strcmp(buf, "\033[3D") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_ATTRIBUTE_MODE_3, 1, 31, 40);
    r2_assert("ESC_SET_ATTRIBUTE_MODE_3 is wrong", strcmp(buf, "\033[1;31;40m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_ATTRIBUTE_MODE_2, 1, 31);
    r2_assert("ESC_SET_ATTRIBUTE_MODE_2 is wrong", strcmp(buf, "\033[1;31m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_ATTRIBUTE_MODE_1, 0);
    r2_assert("ESC_SET_ATTRIBUTE_MODE_1 is wrong", strcmp(buf, "\033[0m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_FG_256, 196);
    r2_assert("ESC_SET_FG_256 is wrong", strcmp(buf, "\033[38;5;196m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_BG_256, 232);
    r2_assert("ESC_SET_BG_256 is wrong", strcmp(buf, "\033[48;5;232m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_FG_RGB, 255, 128, 0);
    r2_assert("ESC_SET_FG_RGB is wrong", strcmp(buf, "\033[38;2;255;128;0m") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_BG_RGB, 0, 0, 255);
    r2_assert("ESC_SET_BG_RGB is wrong", strcmp(buf, "\033[48;2;0;0;255m") == 0);
    snprintf(buf, sizeof(buf), ESC_REPORT_CURSOR_POSITION, 5, 6);
    r2_assert("ESC_REPORT_CURSOR_POSITION is wrong", strcmp(buf, "\033[5;6R") == 0);
    snprintf(buf, sizeof(buf), ESC_SET_KEY_DEFINITION, 65, "ls");
    r2_assert("ESC_SET_KEY_DEFINITION is wrong", strcmp(buf, "\033[65;\"ls\"p") == 0);

    r2_assert("ESC_START is wrong", ESC_START == 0x1B);
    r2_assert("ESC_CURSOR_HOME is wrong", strcmp(ESC_CURSOR_HOME, "\x1b[H") == 0);
    r2_assert("ESC_HIDE_CURSOR is wrong", strcmp(ESC_HIDE_CURSOR, "\x1b[?25l") == 0);
    r2_assert("ESC_SHOW_CURSOR is wrong", strcmp(ESC_SHOW_CURSOR, "\x1b[?25h") == 0);
    r2_assert("ESC_ERASE_SCREEN is wrong", strcmp(ESC_ERASE_SCREEN, "\x1b[2J") == 0);
    r2_assert("ESC_ERASE_LINE is wrong", strcmp(ESC_ERASE_LINE, "\x1b[2K") == 0);
    r2_assert("ESC_ERASE_END_OF_LINE is wrong", strcmp(ESC_ERASE_END_OF_LINE, "\x1b[K") == 0);
    r2_assert("ESC_ERASE_START_OF_LINE is wrong", strcmp(ESC_ERASE_START_OF_LINE, "\x1b[1K") == 0);
    r2_assert("ESC_ERASE_DOWN is wrong", strcmp(ESC_ERASE_DOWN, "\x1b[J") == 0);
    r2_assert("ESC_ERASE_UP is wrong", strcmp(ESC_ERASE_UP, "\x1b[1J") == 0);
    r2_assert("ESC_SAVE_CURSOR is wrong", strcmp(ESC_SAVE_CURSOR, "\x1b[s") == 0);
    r2_assert("ESC_UNSAVE_CURSOR is wrong", strcmp(ESC_UNSAVE_CURSOR, "\x1b[u") == 0);
    r2_assert("ESC_QUERY_CURSOR_POSITION is wrong", strcmp(ESC_QUERY_CURSOR_POSITION, "\x1b[6n") == 0);
    r2_assert("ESC_QUERY_DEVICE_STATUS is wrong", strcmp(ESC_QUERY_DEVICE_STATUS, "\x1b[5n") == 0);
    return 0;
}

static const char *r2_termui_test(void)
{
    r2_run_test(test_escape_sequences_exact);
    r2_run_test(test_simple_test);
    r2_run_test(test_256_grayscale_gradient);
    r2_run_test(test_256_color_gradient);
    r2_run_test(test_rgb_red_to_blue);
    r2_run_test(test_rgb_rainbow);
    return 0;
}
