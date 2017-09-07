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

static int restored_old_term;
static struct termios old_term;

void term_restore(void)
{
  if (! restored_old_term) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    restored_old_term = 1;
  }
}

int term_setup_raw(void)
{
  struct termios term;

  if (tcgetattr(STDIN_FILENO, &old_term) < 0)
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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) < 0 || atexit(term_restore) != 0)
    return -1;
  return 0;
}

int term_get_window_size(int *width, int *height)
{
  struct winsize term_size;

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &term_size) < 0)
    return -1;
  *width = term_size.ws_col;
  *height = term_size.ws_row;
  return 0;
}
