/* screen.h */

#ifndef SCREEN_H_FILE
#define SCREEN_H_FILE

#include "hed.h"

enum vt100_color {
  FG_DEFAULT = 39,
  FG_BLACK   = 30,
  FG_RED     = 31,
  FG_GREEN   = 32,
  FG_YELLOW  = 33,
  FG_BLUE    = 34,
  FG_MAGENTA = 35,
  FG_CYAN    = 36,
  FG_GRAY    = 37,

  BG_DEFAULT = 49,
  BG_BLACK   = 40,
  BG_RED     = 41,
  BG_GREEN   = 42,
  BG_YELLOW  = 43,
  BG_BLUE    = 44,
  BG_MAGENTA = 45,
  BG_CYAN    = 46,
  BG_GRAY    = 47,
};

enum hed_edit_pane {
  PANE_HEX,
  PANE_TEXT,
};

struct hed_screen {
  int w;
  int h;
  size_t cursor_pos;
  size_t top_line;
  enum hed_edit_pane pane;
  bool window_changed;
  bool redraw_needed;
  bool editing_byte;
  bool utf8_box_draw;
  bool vt100_box_draw;

  char buf[1024];
  size_t buf_len;
};

void hed_init_screen(struct hed_screen *scr);
void hed_close_screen(void);

void hed_scr_flush(void);
void hed_scr_out(const char *fmt, ...) HED_PRINTF_FORMAT(1, 2);
void hed_scr_box_draw(const char *str);
void hed_scr_set_color(int c1, int c2);
void hed_scr_set_bold(bool bold);
void hed_scr_reverse_color(bool reverse);
void hed_scr_reset_color(void);
void hed_scr_clear_eol(void);
void hed_scr_move_cursor(int x, int y);
void hed_scr_show_cursor(bool show);
void hed_scr_clear_screen(void);

#define out             hed_scr_out
#define box_draw        hed_scr_box_draw
#define set_color       hed_scr_set_color
#define set_bold        hed_scr_set_bold
#define reverse_color   hed_scr_reverse_color
#define reset_color     hed_scr_reset_color
#define clear_eol       hed_scr_clear_eol
#define move_cursor     hed_scr_move_cursor
#define show_cursor     hed_scr_show_cursor
#define clear_screen    hed_scr_clear_screen

#endif /* SCREEN_H_FILE */
