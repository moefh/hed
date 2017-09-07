/* term.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "term.h"

static int term_fd;
static int restored_old_term;
static struct termios old_term;

void term_restore(void)
{
  if (! restored_old_term) {
    tcsetattr(term_fd, TCSAFLUSH, &old_term);
    restored_old_term = 1;
  }
}

int term_setup_raw(int use_term_fd)
{
  struct termios term;

  term_fd = use_term_fd;
  if (! isatty(term_fd))
    return -1;
  if (tcgetattr(term_fd, &old_term) < 0)
    return -1;
  restored_old_term = 0;
  
  term = old_term;
  term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  term.c_oflag &= ~OPOST;
  term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  term.c_cflag &= ~(CSIZE | PARENB);
  term.c_cflag |= CS8;
  term.c_cc[VMIN] = 0;
  term.c_cc[VTIME] = 1;
  if (tcsetattr(term_fd, TCSAFLUSH, &term) < 0 || atexit(term_restore) != 0)
    return -1;
  return 0;
}

int term_get_window_size(int *width, int *height)
{
  struct winsize term_size;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_size) < 0)
    return -1;
  *width = term_size.ws_col;
  *height = term_size.ws_row;
  return 0;
}
