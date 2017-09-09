/* help.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "help.h"
#include "editor.h"
#include "screen.h"
#include "input.h"

static const char *help_page[] = {
  "Control keys are written with '^', so ^C means Ctrl+C.",
  "Alt keys are written with 'M-', so M-G means Alt+G.",
  "Alternative keys are shown in parentheses.",
  "",
  "Editor keys:",
  "",
  "   ^X                    Close file (exit if there are no more files)",
  "   ^O                    Write file",
  "   ^R                    Read file into new buffer",
  "   M->   (M-.)           Go to next file",
  "   M-<   (M-,)           Go to previous file",
  "",
  "   ^G                    Show this help",
  "   ^L                    Redraw screen",
  "",
  "   ^B    (Left)          Move cursor left",
  "   ^F    (Right)         Move cursor right",
  "   ^P    (Up)            Move cursor up",
  "   ^N    (Down)          Move cursor down",
  "   ^Y    (PageUp)        Move one page up",
  "   ^V    (PageDown)      Move one page down",
  "",
  "   ^C                    Show current position",
  "   M-G                   Go to position",

  "   M-W                   Repeat last search",
  "   TAB                   Switch between hex and text panes",
  "",
  "Only on hex pane:",
  "",
  "   ^W                    Search byte sequence",
  "   0-9, a-f, A-F         Change file bytes",
  "",
  "Only on text pane:",
  "",
  "   ^W                    Search text",
  "   any ASCII char        Change file text",
  "",
};

struct help_state {
  struct hed_editor *editor;
  bool quit;
  size_t top_line;
  const char **lines;
  size_t num_lines;
};

static void init_help_state(struct help_state *hs, struct hed_editor *editor)
{
  hs->editor = editor;
  hs->quit = false;
  hs->top_line = 0;
  hs->lines = help_page;
  hs->num_lines = sizeof(help_page)/sizeof(help_page[0]);
}

static void draw_header(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;

  reset_color();
  set_color(FG_BLACK, BG_GRAY);
  move_cursor(1, 1);
  out(" Help");
  clear_eol();

  move_cursor(scr->w - strlen(HED_BANNER) - 1, 1);
  out("%s", HED_BANNER);
  reset_color();
}

static void draw_footer(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;

  reset_color();
  move_cursor(1, scr->h - 1);
  if (scr->cur_msg[0] != '\0') {
    set_color(FG_BLACK, BG_GRAY);
    out(" %s", scr->cur_msg);
  }
  clear_eol();

  hed_draw_key_help(1 + 0*KEY_HELP_SPACING, scr->h, "^C", "Back");
  hed_draw_key_help(1 + 1*KEY_HELP_SPACING, scr->h, "^P", "Up");
  hed_draw_key_help(1 + 2*KEY_HELP_SPACING, scr->h, "^N", "Down");
  hed_draw_key_help(1 + 3*KEY_HELP_SPACING, scr->h, "^Y", "Page Up");
  hed_draw_key_help(1 + 4*KEY_HELP_SPACING, scr->h, "^V", "Page Down");
  clear_eol();
}

static void draw_main_screen(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;

  if (scr->window_changed) {
    reset_color();
    clear_screen();
    scr->window_changed = false;
  }

  draw_header(hs);
  draw_footer(hs);

  int line = 0;
  while (line + BORDER_LINES < scr->h) {
    if (hs->top_line + line >= hs->num_lines)
      break;
    move_cursor(1, line + 1 + HEADER_LINES);
    out("%s", hs->lines[hs->top_line + line]);
    clear_eol();
    line++;
  }

  while (line + BORDER_LINES < scr->h) {
    reset_color();
    move_cursor(1, line + 1 + HEADER_LINES);
    clear_eol();
    line++;
  }
  
  hed_scr_flush();
  scr->redraw_needed = false;
}

static void move_text_up(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;

  if (hs->top_line > 0) {
    hs->top_line--;
    scr->redraw_needed = true;
  }
}

static void move_text_down(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;
  size_t n_page_lines = scr->h - BORDER_LINES;

  if (hs->top_line + n_page_lines < hs->num_lines) {
    hs->top_line++;
    scr->redraw_needed = true;
  }
}

static void move_text_page_up(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;
  size_t n_page_lines = scr->h - BORDER_LINES;

  if (hs->top_line > n_page_lines)
    hs->top_line -= n_page_lines;
  else
    hs->top_line = 0;
  scr->redraw_needed = true;
}

static void move_text_page_down(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;
  size_t n_page_lines = scr->h - BORDER_LINES;

  if (hs->top_line + 2*n_page_lines < hs->num_lines)
    hs->top_line += n_page_lines;
  else
    hs->top_line = hs->num_lines - n_page_lines;
  scr->redraw_needed = true;
}

static void process_input(struct help_state *hs)
{
  struct hed_screen *scr = &hs->editor->screen;
  char key_err[64];

  int k = read_key(scr->term_fd, key_err, sizeof(key_err));
  switch (k) {
  case KEY_REDRAW:
  case CTRL_KEY('l'):
    reset_color();
    clear_screen();
    scr->redraw_needed = true;
    break;

  case CTRL_KEY('x'):
  case CTRL_KEY('c'):
    scr->redraw_needed = true;
    hs->quit = true;
    break;

  case CTRL_KEY('p'):  move_text_up(hs); break;
  case CTRL_KEY('n'):  move_text_down(hs); break;
  case CTRL_KEY('y'):  move_text_page_up(hs); break;
  case CTRL_KEY('v'):  move_text_page_down(hs); break;
  case KEY_ARROW_UP:   move_text_up(hs); break;
  case KEY_ARROW_DOWN: move_text_down(hs); break;
  case KEY_PAGE_UP:    move_text_page_up(hs); break;
  case KEY_PAGE_DOWN:  move_text_page_down(hs); break;
  }
}

void hed_display_help(struct hed_editor *editor)
{
  clear_msg();

  struct hed_screen *scr = &editor->screen;
  struct help_state help;
  init_help_state(&help, editor);
  
  reset_color();
  clear_screen();
  scr->redraw_needed = true;
  while (! editor->quit && ! help.quit) {
    if (scr->redraw_needed)
      draw_main_screen(&help);
    process_input(&help);
  }

  reset_color();
  clear_screen();
  scr->redraw_needed = true;
}
