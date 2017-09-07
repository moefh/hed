/* editor.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include "editor.h"
#include "screen.h"
#include "term.h"

#define HED_BANNER  "hed v0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum hex_editor_key {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  INS_KEY,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,

  SHIFT_ARROW_LEFT,
  SHIFT_ARROW_RIGHT,
  SHIFT_ARROW_UP,
  SHIFT_ARROW_DOWN,
  SHIFT_INS_KEY,
  SHIFT_DEL_KEY,
  SHIFT_HOME_KEY,
  SHIFT_END_KEY,
  SHIFT_PAGE_UP,
  SHIFT_PAGE_DOWN,

  CTRL_ARROW_LEFT,
  CTRL_ARROW_RIGHT,
  CTRL_ARROW_UP,
  CTRL_ARROW_DOWN,
  CTRL_INS_KEY,
  CTRL_DEL_KEY,
  CTRL_HOME_KEY,
  CTRL_END_KEY,
  CTRL_PAGE_UP,
  CTRL_PAGE_DOWN,

  F1_KEY,
  F2_KEY,
  F3_KEY,
  F4_KEY,
  F5_KEY,
  F6_KEY,
  F7_KEY,
  F8_KEY,
  F9_KEY,
  F10_KEY,
  F11_KEY,
  F12_KEY,

  SHIFT_F1_KEY,
  SHIFT_F2_KEY,
  SHIFT_F3_KEY,
  SHIFT_F4_KEY,
  SHIFT_F5_KEY,
  SHIFT_F6_KEY,
  SHIFT_F7_KEY,
  SHIFT_F8_KEY,
  SHIFT_F9_KEY,
  SHIFT_F10_KEY,
  SHIFT_F11_KEY,
  SHIFT_F12_KEY,
};

static int err_msg(struct hed_editor *editor, const char *fmt, ...) HED_PRINTF_FORMAT(2, 3);

static void init_editor(struct hed_editor *editor)
{
  editor->filename = NULL;
  editor->data = NULL;
  editor->data_len = 0;
  editor->err_msg[0] = '\0';
  editor->modified = false;
}

static int save_file(struct hed_editor *editor)
{
  if (! editor->data)
    return 0;
  
  FILE *f = fopen(editor->filename, "w");
  if (! f)
    return err_msg(editor, "can't open '%s'", editor->filename);
  if (fwrite(editor->data, 1, editor->data_len, f) != editor->data_len) {
    fclose(f);
    return err_msg(editor, "error writing file");
  }
  fclose(f);
  editor->modified = false;
  return 0;
}

static int open_file(struct hed_editor *editor, const char *filename)
{
  FILE *f = fopen(filename, "r");
  if (! f)
    return err_msg(editor, "can't open '%s'", filename);

  char *new_filename = malloc(strlen(filename) + 1);
  if (! new_filename)
    goto err;
  strcpy(new_filename, filename);
  
  // seekable file
  if (fseek(f, 0, SEEK_END) != -1) {
    long size = ftell(f);
    if (size < 0 || size > INT_MAX) {
      err_msg(editor, "file is too large");
      goto err;
    }
    if (fseek(f, 0, SEEK_SET) == -1) {
      err_msg(editor, "can't seek to start of file");
      goto err;
    }
    uint8_t *data = malloc(size);
    if (! data) {
      err_msg(editor, "not enough memory for %ld bytes", size);
      goto err;
    }
    
    if (fread(data, 1, size, f) != (size_t) size) {
      err_msg(editor, "error reading file");
      free(data);
      goto err;
    }

    if (editor->filename)
      free(editor->filename);
    if (editor->data)
      free(editor->data);
    editor->data = data;
    editor->data_len = size;
    editor->filename = new_filename;
    fclose(f);
    return 0;
  }

  err_msg(editor, "reading from pipes not implemented");

 err:
  if (new_filename)
    free(new_filename);
  fclose(f);
  return -1;
}

static int err_msg(struct hed_editor *editor, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(editor->err_msg, sizeof(editor->err_msg), fmt, ap);
  va_end(ap);
  editor->screen.redraw_needed = true;
  return -1;
}

static void die(const char *msg)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(msg);
  exit(1);
}

static void destroy_editor(struct hed_editor *editor)
{
  if (editor->filename)
    free(editor->filename);
  if (editor->data)
    free(editor->data);
}

#define IS_LETTER(x)  ((x) >= 'A' && (x) <= 'Z')
#define IS_DIGIT(x)   ((x) >= '0' && (x) <= '9')

static int read_key_seq(struct hed_editor *editor, const char *seq, size_t len)
{
  if (len == 2 && seq[0] == '[' && IS_LETTER(seq[1])) {
    switch (seq[1]) {
    case 'A': return ARROW_UP;
    case 'B': return ARROW_DOWN;
    case 'C': return ARROW_RIGHT;
    case 'D': return ARROW_LEFT;
    case 'H': return HOME_KEY;
    case 'F': return END_KEY;
    }
  }

  if (len == 3 && seq[0] == '[' && IS_DIGIT(seq[1]) && seq[2] == '~') {
    switch (seq[1]) {
    case '1': return HOME_KEY;
    case '2': return INS_KEY;
    case '3': return DEL_KEY;
    case '4': return END_KEY;
    case '5': return PAGE_UP;
    case '6': return PAGE_DOWN;
    case '7': return HOME_KEY;
    case '8': return END_KEY;
    }
  }

  if (len == 5 && seq[0] == '[' && IS_DIGIT(seq[1]) && seq[2] == ';' && IS_DIGIT(seq[3])) {
    if (seq[1] == '1' && seq[3] == '5' && seq[4] == 'H') return CTRL_HOME_KEY;
    if (seq[1] == '1' && seq[3] == '5' && seq[4] == 'F') return CTRL_END_KEY;
  }
  
  if (len == 2 && seq[0] == 'O') {
    switch (seq[1]) {
    case 'F': return END_KEY;
    case 'H': return HOME_KEY;
    case 'P': return F1_KEY;
    case 'Q': return F2_KEY;
    case 'R': return F3_KEY;
    case 'S': return F4_KEY;
    }
  }

  if (len == 3 && seq[0] == '[' && seq[1] == '[' && IS_LETTER(seq[2])) {
    switch (seq[2]) {
    case 'A': return F1_KEY;
    case 'B': return F2_KEY;
    case 'C': return F3_KEY;
    case 'D': return F4_KEY;
    case 'E': return F5_KEY;
    }
  }

  if (len == 4 && seq[0] == '[' && IS_DIGIT(seq[1]) && IS_DIGIT(seq[2]) && seq[3] == '~') {
    if (seq[1] == '1' && seq[2] == '5') return F5_KEY;
    if (seq[1] == '1' && seq[2] == '7') return F6_KEY;
    if (seq[1] == '1' && seq[2] == '8') return F7_KEY;
    if (seq[1] == '1' && seq[2] == '9') return F8_KEY;
    if (seq[1] == '2' && seq[2] == '0') return F9_KEY;
    if (seq[1] == '2' && seq[2] == '1') return F10_KEY;
    if (seq[1] == '2' && seq[2] == '3') return F11_KEY;
    if (seq[1] == '2' && seq[2] == '4') return F12_KEY;
    
    if (seq[1] == '2' && seq[2] == '5') return SHIFT_F1_KEY;
    if (seq[1] == '2' && seq[2] == '6') return SHIFT_F2_KEY;
    if (seq[1] == '2' && seq[2] == '8') return SHIFT_F3_KEY;
    if (seq[1] == '2' && seq[2] == '9') return SHIFT_F4_KEY;
    if (seq[1] == '3' && seq[2] == '1') return SHIFT_F5_KEY;
    if (seq[1] == '3' && seq[2] == '2') return SHIFT_F6_KEY;
    if (seq[1] == '3' && seq[2] == '3') return SHIFT_F7_KEY;
    if (seq[1] == '3' && seq[2] == '4') return SHIFT_F8_KEY;
  }
  
  err_msg(editor, "-> unrecognized key: %.*s", (int)len, seq);
  return 0;
}

static int read_key(struct hed_editor *editor)
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN && errno != EINTR)
      die("read");
    if (nread == -1 && errno == EINTR)
      return 0;
  }

#define NEXT()        do { if (read(STDIN_FILENO, &seq[len++], 1) != 1) goto err; } while (0)
#define CUR           seq[len-1]
  
  if (c == '\x1b') {
    char seq[64];
    size_t len = 0;
    if (read(STDIN_FILENO, &seq[len++], 1) != 1)
      return c;
    
    while (len < sizeof(seq)-2) {
      NEXT();
      if (CUR == '~' || IS_LETTER(CUR))
        return read_key_seq(editor, seq, len);
      if (CUR == ';') {
        NEXT();
        continue;
      }
      //if (! IS_DIGIT(CUR)) goto err;
    }
    goto err;
  } else {
    //if (c >= 32) return -1;
    return c;
  }
  
 err:
  return -1;
}

#define DUMP_CHAR(c)  (((c)>=32 && (c)<127) ? (c) : '?')

static void redraw_screen(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  show_cursor(false);

  if (scr->window_changed) {
    clear_screen();
    scr->window_changed = false;
  }
  
  set_color(47);
  set_color(30);
  move_cursor(1, 1);
  out(" %s", (editor->filename) ? editor->filename : "NO FILE");
  if (editor->modified)
    out(" (modified)");
  clear_eol();
  move_cursor(scr->w - strlen(HED_BANNER) - 1, 1);
  out("%s", HED_BANNER);
  reset_color();

  //static int count = 0; move_cursor(scr->w-5, 1); out("%5d", count++ % 1000);
  
  if (editor->data) {
    move_cursor(1, 3);

    for (int i = 0; i < scr->h - 4; i++) {
      size_t pos = 16 * (scr->top_line + i);
      if (pos >= editor->data_len)
        break;
      //set_bold(false);
      out("%08x ", (unsigned) pos);
      line_draw("| ");
      int len = (editor->data_len - pos < 16) ? editor->data_len - pos : 16;
      char buf[17];
      //set_bold(true);
      for (int j = 0; j < len; j++) {
        uint8_t b = editor->data[pos+j];

        if (scr->cursor_pos == pos + j) {
          int cur_x = scr->cursor_pos % 16;
          int cur_y = scr->cursor_pos / 16 - scr->top_line;
          move_cursor(11 + 3*cur_x, 3 + cur_y);
          set_color(30);
          set_color((scr->pane == PANE_TEXT) ? 107 : (scr->editing_byte) ? 101 : 103);
          //set_bold(false);
          out(" ");
        }
        out("%02x ", b);
        if (scr->cursor_pos == pos + j) {
          reset_color();
          //set_bold(true);
        }
        
        buf[j] = (b < 32 || b >= 0x7f) ? '.' : b;
      }
      for (int j = len; j < 16; j++) {
        out("   ");
        buf[j] = ' ';
      }
      buf[16] = '\0';
      //set_bold(false);
      line_draw("| ");
      //set_bold(true);

      if (scr->cursor_pos >= pos && scr->cursor_pos <= pos + 16) {
        int n_before = scr->cursor_pos - pos;
        out("%.*s", n_before, buf);
        set_color(30);
        set_color((scr->pane == PANE_TEXT) ? 103 : 107);
        //set_bold(false);
        out("%c", buf[n_before]);
        reset_color();
        //set_bold(true);
        if (n_before < 16)
          out("%.*s", 16-n_before-1, buf + n_before + 1);
        out("\r\n");
      } else
        out("%s\r\n", buf);
    }
    reset_color();

    move_cursor(1, scr->w - 1);
    out("%s", editor->err_msg);
    clear_eol();

    //move_cursor(1, 10); out("%d,%d", cur_x, cur_y);
  } else
    move_cursor(1, 1);
  
  hed_scr_flush();
  scr->redraw_needed = false;
}

static void cursor_right(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  if (scr->cursor_pos + 1 < editor->data_len) {
    scr->cursor_pos++;
    size_t n_page_lines = scr->h - 4;
    while (scr->cursor_pos >= 16 * (scr->top_line + n_page_lines))
      scr->top_line++;
    scr->redraw_needed = true;
  }
}

static void cursor_left(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  if (scr->cursor_pos >= 1) {
    scr->cursor_pos--;
    while (scr->cursor_pos < 16 * scr->top_line)
      scr->top_line--;
    scr->redraw_needed = true;
  }
}

static void cursor_up(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  if (scr->cursor_pos >= 16) {
    scr->cursor_pos -= 16;
    while (scr->cursor_pos < 16 * scr->top_line)
      scr->top_line--;
    scr->redraw_needed = true;
  }
}

static void cursor_down(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  if (scr->cursor_pos + 16 < editor->data_len) {
    scr->cursor_pos += 16;
    size_t n_page_lines = scr->h - 4;
    while (scr->cursor_pos >= 16 * (scr->top_line + n_page_lines))
      scr->top_line++;
    scr->redraw_needed = true;
  }
}

static void process_input(struct hed_editor *editor)
{
  int k = read_key(editor);

  struct hed_screen *scr = &editor->screen;
  switch (k) {
  case CTRL_KEY('q'):
  case CTRL_KEY('x'):
    editor->quit = true;
    break;

  case '\t':
    if (scr->pane == PANE_HEX)
      scr->pane = PANE_TEXT;
    else
      scr->pane = PANE_HEX;
    scr->redraw_needed = true;
    break;
    
  case CTRL_KEY('l'):
    clear_screen();
    scr->redraw_needed = true;
    break;

  case CTRL_KEY('o'):
    save_file(editor);
    scr->redraw_needed = true;
    break;
    
  case HOME_KEY:
    scr->cursor_pos = scr->cursor_pos / 16 * 16;
    scr->redraw_needed = true;
    break;

  case END_KEY:
    scr->cursor_pos = scr->cursor_pos / 16 * 16 + 15;
    if (scr->cursor_pos >= editor->data_len)
      scr->cursor_pos = editor->data_len - 1;
    scr->redraw_needed = true;
    break;

  case CTRL_HOME_KEY:
    scr->cursor_pos = 0;
    scr->top_line = 0;
    scr->redraw_needed = true;
    break;

  case CTRL_END_KEY:
    {
      size_t n_page_lines = scr->h - 4;
      size_t bottom_line = editor->data_len / 16 + (editor->data_len % 16 != 0);
      scr->cursor_pos = editor->data_len - 1;
      scr->top_line = bottom_line - n_page_lines;
      scr->redraw_needed = true;
    }
    break;

  case PAGE_UP:
    {
      int cursor_delta = scr->cursor_pos - 16*scr->top_line;
      size_t n_page_lines = scr->h - 4;
      if (scr->top_line == 0)
        cursor_delta %= 16;
      else if (scr->top_line >= n_page_lines)
        scr->top_line -= n_page_lines;
      else
        scr->top_line = 0;
      scr->cursor_pos = 16*scr->top_line + cursor_delta;
      scr->redraw_needed = true;
    }
    break;

  case PAGE_DOWN:
    {
      int cursor_delta = scr->cursor_pos - 16*scr->top_line;
      size_t n_page_lines = scr->h - 4;
      size_t bottom_line = editor->data_len / 16 + (editor->data_len % 16 != 0);
      if (scr->top_line == bottom_line - n_page_lines) {
        cursor_delta = editor->data_len - 16*scr->top_line - 1;
      } else if (bottom_line > n_page_lines && scr->top_line + 2*n_page_lines < bottom_line)
        scr->top_line += n_page_lines;
      else
        scr->top_line = bottom_line - n_page_lines;
      scr->cursor_pos = 16*scr->top_line + cursor_delta;
      scr->redraw_needed = true;
    }
    break;
    
  case ARROW_UP:
    cursor_up(editor);
    break;
    
  case ARROW_DOWN:
    cursor_down(editor);
    break;

  case ARROW_LEFT:
    cursor_left(editor);
    break;
    
  case ARROW_RIGHT:
    cursor_right(editor);
    break;

#if 0
  case CTRL_HOME_KEY:    err_msg(editor, "ctrl+home"); break;
  case CTRL_END_KEY:     err_msg(editor, "ctrl+end");  break;
  case CTRL_DEL_KEY:     err_msg(editor, "ctrl+del");  break;
  case CTRL_INS_KEY:     err_msg(editor, "ctrl+ins");  break;
  case CTRL_PAGE_UP:     err_msg(editor, "ctrl+pgup"); break;
  case CTRL_PAGE_DOWN:   err_msg(editor, "ctrl+pgdn"); break;
  case CTRL_ARROW_UP:    err_msg(editor, "ctrl+up"); break;
  case CTRL_ARROW_DOWN:  err_msg(editor, "ctrl+down"); break;
  case CTRL_ARROW_LEFT:  err_msg(editor, "ctrl+left"); break;
  case CTRL_ARROW_RIGHT: err_msg(editor, "ctrl+right"); break;

  case SHIFT_HOME_KEY:    err_msg(editor, "shift+home"); break;
  case SHIFT_END_KEY:     err_msg(editor, "shift+end");  break;
  case SHIFT_DEL_KEY:     err_msg(editor, "shift+del");  break;
  case SHIFT_INS_KEY:     err_msg(editor, "shift+ins");  break;
  case SHIFT_PAGE_UP:     err_msg(editor, "shift+pgup"); break;
  case SHIFT_PAGE_DOWN:   err_msg(editor, "shift+pgdn"); break;
  case SHIFT_ARROW_UP:    err_msg(editor, "shift+up"); break;
  case SHIFT_ARROW_DOWN:  err_msg(editor, "shift+down"); break;
  case SHIFT_ARROW_LEFT:  err_msg(editor, "shift+left"); break;
  case SHIFT_ARROW_RIGHT: err_msg(editor, "shift+right"); break;
    
  case DEL_KEY:  err_msg(editor, "del");  break;
  case INS_KEY:  err_msg(editor, "ins");  break;

  case F1_KEY:  err_msg(editor, "F1");  break;
  case F2_KEY:  err_msg(editor, "F2");  break;
  case F3_KEY:  err_msg(editor, "F3");  break;
  case F4_KEY:  err_msg(editor, "F4");  break;
  case F5_KEY:  err_msg(editor, "F5");  break;
  case F6_KEY:  err_msg(editor, "F6");  break;
  case F7_KEY:  err_msg(editor, "F7");  break;
  case F8_KEY:  err_msg(editor, "F8");  break;
  case F9_KEY:  err_msg(editor, "F9");  break;
  case F10_KEY: err_msg(editor, "F10"); break;
  case F11_KEY: err_msg(editor, "F11"); break;
  case F12_KEY: err_msg(editor, "F12"); break;

  case SHIFT_F1_KEY:  err_msg(editor, "shift+F1");  break;
  case SHIFT_F2_KEY:  err_msg(editor, "shift+F2");  break;
  case SHIFT_F3_KEY:  err_msg(editor, "shift+F3");  break;
  case SHIFT_F4_KEY:  err_msg(editor, "shift+F4");  break;
  case SHIFT_F5_KEY:  err_msg(editor, "shift+F5");  break;
  case SHIFT_F6_KEY:  err_msg(editor, "shift+F6");  break;
  case SHIFT_F7_KEY:  err_msg(editor, "shift+F7");  break;
  case SHIFT_F8_KEY:  err_msg(editor, "shift+F8");  break;
  case SHIFT_F9_KEY:  err_msg(editor, "shift+F9");  break;
  case SHIFT_F10_KEY: err_msg(editor, "shift+F10"); break;
  case SHIFT_F11_KEY: err_msg(editor, "shift+F11"); break;
  case SHIFT_F12_KEY: err_msg(editor, "shift+F12"); break;
#endif

  case 0x7f:
    cursor_left(editor);
    break;
  }

  bool reset_editing_byte = true;
  if (editor->data && scr->cursor_pos < editor->data_len) {
    if (scr->pane == PANE_TEXT) {
      if (k >= 32 && k < 0x7f) {
        editor->data[scr->cursor_pos] = k;
        editor->modified = true;
        cursor_right(editor);
        scr->redraw_needed = true;
      }
    } else {
      int c = ((k >= '0' && k <= '9') ? k - '0' :
               (k >= 'a' && k <= 'f') ? k - 'a' + 10 :
               (k >= 'A' && k <= 'F') ? k - 'A' + 10 : -1);
      if (c >= 0) {
        reset_editing_byte = false;
        if (! scr->editing_byte) {
          scr->editing_byte = true;
          editor->data[scr->cursor_pos] &= 0x0f;
          editor->data[scr->cursor_pos] |= c << 4;
        } else {
          scr->editing_byte = false;
          editor->data[scr->cursor_pos] &= 0xf0;
          editor->data[scr->cursor_pos] |= c;
          cursor_right(editor);
        }
        editor->modified = true;
        scr->redraw_needed = true;
      }
    }
  }

  if (reset_editing_byte && scr->editing_byte) {
    scr->editing_byte = false;
    scr->redraw_needed = true;
  }

#if 0
  err_msg(editor, "key: %d", k);
#endif
}

int hed_run_editor(struct hed_editor *editor, const char *filename)
{
  init_editor(editor);
  if (term_setup_raw() < 0) {
    fprintf(stderr, "ERROR setting up terminal\n");
    return -1;
  }
  hed_init_screen(&editor->screen);
  show_cursor(false);
  clear_screen();
    
  if (filename)
    open_file(editor, filename);

  editor->quit = false;
  while (! editor->quit) {
    if (editor->screen.redraw_needed)
      redraw_screen(editor);
    process_input(editor);
  }

  clear_screen();
  show_cursor(true);
  hed_scr_flush();
  destroy_editor(editor);
  return 0;
}
