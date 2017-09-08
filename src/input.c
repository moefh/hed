/* input.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include "input.h"

#define IS_LETTER(x)  ((x) >= 'A' && (x) <= 'Z')
#define IS_DIGIT(x)   ((x) >= '0' && (x) <= '9')

static int read_key_seq(char *seq, size_t len)
{
  if (len == 1 && ((seq[0] >= 'A' && seq[0] <= 'Z') || (seq[0] >= 'a' && seq[0] <= 'z')))
    return ALT_KEY(seq[0]);
  
  if (len == 2 && seq[0] == '[' && IS_LETTER(seq[1])) {
    switch (seq[1]) {
    case 'A': return KEY_ARROW_UP;
    case 'B': return KEY_ARROW_DOWN;
    case 'C': return KEY_ARROW_RIGHT;
    case 'D': return KEY_ARROW_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    }
  }

  if (len == 3 && seq[0] == '[' && IS_DIGIT(seq[1]) && seq[2] == '~') {
    switch (seq[1]) {
    case '1': return KEY_HOME;
    case '2': return KEY_INS;
    case '3': return KEY_DEL;
    case '4': return KEY_END;
    case '5': return KEY_PAGE_UP;
    case '6': return KEY_PAGE_DOWN;
    case '7': return KEY_HOME;
    case '8': return KEY_END;
    }
  }

  if (len == 3 && seq[0] == '[' && IS_DIGIT(seq[1]) && seq[2] == '^') {
    switch (seq[1]) {
    case '1': return KEY_CTRL_HOME;
    case '2': return KEY_CTRL_INS;
    case '3': return KEY_CTRL_DEL;
    case '4': return KEY_CTRL_END;
    case '5': return KEY_CTRL_PAGE_UP;
    case '6': return KEY_CTRL_PAGE_DOWN;
    case '7': return KEY_CTRL_HOME;
    case '8': return KEY_CTRL_END;
    }
  }
  
  if (len == 5 && seq[0] == '[' && IS_DIGIT(seq[1]) && seq[2] == ';' && IS_DIGIT(seq[3])) {
    if (seq[1] == '1' && seq[3] == '5' && seq[4] == 'H') return KEY_CTRL_HOME;
    if (seq[1] == '1' && seq[3] == '5' && seq[4] == 'F') return KEY_CTRL_END;
  }
  
  if (len == 2 && seq[0] == 'O') {
    switch (seq[1]) {
    case 'F': return KEY_END;
    case 'H': return KEY_HOME;
    case 'P': return KEY_F1;
    case 'Q': return KEY_F2;
    case 'R': return KEY_F3;
    case 'S': return KEY_F4;
    }
  }

  if (len == 3 && seq[0] == '[' && seq[1] == '[' && IS_LETTER(seq[2])) {
    switch (seq[2]) {
    case 'A': return KEY_F1;
    case 'B': return KEY_F2;
    case 'C': return KEY_F3;
    case 'D': return KEY_F4;
    case 'E': return KEY_F5;
    }
  }

  if (len == 4 && seq[0] == '[' && IS_DIGIT(seq[1]) && IS_DIGIT(seq[2]) && seq[3] == '~') {
    if (seq[1] == '1' && seq[2] == '5') return KEY_F5;
    if (seq[1] == '1' && seq[2] == '7') return KEY_F6;
    if (seq[1] == '1' && seq[2] == '8') return KEY_F7;
    if (seq[1] == '1' && seq[2] == '9') return KEY_F8;
    if (seq[1] == '2' && seq[2] == '0') return KEY_F9;
    if (seq[1] == '2' && seq[2] == '1') return KEY_F10;
    if (seq[1] == '2' && seq[2] == '3') return KEY_F11;
    if (seq[1] == '2' && seq[2] == '4') return KEY_F12;
    
    if (seq[1] == '2' && seq[2] == '5') return KEY_SHIFT_F1;
    if (seq[1] == '2' && seq[2] == '6') return KEY_SHIFT_F2;
    if (seq[1] == '2' && seq[2] == '8') return KEY_SHIFT_F3;
    if (seq[1] == '2' && seq[2] == '9') return KEY_SHIFT_F4;
    if (seq[1] == '3' && seq[2] == '1') return KEY_SHIFT_F5;
    if (seq[1] == '3' && seq[2] == '2') return KEY_SHIFT_F6;
    if (seq[1] == '3' && seq[2] == '3') return KEY_SHIFT_F7;
    if (seq[1] == '3' && seq[2] == '4') return KEY_SHIFT_F8;
  }
  
  //err_msg(editor, "-> unrecognized key: %.*s", (int)len, seq);
  seq[len] = '\0';
  return KEY_BAD_SEQUENCE;
}

int read_key(int fd, char *seq, size_t max_seq_len)
{
  int nread;
  unsigned char c;
  while ((nread = read(fd, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN && errno != EINTR)
      return KEY_READ_ERROR;
    if (nread == -1 && errno == EINTR)
      return KEY_REDRAW;
  }

#define NEXT()        do { if (read(fd, &seq[len], 1) != 1) return read_key_seq(seq, len); len++; } while (0)
#define CUR           seq[len-1]
  
  size_t len = 0;
  if (c == '\x1b') {
    if (read(fd, &seq[len++], 1) != 1)
      return c;
    
    while (len < max_seq_len-3) {
      NEXT();
      if (CUR == '~' || CUR == '^' || IS_LETTER(CUR))
        return read_key_seq(seq, len);
      if (CUR == ';') {
        NEXT();
        continue;
      }
      //if (! IS_DIGIT(CUR)) return read_key_seq(seq, len);
    }
    return read_key_seq(seq, len);
  }
  //if (c >= 32) return -1;
  return c;
}
