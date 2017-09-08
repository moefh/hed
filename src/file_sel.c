/* file_sel.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#include "file_sel.h"
#include "editor.h"
#include "screen.h"
#include "input.h"

struct file_sel {
  struct hed_editor *editor;
};

static void draw_header(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;

  reset_color();
  set_color(FG_BLACK, BG_GRAY);
  move_cursor(1, 1);
  clear_eol();
  move_cursor(scr->w - strlen(HED_BANNER) - 1, 1);
  out("%s", HED_BANNER);
  reset_color();
}

static void draw_footer(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;

  reset_color();
  move_cursor(1, scr->h - 1);
  if (scr->cur_msg[0] != '\0') {
    set_color(FG_BLACK, BG_GRAY);
    out(" %s", scr->cur_msg);
  }
  clear_eol();

  hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^C", "Cancel");
  //hed_draw_key_help(1 + 1*KEY_HELP_SPACING, scr->h, "^T", "To Files");
  clear_eol();
}

static void draw_main_screen(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;

  if (scr->window_changed) {
    reset_color();
    clear_screen();
    scr->window_changed = false;
  }

  draw_header(fs);
  draw_footer(fs);

  hed_scr_flush();
  scr->redraw_needed = false;
}

int hed_select_file(struct hed_editor *editor, char *filename, size_t max_filename_len)
{
  UNUSED(filename);
  UNUSED(max_filename_len);
  
  struct hed_screen *scr = &editor->screen;
  struct file_sel file_sel;
  file_sel.editor = editor;
  
  char key_err[64];

  clear_msg();
  reset_color();
  clear_screen();
  scr->redraw_needed = true;
  while (! editor->quit) {
    if (scr->redraw_needed)
      draw_main_screen(&file_sel);
    hed_scr_flush();
    
    int k = read_key(scr->term_fd, key_err, sizeof(key_err));
    switch (k) {
    case KEY_REDRAW:
      reset_color();
      clear_screen();
      scr->redraw_needed = true;
      break;
      
    case CTRL_KEY('c'):
      scr->redraw_needed = true;
      return -1;
    }
  }
  
  scr->redraw_needed = true;
  return 0;
}
