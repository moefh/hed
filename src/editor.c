/* editor.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>

#include "editor.h"
#include "screen.h"
#include "term.h"
#include "input.h"

#define HED_BANNER  "hed v0.1"
#define HEADER_LINES  2
#define FOOTER_LINES  2
#define BORDER_LINES  (HEADER_LINES+FOOTER_LINES)

#define show_msg  hed_show_msg
#define clear_msg hed_clear_msg

void hed_init_editor(struct hed_editor *editor)
{
  editor->filename = NULL;
  editor->data = NULL;
  editor->data_len = 0;
  editor->cur_msg[0] = '\0';
  editor->msg_was_set = false;
  editor->modified = false;
  editor->reading_string = false;
}

int hed_show_msg(struct hed_editor *editor, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(editor->cur_msg, sizeof(editor->cur_msg), fmt, ap);
  va_end(ap);
  
  editor->screen.redraw_needed = true;
  editor->msg_was_set = true;
  return -1;
}

int hed_clear_msg(struct hed_editor *editor)
{
  editor->cur_msg[0] = '\0';
  editor->screen.redraw_needed = true;
  editor->msg_was_set = true;
  return -1;
}

static void destroy_editor(struct hed_editor *editor)
{
  if (editor->filename)
    free(editor->filename);
  if (editor->data)
    free(editor->data);
}

static int write_file(struct hed_editor *editor, const char *filename)
{
  if (! editor->data)
    return 0;

  char *new_filename = NULL;
  if (! editor->filename || strcmp(filename, editor->filename) != 0) {
    new_filename = malloc(strlen(filename) + 1);
    if (! new_filename)
      return show_msg(editor, "ERROR: out of memory"); 
    strcpy(new_filename, filename);
  }
  
  FILE *f = fopen(filename, "w");
  if (! f) {
    free(new_filename);
    return show_msg(editor, "ERROR: can't open file '%s'", filename);
  }
  if (fwrite(editor->data, 1, editor->data_len, f) != editor->data_len) {
    free(new_filename);
    fclose(f);
    return show_msg(editor, "ERROR: can't write file '%s'", filename);
  }
  fclose(f);
  show_msg(editor, "File saved: '%s'", filename);
  editor->modified = false;

  if (new_filename)
    editor->filename = new_filename;
  return 0;
}

int hed_set_data(struct hed_editor *editor, uint8_t *data, size_t data_len)
{
  if (editor->filename)
    free(editor->filename);
  if (editor->data)
    free(editor->data);
  
  editor->data = data;
  editor->data_len = data_len;
  editor->filename = NULL;
  editor->modified = false;
  return 0;
}

int hed_read_file(struct hed_editor *editor, const char *filename)
{
  FILE *f = fopen(filename, "r");
  if (! f)
    return show_msg(editor, "ERROR: can't open file '%s'", filename);

  char *new_filename = malloc(strlen(filename) + 1);
  if (! new_filename)
    goto err;
  strcpy(new_filename, filename);
  
  // seekable file
  if (fseek(f, 0, SEEK_END) != -1) {
    long size = ftell(f);
    if (size < 0 || size > INT_MAX) {
      show_msg(editor, "ERROR: file is too large");
      goto err;
    }
    if (fseek(f, 0, SEEK_SET) == -1) {
      show_msg(editor, "ERROR: can't seek to start of file");
      goto err;
    }
    uint8_t *data = malloc(size);
    if (! data) {
      show_msg(editor, "ERROR: not enough memory for %ld bytes", size);
      goto err;
    }
    
    if (fread(data, 1, size, f) != (size_t) size) {
      show_msg(editor, "ERROR: error reading file");
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
    editor->modified = false;
    fclose(f);
    return 0;
  }

  show_msg(editor, "ERROR: reading from pipes not implemented");

 err:
  if (new_filename)
    free(new_filename);
  fclose(f);
  return -1;
}

#define DUMP_CHAR(c)  (((c)>=32 && (c)<127) ? (c) : '?')

static void show_header(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;

  reset_color();
  set_color(FG_BLACK, BG_GRAY);
  move_cursor(1, 1);
  out(" %s", (editor->filename) ? editor->filename : "NO FILE");
  if (editor->modified)
    out(" (modified)");
  clear_eol();
  move_cursor(scr->w - strlen(HED_BANNER) - 1, 1);
  out("%s", HED_BANNER);
  reset_color();
}

static void show_key_help(int x, int y, const char *key, const char *help)
{
  move_cursor(x, y);
  set_color(FG_BLACK, BG_GRAY);
  out("%s", key);
  reset_color();
  out(" %s", help);

  size_t txt_len = strlen(key) + strlen(help) + 1;
  for (size_t i = txt_len; i < 17; i++)
    out(" ");
}

static void show_footer(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;

  reset_color();
  move_cursor(1, scr->h - 1);
  if (editor->cur_msg[0] != '\0') {
    set_color(FG_BLACK, BG_GRAY);
    out(" %s", editor->cur_msg);
    clear_eol();
  }
  clear_eol();

  if (editor->reading_string) {
    show_key_help(1 + 0*17, scr->h, "^C", "Cancel");
    show_key_help(1 + 1*17, scr->h, "RET", "Accept");
  } else {
    show_key_help(1 + 0*17, scr->h, "^X", "Exit Editor");
    show_key_help(1 + 1*17, scr->h, "TAB", "Edit Mode");
    show_key_help(1 + 2*17, scr->h, "^O", "Write File");
    show_key_help(1 + 3*17, scr->h, "^R", "Read File");
  }
  clear_eol();
}

static void redraw_screen(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;

  if (scr->window_changed) {
    reset_color();
    clear_screen();
    scr->window_changed = false;
  }

  show_header(editor);
  show_footer(editor);

  //static int count = 0; move_cursor(scr->w-5, 1); out("%5d", count++ % 1000);
  
  if (editor->data) {
    move_cursor(1, 3);

    for (int i = 0; i < scr->h - BORDER_LINES; i++) {
      size_t pos = 16 * (scr->top_line + i);
      if (pos >= editor->data_len)
        break;
      set_bold(false);
      out("%08x ", (unsigned) pos);
      box_draw("| ");
      int len = (editor->data_len - pos < 16) ? editor->data_len - pos : 16;
      char buf[17];
      set_bold(scr->pane == PANE_HEX);
      for (int j = 0; j < len; j++) {
        uint8_t b = editor->data[pos+j];

        if (scr->cursor_pos == pos + j) {
          int cur_x = scr->cursor_pos % 16;
          int cur_y = scr->cursor_pos / 16 - scr->top_line;
          move_cursor(11 + 3*cur_x, 3 + cur_y);
          set_color(FG_BLACK, (scr->pane == PANE_HEX && scr->editing_byte) ? BG_YELLOW : BG_GRAY);
          set_bold(false);
          out(" ");
        }
        out("%02x ", b);
        if (scr->cursor_pos == pos + j) {
          reset_color();
          set_bold(scr->pane == PANE_HEX);
        }
        
        buf[j] = (b < 32 || b >= 0x7f) ? '.' : b;
      }
      for (int j = len; j < 16; j++) {
        out("   ");
        buf[j] = ' ';
      }
      buf[16] = '\0';
      set_bold(false);
      box_draw("| ");

      set_bold(scr->pane == PANE_TEXT);
      if (scr->cursor_pos >= pos && scr->cursor_pos <= pos + 16) {
        int n_before = scr->cursor_pos - pos;
        out("%.*s", n_before, buf);
        set_color(FG_BLACK, BG_GRAY);
        set_bold(false);
        out("%c", buf[n_before]);
        reset_color(); 
        set_bold(scr->pane == PANE_TEXT);
        if (n_before < 16)
          out("%.*s", 16-n_before-1, buf + n_before + 1);
        out("\r\n");
      } else
        out("%s\r\n", buf);
    }
    //move_cursor(1, 10); out("%d,%d", cur_x, cur_y);
  }
  
  hed_scr_flush();
  scr->redraw_needed = false;
}

static void cursor_right(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  if (scr->cursor_pos + 1 < editor->data_len) {
    scr->cursor_pos++;
    size_t n_page_lines = scr->h - BORDER_LINES;
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
    size_t n_page_lines = scr->h - BORDER_LINES;
    while (scr->cursor_pos >= 16 * (scr->top_line + n_page_lines))
      scr->top_line++;
    scr->redraw_needed = true;
  }
}

static void cursor_page_up(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  int cursor_delta = scr->cursor_pos - 16*scr->top_line;
  size_t n_page_lines = scr->h - BORDER_LINES;
  if (scr->top_line == 0)
    cursor_delta %= 16;
  else if (scr->top_line >= n_page_lines)
    scr->top_line -= n_page_lines;
  else
    scr->top_line = 0;
  scr->cursor_pos = 16*scr->top_line + cursor_delta;
  scr->redraw_needed = true;
}

static void cursor_page_down(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  int cursor_delta = scr->cursor_pos - 16*scr->top_line;
  size_t n_page_lines = scr->h - BORDER_LINES;
  size_t bottom_line = editor->data_len / 16 + (editor->data_len % 16 != 0);
  if (bottom_line < n_page_lines || scr->top_line == bottom_line - n_page_lines) {
    if (bottom_line < n_page_lines)
      scr->top_line = 0;
    cursor_delta = editor->data_len - 16*scr->top_line - 1;
  } else if (bottom_line > n_page_lines && scr->top_line + 2*n_page_lines < bottom_line)
    scr->top_line += n_page_lines;
  else
    scr->top_line = bottom_line - n_page_lines;
  scr->cursor_pos = 16*scr->top_line + cursor_delta;
  scr->redraw_needed = true;
}

static void cursor_home(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  scr->cursor_pos = scr->cursor_pos / 16 * 16;
  scr->redraw_needed = true;
}

static void cursor_end(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  scr->cursor_pos = scr->cursor_pos / 16 * 16 + 15;
  if (scr->cursor_pos >= editor->data_len)
    scr->cursor_pos = editor->data_len - 1;
  scr->redraw_needed = true;
}

static void cursor_start_of_file(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  scr->cursor_pos = 0;
  scr->top_line = 0;
  scr->redraw_needed = true;
}

static void cursor_end_of_file(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  size_t n_page_lines = scr->h - BORDER_LINES;
  size_t bottom_line = editor->data_len / 16 + (editor->data_len % 16 != 0);
  scr->cursor_pos = editor->data_len - 1;
  if (bottom_line < n_page_lines)
    scr->top_line = 0;
  else
    scr->top_line = bottom_line - n_page_lines;
  scr->redraw_needed = true;
}

static int read_string_prompt(struct hed_editor *editor, const char *prompt, char *str, size_t max_str_len)
{
  struct hed_screen *scr = &editor->screen;

  UNUSED(max_str_len);
  show_msg(editor, "%s: ", prompt);
  size_t prompt_len = strlen(prompt);
  size_t str_len = strlen(str);
  size_t cursor_pos = str_len;
  char key_err[64];

  editor->reading_string = true;
  scr->redraw_needed = true;
  while (! editor->quit) {
    show_cursor(false);
    if (scr->redraw_needed)
      redraw_screen(editor);
    reset_color();
    set_color(FG_BLACK, BG_GRAY);
    move_cursor(4 + prompt_len, scr->h - 1);
    out("%s", str);
    clear_eol();
    move_cursor(4 + prompt_len + cursor_pos, scr->h - 1);
    set_color(FG_BLACK, BG_GRAY);
    show_cursor(true);
    hed_scr_flush();
    
    int k = read_key(scr->term_fd, key_err, sizeof(key_err));
    switch (k) {
    case KEY_REDRAW:
      show_cursor(false);
      reset_color();
      clear_screen();
      show_cursor(true);
      scr->redraw_needed = true;
      break;
      
    case CTRL_KEY('c'):
    case '\r':
      editor->reading_string = false;
      show_cursor(false);
      clear_msg(editor);
      return (k == '\r') ? 0 : -1;
      
    case KEY_HOME:        cursor_pos = 0; break;
    case KEY_END:         cursor_pos = str_len; break;
    case KEY_ARROW_LEFT:  if (cursor_pos > 0) cursor_pos--; break;
    case KEY_ARROW_RIGHT: if (cursor_pos < str_len) cursor_pos++; break;

    case 8:
    case 127:
      if (cursor_pos > 0) {
        memmove(str + cursor_pos - 1, str + cursor_pos, str_len - cursor_pos + 1);
        cursor_pos--;
        str_len--;
      }
      break;

    case KEY_DEL:
      if (cursor_pos < str_len) {
        memmove(str + cursor_pos, str + cursor_pos + 1, str_len - cursor_pos + 1);
        str_len--;
      }
      break;
      
    default:
      if (k >= 32 && k < 127) {
        memmove(str + cursor_pos + 1, str + cursor_pos, str_len - cursor_pos + 1);
        str[cursor_pos++] = k;
        str_len++;
      }
    }
  }
  
  editor->reading_string = false;
  clear_msg(editor);
  return 0;
}

static void process_input(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  
  char key_err[64];
  int k = read_key(scr->term_fd, key_err, sizeof(key_err));

  editor->msg_was_set = false;
  switch (k) {
  case KEY_REDRAW:
    reset_color();
    clear_screen();
    scr->redraw_needed = true;
    break;

  case KEY_BAD_SEQUENCE:
    show_msg(editor, "Unknown key: <ESC>%s", key_err);
    break;
    
    //case '\x1b': show_msg(editor, "Key: <ESC>"); break;
    
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
    if (! editor->data) {
      show_msg(editor, "No data to write!");
    } else {
      char filename[256];
      if (editor->filename)
        snprintf(filename, sizeof(filename), "%s", editor->filename);
      else
        filename[0] = '\0';
      if (read_string_prompt(editor, "Write file", filename, sizeof(filename)) >= 0)
        write_file(editor, filename);
      scr->redraw_needed = true;
    }
    break;

  case CTRL_KEY('r'):
    {
      char filename[256];
      filename[0] = '\0';
      if (read_string_prompt(editor, "Read file", filename, sizeof(filename)) >= 0)
        hed_read_file(editor, filename);
      scr->redraw_needed = true;
    }
    break;

  case CTRL_KEY('a'):    cursor_home(editor); break;
  case CTRL_KEY('e'):    cursor_end(editor); break;
  case 8:                cursor_left(editor); break;
  case 127:              cursor_left(editor); break;
  case KEY_HOME:         cursor_home(editor); break;
  case KEY_END:          cursor_end(editor);  break;
  case KEY_CTRL_HOME:    cursor_start_of_file(editor); break;
  case KEY_CTRL_END:     cursor_end_of_file(editor); break;
  case KEY_PAGE_UP:      cursor_page_up(editor); break;
  case KEY_PAGE_DOWN:    cursor_page_down(editor); break;
  case KEY_ARROW_UP:     cursor_up(editor); break;
  case KEY_ARROW_DOWN:   cursor_down(editor); break;
  case KEY_ARROW_LEFT:   cursor_left(editor); break;
  case KEY_ARROW_RIGHT:  cursor_right(editor); break;

#if 0
  case KEY_CTRL_DEL:         show_msg(editor, "key: ctrl+del");  break;
  case KEY_CTRL_INS:         show_msg(editor, "key: ctrl+ins");  break;
  case KEY_CTRL_PAGE_UP:     show_msg(editor, "key: ctrl+pgup"); break;
  case KEY_CTRL_PAGE_DOWN:   show_msg(editor, "key: ctrl+pgdn"); break;
  case KEY_CTRL_ARROW_UP:    show_msg(editor, "key: ctrl+up"); break;
  case KEY_CTRL_ARROW_DOWN:  show_msg(editor, "key: ctrl+down"); break;
  case KEY_CTRL_ARROW_LEFT:  show_msg(editor, "key: ctrl+left"); break;
  case KEY_CTRL_ARROW_RIGHT: show_msg(editor, "key: ctrl+right"); break;

  case KEY_SHIFT_HOME:        show_msg(editor, "key: shift+home"); break;
  case KEY_SHIFT_END:         show_msg(editor, "key: shift+end");  break;
  case KEY_SHIFT_DEL:         show_msg(editor, "key: shift+del");  break;
  case KEY_SHIFT_INS:         show_msg(editor, "key: shift+ins");  break;
  case KEY_SHIFT_PAGE_UP:     show_msg(editor, "key: shift+pgup"); break;
  case KEY_SHIFT_PAGE_DOWN:   show_msg(editor, "key: shift+pgdn"); break;
  case KEY_SHIFT_ARROW_UP:    show_msg(editor, "key: shift+up"); break;
  case KEY_SHIFT_ARROW_DOWN:  show_msg(editor, "key: shift+down"); break;
  case KEY_SHIFT_ARROW_LEFT:  show_msg(editor, "key: shift+left"); break;
  case KEY_SHIFT_ARROW_RIGHT: show_msg(editor, "key: shift+right"); break;
    
  case KEY_DEL:  show_msg(editor, "key: del");  break;
  case KEY_INS:  show_msg(editor, "key: ins");  break;

  case KEY_F1:  show_msg(editor, "key: F1");  break;
  case KEY_F2:  show_msg(editor, "key: F2");  break;
  case KEY_F3:  show_msg(editor, "key: F3");  break;
  case KEY_F4:  show_msg(editor, "key: F4");  break;
  case KEY_F5:  show_msg(editor, "key: F5");  break;
  case KEY_F6:  show_msg(editor, "key: F6");  break;
  case KEY_F7:  show_msg(editor, "key: F7");  break;
  case KEY_F8:  show_msg(editor, "key: F8");  break;
  case KEY_F9:  show_msg(editor, "key: F9");  break;
  case KEY_F10: show_msg(editor, "key: F10"); break;
  case KEY_F11: show_msg(editor, "key: F11"); break;
  case KEY_F12: show_msg(editor, "key: F12"); break;

  case KEY_SHIFT_F1:  show_msg(editor, "key: shift+F1");  break;
  case KEY_SHIFT_F2:  show_msg(editor, "key: shift+F2");  break;
  case KEY_SHIFT_F3:  show_msg(editor, "key: shift+F3");  break;
  case KEY_SHIFT_F4:  show_msg(editor, "key: shift+F4");  break;
  case KEY_SHIFT_F5:  show_msg(editor, "key: shift+F5");  break;
  case KEY_SHIFT_F6:  show_msg(editor, "key: shift+F6");  break;
  case KEY_SHIFT_F7:  show_msg(editor, "key: shift+F7");  break;
  case KEY_SHIFT_F8:  show_msg(editor, "key: shift+F8");  break;
  case KEY_SHIFT_F9:  show_msg(editor, "key: shift+F9");  break;
  case KEY_SHIFT_F10: show_msg(editor, "key: shift+F10"); break;
  case KEY_SHIFT_F11: show_msg(editor, "key: shift+F11"); break;
  case KEY_SHIFT_F12: show_msg(editor, "key: shift+F12"); break;
#endif
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

  if (! editor->msg_was_set && editor->cur_msg[0] != '\0') {
    editor->cur_msg[0] = '\0';
    scr->redraw_needed = true;
  }
  
#if 0
  show_msg(editor, "key: %d", k);
#endif
}

int hed_run_editor(struct hed_editor *editor)
{
  if (hed_init_screen(&editor->screen) < 0) {
    fprintf(stderr, "ERROR setting up terminal\n");
    return -1;
  }

  show_cursor(false);
  clear_screen();
    
  editor->quit = false;
  while (! editor->quit) {
    if (editor->screen.redraw_needed)
      redraw_screen(editor);
    process_input(editor);
  }

  reset_color();
  clear_screen();
  show_cursor(true);
  hed_scr_flush();
  destroy_editor(editor);
  return 0;
}
