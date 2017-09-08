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

int hed_init_screen(struct hed_screen *scr)
{
  signal(SIGWINCH, SIG_DFL);
  screen = scr;

  screen->term_fd = (isatty(STDIN_FILENO)) ? STDIN_FILENO : STDOUT_FILENO;
  if (term_setup_raw(screen->term_fd) < 0)
    return -1;
  
  term_get_window_size(&screen->w, &screen->h);
  screen->window_changed = true;
  screen->redraw_needed = true;
  screen->utf8_box_draw = true;
  screen->vt100_box_draw = true;
  screen->msg_was_set = false;
  screen->cur_msg[0] = '\0';
  screen->top_line = 0;
  screen->cursor_pos = 0;
  screen->out_buf_len = 0;
  
  signal(SIGWINCH, handle_sigwinch);
  return 0;
}

int hed_scr_show_msg(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(screen->cur_msg, sizeof(screen->cur_msg), fmt, ap);
  va_end(ap);
  
  screen->redraw_needed = true;
  screen->msg_was_set = true;
  return -1;
}

int hed_scr_clear_msg(void)
{
  screen->cur_msg[0] = '\0';
  screen->redraw_needed = true;
  screen->msg_was_set = true;
  return -1;
}

void hed_close_screen(void)
{
  signal(SIGWINCH, SIG_DFL);
  screen = NULL;
}

void hed_scr_flush(void)
{
  if (screen->out_buf_len > 0) {
    size_t pos = 0;
    while (pos < screen->out_buf_len) {
      ssize_t n = write(screen->term_fd, screen->out_buf + pos, screen->out_buf_len - pos);
      if (n < 0) {
        if (errno != EINTR)
          break;
      } else if (n == 0)
        break;
      pos += n;
    }
    screen->out_buf_len = 0;
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
  if (screen->out_buf_len + len > sizeof(screen->out_buf))
    hed_scr_flush();
  memcpy(screen->out_buf + screen->out_buf_len, buf, len);
  screen->out_buf_len += len;
}

/*
 * Convenience function for box drawing.
 * 
 * The corner characters (1, 3, 7, 9) were chosen because their
 * corners correspond to their positions in a keyboard number pad.
 */
void hed_scr_box_draw(const char *str)
{
  if (screen->utf8_box_draw) {
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
  } else if (screen->vt100_box_draw) {
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
  } else {
    for (const char *p = str; *p != '\0'; p++) {
      switch (*p) {
      case '7': hed_scr_out("+"); break;
      case '9': hed_scr_out("+"); break;
      case '1': hed_scr_out("+"); break;
      case '3': hed_scr_out("+"); break;
      default: hed_scr_out("%c", *p); break;
      }
    }
  }
}

void hed_scr_set_color(int c1, int c2)
{
  if (c1 >= 0)
    hed_scr_out("\x1b[%dm", c1);
  if (c2 >= 0)
    hed_scr_out("\x1b[%dm", c2);
}

void hed_scr_set_bold(bool bold)
{
  if (bold)
    hed_scr_out("\x1b[1m");
  else
    hed_scr_out("\x1b[22m");
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
