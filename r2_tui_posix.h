/* r2_tui_posix - v0.1 - public domain terminal plumbing for r2_tui on Mac / Linux
    no warranty implied; use at your own risk

    The platform side of r2_tui for POSIX terminals: raw mode, the
    alternate screen, non-blocking reads, window size and a millisecond
    clock. Chestnut (or any other target) provides its own equivalent;
    r2_tui.h itself never touches the OS.

    Header only (all static). It needs POSIX + SIGWINCH, so define these
    before your first #include:

        #define _DEFAULT_SOURCE
        #define _DARWIN_C_SOURCE
        #include "r2_tui_posix.h"

        tui_posix p;
        if (tui_posix_open(&p) != 0) ...   // raw mode + alternate screen
        int cols, rows;
        tui_posix_size(&cols, &rows);
        ...
        n = tui_posix_read(buf, sizeof(buf));        // never blocks
        tui_posix_write(out, n);
        if (tui_posix_resized()) ...                 // SIGWINCH since last call
        tui_posix_wait(timeout_ms);                  // sleep until input or timeout
        ...
        tui_posix_close(&p);                         // always restore the terminal

    Raw mode turns off signals too, so Ctrl+C / Ctrl+Z / Ctrl+\ arrive as
    keys: the application decides what quits.

LICENSE
    See r2_tui.h for license information (this file is released under
    the same terms).
*/
#ifndef R2_TUI_POSIX_H
#define R2_TUI_POSIX_H

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "r2_termui.h"

typedef struct tui_posix
{
    struct termios saved;
    int open;
} tui_posix;

static volatile sig_atomic_t r2__tui_posix_winch = 0;

static void r2__tui_posix_on_winch(int sig)
{
    (void)sig;
    r2__tui_posix_winch = 1;
}

/** Write everything (retrying on partial writes). 0 on success. */
static int tui_posix_write(const void *buf, size_t n)
{
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0)
    {
        ssize_t w = write(STDOUT_FILENO, p, n);
        if (w < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

static int tui_posix_write_str(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return tui_posix_write(s, n);
}

/** Raw mode, alternate screen, SIGWINCH handler. 0 on success. */
static int tui_posix_open(tui_posix *p)
{
    struct termios raw;
    struct sigaction sa;
    p->open = 0;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &p->saved) != 0)
        return -1;
    raw = p->saved;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        return -1;

    sa.sa_handler = r2__tui_posix_on_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, 0);

    p->open = 1;
    tui_posix_write_str(ESC_ALT_SCREEN_ON ESC_AUTOWRAP_OFF ESC_HIDE_CURSOR ESC_ERASE_SCREEN);
    return 0;
}

/** Undo tui_posix_open: attributes, cursor, main screen, cooked mode. */
static void tui_posix_close(tui_posix *p)
{
    if (!p->open)
        return;
    tui_posix_write_str(ESC_RESET_ATTRIBUTES ESC_SYNC_END ESC_AUTOWRAP_ON ESC_SHOW_CURSOR ESC_ALT_SCREEN_OFF);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &p->saved);
    p->open = 0;
}

/** Current terminal size. 0 on success. */
static int tui_posix_size(int *cols, int *rows)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0 || ws.ws_row == 0)
        return -1;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/** 1 if the window was resized since the last call. */
static int tui_posix_resized(void)
{
    if (!r2__tui_posix_winch)
        return 0;
    r2__tui_posix_winch = 0;
    return 1;
}

/** Whatever input is waiting (never blocks); 0 if none. */
static size_t tui_posix_read(void *buf, size_t cap)
{
    ssize_t n = read(STDIN_FILENO, buf, cap);
    return n > 0 ? (size_t)n : 0u;
}

/** Wait up to ms for input (returns early on input or a signal). */
static void tui_posix_wait(int ms)
{
    struct pollfd fd;
    fd.fd = STDIN_FILENO;
    fd.events = POLLIN;
    fd.revents = 0;
    poll(&fd, 1, ms);
}

/** Monotonic milliseconds (wraps; r2_tui only uses differences). */
static uint32_t tui_posix_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

#endif /* R2_TUI_POSIX_H */
