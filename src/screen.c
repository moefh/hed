/* screen.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

#include "screen.h"
#include "term.h"

static struct hed_screen *screen;

static void handle_sigwinch(int signum)
{
  if (screen) {
    term_get_window_size(&screen->w, &screen->h);
    screen->window_changed = true;
    screen->redraw_needed = true;
  }
  signal(signum, handle_sigwinch);
}

void hed_init_screen(struct hed_screen *scr)
{
  signal(SIGWINCH, SIG_DFL);
  screen = scr;

  term_get_window_size(&screen->w, &screen->h);
  screen->window_changed = true;
  screen->redraw_needed = true;
  screen->editing_byte = false;
  screen->top_line = 0;
  screen->cursor_pos = 0;
  screen->pane = PANE_HEX;
  screen->buf_len = 0;
  
  signal(SIGWINCH, handle_sigwinch);
}

void hed_close_screen(void)
{
  signal(SIGWINCH, SIG_DFL);
  screen = NULL;
}

void hed_scr_flush(void)
{
  if (screen->buf_len > 0) {
    write(STDOUT_FILENO, screen->buf, screen->buf_len);
    screen->buf_len = 0;
  }
}

void hed_scr_out(const char *fmt, ...)
{
  static char buf[1024];

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  size_t len = strlen(buf);
  if (screen->buf_len + len > sizeof(screen->buf))
    hed_scr_flush();
  memcpy(screen->buf + screen->buf_len, buf, len);
  screen->buf_len += len;
}

void hed_scr_line_draw(const char *str)
{
#if 0
  hed_scr_out("\x1b(0");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '|': hed_scr_out("\x78"); break;
    case '-': hed_scr_out("\x71"); break;
    case '7': hed_scr_out("\x6c"); break;
    case '9': hed_scr_out("\x6b"); break;
    case '1': hed_scr_out("\x6d"); break;
    case '3': hed_scr_out("\x6a"); break;
    default: hed_scr_out("%c", *p); break;
    }
  }
  hed_scr_out("\x1b(B");
#else
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '|': hed_scr_out("\u2502"); break;
    case '-': hed_scr_out("\u2500"); break;
    case '7': hed_scr_out("\u250c"); break;
    case '9': hed_scr_out("\u2510"); break;
    case '1': hed_scr_out("\u2514"); break;
    case '3': hed_scr_out("\u2518"); break;
    default: hed_scr_out("%c", *p); break;
    }
  }
#endif
}

void hed_scr_set_color(int color)
{
  hed_scr_out("\x1b[%dm", color);
}

void hed_scr_set_bold(bool bold)
{
  if (bold)
    hed_scr_out("\x1b[1m");
  else
    hed_scr_out("\x1b[21m");
}

void hed_scr_reverse_color(bool reverse)
{
  if (reverse)
    hed_scr_out("\x1b[7m");
  else
    hed_scr_out("\x1b[27m");
}

void hed_scr_reset_color(void)
{
  hed_scr_out("\x1b[0m");
}

void hed_scr_clear_eol(void)
{
  hed_scr_out("\x1b[K");
}

void hed_scr_move_cursor(int x, int y)
{
  if (x < 1) x = 1;
  if (y < 1) y = 1;
  if (x > screen->w) x = screen->w;
  if (y > screen->h) y = screen->h;
  hed_scr_out("\x1b[%d;%dH", y, x);
}

void hed_scr_show_cursor(bool show)
{
  if (show)
    hed_scr_out("\x1b[?25h");
  else
    hed_scr_out("\x1b[?25l");
}

void hed_scr_clear_screen(void)
{
  hed_scr_out("\x1b[2J");
  hed_scr_out("\x1b[H");
}
