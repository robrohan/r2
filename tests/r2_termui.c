#include "../r2_termui.h"
#include <stdio.h>
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

static const char *r2_termui_test(void)
{
    r2_run_test(test_simple_test);
    r2_run_test(test_256_grayscale_gradient);
    r2_run_test(test_256_color_gradient);
    r2_run_test(test_rgb_red_to_blue);
    r2_run_test(test_rgb_rainbow);
    return 0;
}
