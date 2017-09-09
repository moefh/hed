/* editor.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#include "editor.h"
#include "screen.h"
#include "term.h"
#include "input.h"
#include "file.h"
#include "file_sel.h"

void hed_init_editor(struct hed_editor *editor)
{
  editor->file = NULL;
  editor->mode = HED_MODE_DEFAULT;
  editor->half_byte_edited = false;
  editor->search_str[0] = '\0';
  editor->read_only = false;
}

static void destroy_editor(struct hed_editor *editor)
{
  struct hed_file *file = editor->file;
  if (! file)
    return;
  
  do {
    struct hed_file *next = file->next;
    hed_free_file(file);
    file = next;
  } while (file != editor->file);
}

static void close_current_file(struct hed_editor *editor)
{
  struct hed_file *file = editor->file;
  if (! file)
    return;
  if (file->next == file) {
    hed_free_file(file);
    editor->file = NULL;
    return;
  }
  
  file->next->prev = file->prev;
  file->prev->next = file->next;
  editor->file = file->next;
  hed_free_file(file);
}

void hed_add_file(struct hed_editor *editor, struct hed_file *file)
{
  // close current file if it's the only one and it's empty
  if (editor->file && editor->file == editor->file->next && ! editor->file->data)
    close_current_file(editor);
  
  if (! editor->file) {
    file->next = file;
    file->prev = file;
    editor->file = file;
    return;
  }
  
  file->next = editor->file;
  file->prev = editor->file->prev;
  file->prev->next = file;
  file->next->prev = file;
}

static void draw_header(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  reset_color();
  set_color(FG_BLACK, BG_GRAY);
  move_cursor(1, 1);
  out(" %s", (file->filename) ? file->filename : "New Buffer");
  if (file->modified)
    out(" (modified)");
  if (editor->read_only)
    out(" (view mode)");
  clear_eol();
  move_cursor(scr->w - strlen(HED_BANNER) - 1, 1);
  out("%s", HED_BANNER);
  reset_color();
}

void hed_draw_key_help(int x, int y, const char *key, const char *help)
{
  move_cursor(x, y);
  set_color(FG_BLACK, BG_GRAY);
  out("%s", key);
  reset_color();
  out(" %s", help);

  size_t txt_len = strlen(key) + strlen(help) + 1;
  for (size_t i = txt_len; i < KEY_HELP_SPACING-1; i++)
    out(" ");
}

static void draw_footer(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;

  reset_color();
  move_cursor(1, scr->h - 1);
  if (scr->cur_msg[0] != '\0') {
    set_color(FG_BLACK, BG_GRAY);
    out(" %s", scr->cur_msg);
  }
  clear_eol();

  switch (editor->mode) {
  case HED_MODE_READ_FILENAME:
    hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^C", "Cancel");
    hed_draw_key_help(1 + 1*KEY_HELP_SPACING, scr->h, "^T", "To Files");
    break;

  case HED_MODE_READ_STRING:
    hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^C", "Cancel");
    break;

  case HED_MODE_READ_YESNO:
    hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^C", "Cancel");
    hed_draw_key_help(1 + 1*KEY_HELP_SPACING, scr->h, " Y", "Yes");
    hed_draw_key_help(1 + 2*KEY_HELP_SPACING, scr->h, " N", "No");
    break;

  case HED_MODE_DEFAULT:
    if (editor->file->next == editor->file)
      hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^X", "Exit");
    else
      hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^X", "Close");
    hed_draw_key_help(  1 + 1*KEY_HELP_SPACING, scr->h, "^O", "Write File");
    hed_draw_key_help(  1 + 2*KEY_HELP_SPACING, scr->h, "^R", "Read File");
    hed_draw_key_help(  1 + 3*KEY_HELP_SPACING, scr->h, "^W", "Where Is");
    if (! editor->read_only)
      hed_draw_key_help(1 + 4*KEY_HELP_SPACING, scr->h, "TAB", "Switch Mode");
  }
  clear_eol();
}

static void draw_main_screen(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  if (scr->window_changed || ! file->data) {
    reset_color();
    clear_screen();
    scr->window_changed = false;
  }

  draw_header(editor);
  draw_footer(editor);

  if (file->data) {
    move_cursor(1, 3);

    for (int i = 0; i < scr->h - BORDER_LINES; i++) {
      size_t pos = 16 * (file->top_line + i);
      if (pos >= file->data_len)
        break;
      set_bold(false);
      out("%08x ", (unsigned) pos);
      box_draw("| ");
      int len = (file->data_len - pos < 16) ? file->data_len - pos : 16;
      char buf[17];
      set_bold(file->pane == HED_PANE_HEX && ! editor->read_only);
      for (int j = 0; j < len; j++) {
        uint8_t b = file->data[pos+j];

        if (j == 8)
          out(" ");
        
        if (file->cursor_pos == pos + j) {
          int cur_x = file->cursor_pos % 16;
          int cur_y = file->cursor_pos / 16 - file->top_line;
          move_cursor(11 + 3*cur_x + (j >= 8), 3 + cur_y);
          set_color(FG_BLACK,
                    ((editor->half_byte_edited) ? BG_YELLOW
                     : (file->pane == HED_PANE_HEX && ! editor->read_only) ? BG_GREEN
                     : BG_GRAY));
          set_bold(false);
          out(" ");
        }
        out("%02x ", b);
        if (file->cursor_pos == pos + j) {
          reset_color();
          set_bold(file->pane == HED_PANE_HEX && ! editor->read_only);
        }
        
        buf[j] = (b < 32 || b >= 0x7f) ? '.' : b;
      }
      for (int j = len; j < 16; j++) {
        out("   ");
        if (j == 8)
          out(" ");
        buf[j] = ' ';
      }
      buf[16] = '\0';
      set_bold(false);
      box_draw("| ");

      set_bold(file->pane == HED_PANE_TEXT && ! editor->read_only);
      if (file->cursor_pos >= pos && file->cursor_pos <= pos + 16) {
        int n_before = file->cursor_pos - pos;
        out("%.*s", n_before, buf);
        set_color(FG_BLACK, (file->pane == HED_PANE_TEXT && ! editor->read_only) ? BG_GREEN : BG_GRAY);
        set_bold(false);
        out("%c", buf[n_before]);
        reset_color(); 
        set_bold(file->pane == HED_PANE_TEXT && ! editor->read_only);
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

void hed_set_cursor_pos(struct hed_editor *editor, size_t pos, size_t visible_len_after)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;
  size_t n_page_lines = scr->h - BORDER_LINES;
  size_t last_line = file->data_len / 16 + (file->data_len % 16 != 0);

  if (pos >= file->data_len)
    pos = (file->data_len == 0) ? 0 : file->data_len-1;
  if (pos + visible_len_after >= file->data_len)
    visible_len_after = file->data_len - pos;
  
  file->cursor_pos = pos;

  // If the cursor or 'visible_len_after' bytes after it are not visible,
  // center the screen vertically around the cursor
  if (! (file->cursor_pos / 16 >= file->top_line
         && (file->cursor_pos+visible_len_after) / 16 >= file->top_line
         && file->cursor_pos / 16 <= file->top_line + n_page_lines
         && (file->cursor_pos+visible_len_after) / 16 <= file->top_line + n_page_lines)) {
    if (file->cursor_pos / 16 < n_page_lines/2)
      file->top_line = 0;
    else {
      file->top_line = file->cursor_pos / 16 - n_page_lines/2;
      if (file->top_line + n_page_lines > last_line)
        file->top_line = last_line - n_page_lines;
    }
  }
  scr->redraw_needed = true;
}

static void cursor_right(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  if (file->cursor_pos + 1 < file->data_len) {
    file->cursor_pos++;
    size_t n_page_lines = scr->h - BORDER_LINES;
    while (file->cursor_pos >= 16 * (file->top_line + n_page_lines))
      file->top_line++;
    scr->redraw_needed = true;
  }
}

static void cursor_left(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  if (file->cursor_pos >= 1) {
    file->cursor_pos--;
    while (file->cursor_pos < 16 * file->top_line)
      file->top_line--;
    scr->redraw_needed = true;
  }
}

static void cursor_up(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  if (file->cursor_pos >= 16) {
    file->cursor_pos -= 16;
    while (file->cursor_pos < 16 * file->top_line)
      file->top_line--;
    scr->redraw_needed = true;
  }
}

static void cursor_down(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  if (file->cursor_pos + 16 < file->data_len) {
    file->cursor_pos += 16;
    size_t n_page_lines = scr->h - BORDER_LINES;
    while (file->cursor_pos >= 16 * (file->top_line + n_page_lines))
      file->top_line++;
    scr->redraw_needed = true;
  }
}

static void cursor_page_up(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;
  int cursor_delta = file->cursor_pos - 16*file->top_line;
  size_t n_page_lines = scr->h - BORDER_LINES;

  if (file->top_line == 0)
    cursor_delta %= 16;
  else if (file->top_line >= n_page_lines)
    file->top_line -= n_page_lines;
  else
    file->top_line = 0;
  file->cursor_pos = 16*file->top_line + cursor_delta;
  scr->redraw_needed = true;
}

static void cursor_page_down(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;
  int cursor_delta = file->cursor_pos - 16*file->top_line;
  size_t n_page_lines = scr->h - BORDER_LINES;
  size_t last_line = file->data_len / 16 + (file->data_len % 16 != 0);

  if (last_line < n_page_lines || file->top_line == last_line - n_page_lines) {
    if (last_line < n_page_lines)
      file->top_line = 0;
    cursor_delta = file->data_len - 16*file->top_line - 1;
  } else if (last_line > n_page_lines && file->top_line + 2*n_page_lines < last_line)
    file->top_line += n_page_lines;
  else
    file->top_line = last_line - n_page_lines;
  file->cursor_pos = 16*file->top_line + cursor_delta;
  scr->redraw_needed = true;
}

static void cursor_home(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  file->cursor_pos = file->cursor_pos / 16 * 16;
  scr->redraw_needed = true;
}

static void cursor_end(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  file->cursor_pos = file->cursor_pos / 16 * 16 + 15;
  if (file->cursor_pos >= file->data_len)
    file->cursor_pos = file->data_len - 1;
  scr->redraw_needed = true;
}

static void cursor_start_of_file(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;

  file->cursor_pos = 0;
  file->top_line = 0;
  scr->redraw_needed = true;
}

static void cursor_end_of_file(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;
  size_t n_page_lines = scr->h - BORDER_LINES;
  size_t last_line = file->data_len / 16 + (file->data_len % 16 != 0);
  
  file->cursor_pos = file->data_len - 1;
  if (last_line < n_page_lines)
    file->top_line = 0;
  else
    file->top_line = last_line - n_page_lines;
  scr->redraw_needed = true;
}

static int prompt_get_yesno(struct hed_editor *editor, const char *prompt, bool *response)
{
  struct hed_screen *scr = &editor->screen;

  show_msg("%s", prompt);
  size_t prompt_len = strlen(prompt);
  char key_err[64];

  editor->mode = HED_MODE_READ_YESNO;
  scr->redraw_needed = true;
  while (! editor->quit) {
    if (scr->redraw_needed)
      draw_main_screen(editor);
    move_cursor(3 + prompt_len, scr->h - 1);
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
    case 'y': case 'Y':
    case 'n': case 'N':
      if (k == 'y' || k == 'Y')
        *response = true;
      else if (k == 'n' || k == 'N')
        *response = false;
      editor->mode = HED_MODE_DEFAULT;
      show_cursor(false);
      clear_msg();
      return (k == CTRL_KEY('c')) ? -1 : 0;
    }
  }
  return -1;
}

static int prompt_get_text(struct hed_editor *editor, const char *prompt, char *str, size_t max_str_len)
{
  struct hed_screen *scr = &editor->screen;

  size_t prompt_len = strlen(prompt);
  size_t str_len = strlen(str);
  size_t cursor_pos = str_len;
  char key_err[64];

  clear_msg();
  scr->redraw_needed = true;
  while (! editor->quit) {
    show_cursor(false);
    if (scr->redraw_needed)
      draw_main_screen(editor);
    reset_color();
    set_color(FG_BLACK, BG_GRAY);
    move_cursor(1, scr->h - 1);
    out(" %s: %s", prompt, str);
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
      editor->mode = HED_MODE_DEFAULT;
      show_cursor(false);
      clear_msg();
      return (k == '\r') ? 0 : -1;

    case CTRL_KEY('a'):
    case KEY_HOME:
      cursor_pos = 0;
      break;
      
    case CTRL_KEY('e'):
    case KEY_END:
      cursor_pos = str_len;
      break;
      
    case CTRL_KEY('b'):
    case KEY_ARROW_LEFT:
      if (cursor_pos > 0)
        cursor_pos--;
      break;
      
    case CTRL_KEY('f'):
    case KEY_ARROW_RIGHT:
      if (cursor_pos < str_len)
        cursor_pos++;
      break;

    case CTRL_KEY('t'):
      if (editor->mode == HED_MODE_READ_FILENAME) {
        show_cursor(false);
        char filename[256];
        if (hed_select_file(editor, filename, sizeof(filename)) >= 0) {
          strncpy(str, filename, max_str_len-1);
          str[max_str_len-1] = '\0';
          editor->mode = HED_MODE_DEFAULT;
          return 0;
        }
        show_cursor(true);
        scr->redraw_needed = true;
      }
      break;
      
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
      if (k >= 32 && k < 127 && (str_len + 2 <= max_str_len)) {
        memmove(str + cursor_pos + 1, str + cursor_pos, str_len - cursor_pos + 1);
        str[cursor_pos++] = k;
        str_len++;
      }
    }
  }
  
  editor->mode = HED_MODE_DEFAULT;
  clear_msg();
  return 0;
}

static int prompt_get_string(struct hed_editor *editor, const char *prompt, char *str, size_t max_str_len)
{
  editor->mode = HED_MODE_READ_STRING;
  return prompt_get_text(editor, prompt, str, max_str_len);
}

static int prompt_get_filename(struct hed_editor *editor, const char *prompt, char *str, size_t max_str_len)
{
  editor->mode = HED_MODE_READ_FILENAME;
  return prompt_get_text(editor, prompt, str, max_str_len);
}
  
static int prompt_save_file(struct hed_editor *editor)
{
  char filename[256];
  if (editor->file->filename)
    snprintf(filename, sizeof(filename), "%s", editor->file->filename);
  else
    filename[0] = '\0';
  if (prompt_get_filename(editor, "Write file", filename, sizeof(filename)) < 0) {
    editor->screen.redraw_needed = true;
    return -1;
  }
  editor->screen.redraw_needed = true;
  return hed_write_file(editor->file, filename);
}

static int prompt_read_file(struct hed_editor *editor)
{
  char filename[256];
  filename[0] = '\0';
  if (prompt_get_filename(editor, "Read file", filename, sizeof(filename)) < 0) {
    editor->screen.redraw_needed = true;
    return -1;
  }
  editor->screen.redraw_needed = true;
  
  struct hed_file *file = hed_read_file(filename);
  if (! file)
    return -1;
  hed_add_file(editor, file);
  editor->file = file;
  return 0;
}

static size_t conv_search_bytes(uint8_t *bytes, size_t max_len, const char *search_str)
{
  size_t len = 0;
  const char *src = search_str;
  while (*src != '\0' && len < 2*max_len) {
    uint8_t nibble = 0;
    while (*src != '\0') {
      if (*src >= '0' && *src <= '9') {
        nibble = *src - '0';
        src++;
        break;
      } else if (*src >= 'a' && *src <= 'f') {
        nibble = *src - 'a' + 10;
        src++;
        break;
      } else if (*src >= 'A' && *src <= 'F') {
        nibble = *src - 'A' + 10;
        src++;
        break;
      } else if (*src == ' ' || *src == ',') {
        src++;
      } else
        return 0;
    }

    if (len % 2 == 0)
      bytes[len/2] = nibble << 4;
    else
      bytes[len/2] |= nibble;
    len++;
  }
  if (len % 2 != 0)
    return 0;
  return len/2;
}

static int perform_search(struct hed_editor *editor)
{
  struct hed_file *file = editor->file;

  size_t search_len = strlen(editor->search_str);
  uint8_t *search_bytes;
  uint8_t search_buf[sizeof(editor->search_str)];
  if (file->pane == HED_PANE_TEXT) {
    search_bytes = (uint8_t *) editor->search_str;
  } else {
    search_len = conv_search_bytes(search_buf, sizeof(search_buf), editor->search_str);
    if (search_len == 0)
      return show_msg("Invalid byte sequence (must be a list pairs of hex numbers)");
    search_bytes = search_buf;
  }

  // TODO: Boyer-Moore search?
  bool found = false;
  size_t pos;
  for (pos = file->cursor_pos+1; pos + search_len < file->data_len; pos++) {
    if (memcmp(file->data + pos, search_bytes, search_len) == 0) {
      found = true;
      file->cursor_pos = pos;
      break;
    }
  }
  if (! found)
    return show_msg((file->pane == HED_PANE_HEX) ? "Byte sequence not found" : "Text not found");
  hed_set_cursor_pos(editor, pos, search_len);
  return 0;
}

static void process_input(struct hed_editor *editor)
{
  struct hed_screen *scr = &editor->screen;
  struct hed_file *file = editor->file;
  
  char key_err[64];
  int k = read_key(scr->term_fd, key_err, sizeof(key_err));

  scr->msg_was_set = false;
  switch (k) {
  case KEY_REDRAW:
    reset_color();
    clear_screen();
    scr->redraw_needed = true;
    break;

  case KEY_BAD_SEQUENCE:
    show_msg("Unknown key: <ESC>%s", key_err);
    break;
    
  case CTRL_KEY('x'):
    if (editor->file->modified) {
      bool save_changes = true;
      if (prompt_get_yesno(editor, "Save changes?  (Answering no will DISCARD changes.)", &save_changes) < 0)
        break;
      if (save_changes) {
        if (editor->file->filename) {
          if (hed_write_file(editor->file, editor->file->filename) < 0)
            break;
        } else {
          if (prompt_save_file(editor) < 0)
            break;
        }
      }
    }
    close_current_file(editor);
    file = editor->file;
    if (! editor->file) {
      editor->quit = true;
      return;
    }
    scr->redraw_needed = true;
    break;

  case ALT_KEY('.'):
    editor->file = editor->file->next;
    file = editor->file;
    scr->redraw_needed = true;
    break;
    
  case ALT_KEY(','):
    editor->file = editor->file->prev;
    file = editor->file;
    scr->redraw_needed = true;
    break;
    
  case '\t':
    if (file->pane == HED_PANE_HEX)
      file->pane = HED_PANE_TEXT;
    else
      file->pane = HED_PANE_HEX;
    scr->redraw_needed = true;
    break;
    
  case CTRL_KEY('l'):
    clear_screen();
    scr->redraw_needed = true;
    break;

  case CTRL_KEY('o'):
    if (! file->data)
      show_msg("No data to write!");
    else
      prompt_save_file(editor);
    break;

  case CTRL_KEY('r'):
    prompt_read_file(editor);
    file = editor->file;
    break;

  case CTRL_KEY('w'):
    if (file->data) {
      const char *prompt = (file->pane == HED_PANE_HEX) ? "Search bytes" : "Search text";
      char prompt_str[40];
      if (editor->search_str[0] != '\0') {
        size_t prompt_len = strlen(prompt);
        size_t len = strlen(editor->search_str);
        if (len + prompt_len + 10 > sizeof(prompt_str)) {
          len = sizeof(prompt_str) - prompt_len - 10;
          snprintf(prompt_str, sizeof(prompt_str), "%s [%.*s...]", prompt, (int) len, editor->search_str);
        } else
          snprintf(prompt_str, sizeof(prompt_str), "%s [%.*s]", prompt, (int) len, editor->search_str);
        prompt = prompt_str;
      }
      char search_str[sizeof(editor->search_str)];
      search_str[0] = '\0';
      if (prompt_get_filename(editor, prompt, search_str, sizeof(search_str)) < 0)
        break;
      if (search_str[0] != '\0')
        strcpy(editor->search_str, search_str);
      perform_search(editor);
    }
    break;

  case ALT_KEY('g'):
    if (file->data) {
      char location_str[256];
      location_str[0] = '\0';
      if (prompt_get_string(editor, "Go to offset", location_str, sizeof(location_str)) < 0)
        break;
      char *end = NULL;
      errno = 0;
      unsigned long offset = strtoul(location_str, &end, 16);
      if (errno != 0 || *end != '\0') {
        show_msg("Bad offset: %s", location_str);
        break;
      }
      hed_set_cursor_pos(editor, (size_t) offset, 16);
    }
    break;

  case ALT_KEY('\\'):    cursor_start_of_file(editor); break;
  case ALT_KEY('/'):     cursor_end_of_file(editor); break;
  case CTRL_KEY('a'):    cursor_home(editor); break;
  case CTRL_KEY('e'):    cursor_end(editor); break;
  case CTRL_KEY('b'):    cursor_left(editor); break;
  case CTRL_KEY('f'):    cursor_right(editor); break;
  case CTRL_KEY('p'):    cursor_up(editor); break;
  case CTRL_KEY('n'):    cursor_down(editor); break;
  case CTRL_KEY('y'):    cursor_page_up(editor); break;
  case CTRL_KEY('v'):    cursor_page_down(editor); break;
  case KEY_CTRL_HOME:    cursor_start_of_file(editor); break;
  case KEY_CTRL_END:     cursor_end_of_file(editor); break;
  case KEY_HOME:         cursor_home(editor); break;
  case KEY_END:          cursor_end(editor);  break;
  case KEY_PAGE_UP:      cursor_page_up(editor); break;
  case KEY_PAGE_DOWN:    cursor_page_down(editor); break;
  case KEY_ARROW_UP:     cursor_up(editor); break;
  case KEY_ARROW_DOWN:   cursor_down(editor); break;
  case KEY_ARROW_LEFT:   cursor_left(editor); break;
  case KEY_ARROW_RIGHT:  cursor_right(editor); break;

#if 0
  case KEY_CTRL_DEL:          show_msg("key: ctrl+del");  break;
  case KEY_CTRL_INS:          show_msg("key: ctrl+ins");  break;
  case KEY_CTRL_PAGE_UP:      show_msg("key: ctrl+pgup"); break;
  case KEY_CTRL_PAGE_DOWN:    show_msg("key: ctrl+pgdn"); break;
  case KEY_CTRL_ARROW_UP:     show_msg("key: ctrl+up"); break;
  case KEY_CTRL_ARROW_DOWN:   show_msg("key: ctrl+down"); break;
  case KEY_CTRL_ARROW_LEFT:   show_msg("key: ctrl+left"); break;
  case KEY_CTRL_ARROW_RIGHT:  show_msg("key: ctrl+right"); break;

  case KEY_SHIFT_HOME:        show_msg("key: shift+home"); break;
  case KEY_SHIFT_END:         show_msg("key: shift+end");  break;
  case KEY_SHIFT_DEL:         show_msg("key: shift+del");  break;
  case KEY_SHIFT_INS:         show_msg("key: shift+ins");  break;
  case KEY_SHIFT_PAGE_UP:     show_msg("key: shift+pgup"); break;
  case KEY_SHIFT_PAGE_DOWN:   show_msg("key: shift+pgdn"); break;
  case KEY_SHIFT_ARROW_UP:    show_msg("key: shift+up"); break;
  case KEY_SHIFT_ARROW_DOWN:  show_msg("key: shift+down"); break;
  case KEY_SHIFT_ARROW_LEFT:  show_msg("key: shift+left"); break;
  case KEY_SHIFT_ARROW_RIGHT: show_msg("key: shift+right"); break;
    
  case KEY_DEL: show_msg("key: del");  break;
  case KEY_INS: show_msg("key: ins");  break;

  case KEY_F1:  show_msg("key: F1");  break;
  case KEY_F2:  show_msg("key: F2");  break;
  case KEY_F3:  show_msg("key: F3");  break;
  case KEY_F4:  show_msg("key: F4");  break;
  case KEY_F5:  show_msg("key: F5");  break;
  case KEY_F6:  show_msg("key: F6");  break;
  case KEY_F7:  show_msg("key: F7");  break;
  case KEY_F8:  show_msg("key: F8");  break;
  case KEY_F9:  show_msg("key: F9");  break;
  case KEY_F10: show_msg("key: F10"); break;
  case KEY_F11: show_msg("key: F11"); break;
  case KEY_F12: show_msg("key: F12"); break;

  case KEY_SHIFT_F1:  show_msg("key: shift+F1");  break;
  case KEY_SHIFT_F2:  show_msg("key: shift+F2");  break;
  case KEY_SHIFT_F3:  show_msg("key: shift+F3");  break;
  case KEY_SHIFT_F4:  show_msg("key: shift+F4");  break;
  case KEY_SHIFT_F5:  show_msg("key: shift+F5");  break;
  case KEY_SHIFT_F6:  show_msg("key: shift+F6");  break;
  case KEY_SHIFT_F7:  show_msg("key: shift+F7");  break;
  case KEY_SHIFT_F8:  show_msg("key: shift+F8");  break;
  case KEY_SHIFT_F9:  show_msg("key: shift+F9");  break;
  case KEY_SHIFT_F10: show_msg("key: shift+F10"); break;
  case KEY_SHIFT_F11: show_msg("key: shift+F11"); break;
  case KEY_SHIFT_F12: show_msg("key: shift+F12"); break;
#endif
  }

  if (! editor->read_only) {
    bool reset_editing_byte = true;
    if (file->data && file->cursor_pos < file->data_len) {
      if (file->pane == HED_PANE_TEXT) {
        if (k >= 32 && k < 0x7f) {
          file->data[file->cursor_pos] = k;
          editor->file->modified = true;
          cursor_right(editor);
          scr->redraw_needed = true;
        }
      } else {
        int c = ((k >= '0' && k <= '9') ? k - '0' :
                 (k >= 'a' && k <= 'f') ? k - 'a' + 10 :
                 (k >= 'A' && k <= 'F') ? k - 'A' + 10 : -1);
        if (c >= 0) {
          reset_editing_byte = false;
          if (! editor->half_byte_edited) {
            editor->half_byte_edited = true;
            file->data[file->cursor_pos] &= 0x0f;
            file->data[file->cursor_pos] |= c << 4;
          } else {
            editor->half_byte_edited = false;
            file->data[file->cursor_pos] &= 0xf0;
            file->data[file->cursor_pos] |= c;
            cursor_right(editor);
          }
          editor->file->modified = true;
          scr->redraw_needed = true;
        }
      }
    }

    if (reset_editing_byte && editor->half_byte_edited) {
      editor->half_byte_edited = false;
      scr->redraw_needed = true;
    }
  }

#if 0
  if (! scr->msg_was_set)
    show_msg("key: %d", k);
#endif
  
  if (! scr->msg_was_set && scr->cur_msg[0] != '\0')
    clear_msg();
}

int hed_run_editor(struct hed_editor *editor, size_t start_cursor_pos)
{
  if (! editor->file) {
    struct hed_file *file = hed_new_file_from_data(NULL, 0);
    if (! file) {
      fprintf(stderr, "ERROR: out of memory\n");
      return -1;
    }
    hed_add_file(editor, file);
  }
  
  if (hed_init_screen(&editor->screen) < 0) {
    fprintf(stderr, "ERROR setting up terminal\n");
    return -1;
  }
  hed_set_cursor_pos(editor, start_cursor_pos, 16);
  
  show_cursor(false);
  clear_screen();
    
  editor->quit = false;
  while (! editor->quit) {
    if (editor->screen.redraw_needed)
      draw_main_screen(editor);
    process_input(editor);
  }

  reset_color();
  clear_screen();
  show_cursor(true);
  hed_scr_flush();
  destroy_editor(editor);
  return 0;
}
