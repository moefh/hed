/* file_sel.c */

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "file_sel.h"
#include "editor.h"
#include "screen.h"
#include "input.h"

struct file_item {
  struct file_item *next;
  struct file_item *prev;
  size_t index;
  bool is_file;
  off_t size;
  char filename[];
};

struct file_sel {
  struct hed_editor *editor;
  bool quit;
  int ret;
  char *dir_name;
  struct file_item *dir_list;

  size_t max_filename_len;
  struct file_item *sel;
  size_t top_line;
};

static int compare_files(const void *p1, const void *p2)
{
  const struct file_item *f1 = *(struct file_item **)p1;
  const struct file_item *f2 = *(struct file_item **)p2;
  if (f1->is_file == f2->is_file)
    return strcmp(f1->filename, f2->filename);
  return (! f1->is_file) ? -1 : 1;
}

static void sort_file_list(struct file_item **list)
{
  size_t n_files = 0;
  for (struct file_item *f = *list; f != NULL; f = f->next)
    n_files++;
  if (n_files <= 1)
    return;
  
  struct file_item **vec = malloc(sizeof(struct file_item *) * n_files);
  if (vec) {
    n_files = 0;
    for (struct file_item *f = *list; f != NULL; f = f->next)
      vec[n_files++] = f;
    qsort(vec, n_files, sizeof(struct file_item *), compare_files);
    for (size_t i = 0; i < n_files; i++) {
      vec[i]->index = i;
      vec[i]->next = (i+1 < n_files) ? vec[i+1] : NULL;
      vec[i]->prev = (i>0) ? vec[i-1] : NULL;
    }
    *list = vec[0];
    free(vec);
  }
}

static int stat_at(const char *dir, const char *file, char **p_buf, size_t *p_buf_len, struct stat *st)
{
  char *buf = *p_buf;

  size_t need_len = strlen(dir) + 1 + strlen(file) + 1;
  if (! buf || *p_buf_len < need_len) {
    buf = realloc(buf, need_len);
    if (! buf)
      return -1;
    *p_buf_len = need_len;
    *p_buf = buf;
  }
  strcpy(buf, dir);
  strcat(buf, "/");
  strcat(buf, file);

  return stat(buf, st);
}

static struct file_item *read_dir_list(const char *dir_name)
{
  DIR *dir = opendir(dir_name);
  if (! dir) {
    show_msg("Can't read directory '%s'", dir_name);
    return NULL;
  }
  char *name_buf = NULL;
  size_t name_buf_len = 0;
  bool is_root = strcmp(dir_name, "/") == 0;
  struct stat st;

  struct file_item *list = NULL;
  while (true) {
    struct dirent *ent = readdir(dir);
    if (! ent)
      break;
    if (strcmp(ent->d_name, ".") == 0 || (is_root && strcmp(ent->d_name, "..") == 0))
      continue;
    if (stat_at(dir_name, ent->d_name, &name_buf, &name_buf_len, &st) < 0)
      continue;
    
    size_t file_name_len = strlen(ent->d_name);
    struct file_item *file = malloc(sizeof(struct file_item) + file_name_len + 1);
    if (! file)
      break;

    strncpy(file->filename, ent->d_name, file_name_len);
    file->filename[file_name_len] = '\0';
    
    file->is_file = (st.st_mode & S_IFMT) == S_IFREG;
    file->size = st.st_size;

    file->prev = NULL;
    file->next = list;
    if (list)
      list->prev = file;
    list = file;
  }
  
  free(name_buf);
  closedir(dir);

  sort_file_list(&list);
  return list;
}

static void free_dir_list(struct file_item *list)
{
  while (list != NULL) {
    struct file_item *next = list->next;
    free(list);
    list = next;
  }
}

/* ============================================================== */
/* === FILE SEL                                                   */
/* ============================================================== */

static void init_file_sel(struct file_sel *fs, struct hed_editor *editor)
{
  fs->editor = editor;
  fs->dir_name = NULL;
  fs->dir_list = NULL;
  fs->max_filename_len = 0;
  fs->sel = NULL;
  fs->quit = false;
  fs->ret = -1;
  fs->top_line = 0;
}

static void destroy_file_sel(struct file_sel *fs)
{
  if (fs->dir_name)
    free(fs->dir_name);
  if (fs->dir_list)
    free_dir_list(fs->dir_list);
}

static char *get_absolute_dir(const char *dir_name)
{
#if BROKEN_REALPATH
  size_t dir_name_len = strlen(dir_name);
  char *new_dir_name = malloc(dir_name_len + 1);
  if (! new_dir_name)
    return show_msg("ERROR: out of memory");
  memcpy(new_dir_name, dir_name, dir_name_len+1);
#else
  char *new_dir_name = realpath(dir_name, NULL);
#endif
  return new_dir_name;
}

static int change_dir(struct file_sel *fs, const char *dir_name)
{
  char *new_dir_name;
  if (fs->dir_name) {
    size_t fs_dir_name_len = strlen(fs->dir_name);
    size_t dir_name_len = strlen(dir_name);
    char *full_dir_name = malloc(fs_dir_name_len + 1 + dir_name_len + 1);
    if (! full_dir_name)
      return show_msg("ERROR: out of memory");
    memcpy(full_dir_name, fs->dir_name, fs_dir_name_len);
    full_dir_name[fs_dir_name_len] = '/';
    memcpy(full_dir_name + fs_dir_name_len + 1, dir_name, dir_name_len + 1);
    new_dir_name = get_absolute_dir(full_dir_name);
    free(full_dir_name);
  } else
    new_dir_name = get_absolute_dir(dir_name);

  struct file_item *dir_list = read_dir_list(new_dir_name);
  if (! dir_list) {
    free(new_dir_name);
    return -1;
  }
  size_t num_files = 0;
  size_t max_filename_len = 0;
  for (struct file_item *f = dir_list; f != NULL; f = f->next) {
    size_t filename_len = strlen(f->filename);
    if (filename_len > max_filename_len)
      max_filename_len = filename_len;
    num_files++;
  }

  if (fs->dir_name)
    free(fs->dir_name);
  if (fs->dir_list)
    free_dir_list(fs->dir_list);
  fs->dir_name = new_dir_name;
  fs->dir_list = dir_list;
  fs->max_filename_len = max_filename_len;

  fs->sel = fs->dir_list;
  fs->top_line = 0;
  return 0;
}

static void draw_header(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;

  reset_color();
  set_color(FG_BLACK, BG_GRAY);
  move_cursor(1, 1);
  clear_eol();

  if (fs->dir_name)
    out(" DIR: %.*s", scr->w - 30, fs->dir_name);
  
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

  struct file_item *file = fs->dir_list;
  for (size_t i = 0; i < fs->top_line && file != NULL; i++)
    file = file->next;

  int line = 0;
  int col_len = (fs->max_filename_len+1 > INT_MAX) ? INT_MAX : (int)(fs->max_filename_len+1);
  if (col_len > scr->w - 20)
    col_len = scr->w - 20;
  while (file != NULL && line + 1 + BORDER_LINES < scr->h) {
    if (file == fs->sel)
      set_color(FG_BLACK, BG_GRAY);
    else
      reset_color();
    move_cursor(1, line + 1 + HEADER_LINES);

    // filename
    size_t filename_len = strlen(file->filename);
    if (filename_len > (size_t) col_len)
      out("%.*s", col_len, file->filename);
    else
      out("%s", file->filename);
    for (size_t i = filename_len; i < (size_t)col_len; i++)
      out(" ");

    // size
    if (file->is_file)
      out("%14" PRIu64 " bytes", (uint64_t) file->size);
    else
      out("               (dir)");

    // clear rest of line
    if (file == fs->sel)
      reset_color();
    clear_eol();
        
    file = file->next;
    line++;
  }

  while (line + 1 + BORDER_LINES < scr->h) {
    reset_color();
    move_cursor(1, line + 1 + HEADER_LINES);
    clear_eol();
    line++;
  }
  
  hed_scr_flush();
  scr->redraw_needed = false;
}

static void move_sel_up(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;

  if (fs->sel->prev) {
    fs->sel = fs->sel->prev;
    if (fs->sel->index < fs->top_line)
      fs->top_line = fs->sel->index;
    scr->redraw_needed = true;
  }
}

static void move_sel_down(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;
  size_t n_page_lines = scr->h - 1 - BORDER_LINES;

  if (fs->sel->next) {
    fs->sel = fs->sel->next;
    if (fs->top_line + n_page_lines - 1 < fs->sel->index)
      fs->top_line = fs->sel->index - n_page_lines + 1;
    scr->redraw_needed = true;
  }
}

static void move_sel_page_up(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;
  size_t n_page_lines = scr->h - 1 - BORDER_LINES;

  for (size_t i = 0; i < n_page_lines; i++) {
    if (fs->sel->prev)
      fs->sel = fs->sel->prev;
    else
      break;
  }
  if (fs->sel->index < fs->top_line) {
    if (fs->sel->index > n_page_lines/2)
      fs->top_line = fs->sel->index - n_page_lines/2;
    else
      fs->top_line = 0;
  }
  scr->redraw_needed = true;
}

static void move_sel_page_down(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;
  size_t n_page_lines = scr->h - 1 - BORDER_LINES;

  for (size_t i = 0; i < n_page_lines; i++) {
    if (fs->sel->next)
      fs->sel = fs->sel->next;
    else
      break;
  }
  if (fs->top_line + n_page_lines - 1 < fs->sel->index) {
    if (fs->sel->index > n_page_lines/2)
      fs->top_line = fs->sel->index - n_page_lines/2;
    else
      fs->top_line = 0;
  }
  scr->redraw_needed = true;
}

static void process_input(struct file_sel *fs)
{
  struct hed_screen *scr = &fs->editor->screen;
  char key_err[64];

  int k = read_key(scr->term_fd, key_err, sizeof(key_err));
  switch (k) {
  case KEY_REDRAW:
    reset_color();
    clear_screen();
    scr->redraw_needed = true;
    break;

  case CTRL_KEY('l'):
    scr->redraw_needed = true;
    break;
    
  case CTRL_KEY('c'):
    scr->redraw_needed = true;
    fs->quit = true;
    fs->ret = -1;
    break;

  case '\r':
    if (fs->sel) {
      if (fs->sel->is_file) {
        fs->quit = true;
        fs->ret = 0;
        return;
      }
      change_dir(fs, fs->sel->filename);
      scr->redraw_needed = true;
    }
        
    break;

  case CTRL_KEY('p'):  move_sel_up(fs); break;
  case CTRL_KEY('n'):  move_sel_down(fs); break;
  case CTRL_KEY('y'):  move_sel_page_up(fs); break;
  case CTRL_KEY('v'):  move_sel_page_down(fs); break;
  case KEY_ARROW_UP:   move_sel_up(fs); break;
  case KEY_ARROW_DOWN: move_sel_down(fs); break;
  case KEY_PAGE_UP:    move_sel_page_up(fs); break;
  case KEY_PAGE_DOWN:  move_sel_page_down(fs); break;
  }
}

int hed_select_file(struct hed_editor *editor, char *filename, size_t max_filename_len)
{
  UNUSED(filename);
  UNUSED(max_filename_len);
  
  clear_msg();

  struct hed_screen *scr = &editor->screen;
  struct file_sel file_sel;
  init_file_sel(&file_sel, editor);
  change_dir(&file_sel, ".");
  
  reset_color();
  clear_screen();
  scr->redraw_needed = true;
  while (! editor->quit && ! file_sel.quit) {
    if (scr->redraw_needed)
      draw_main_screen(&file_sel);
    process_input(&file_sel);
  }

  if (file_sel.ret >= 0) {
    if (file_sel.sel)
      snprintf(filename, max_filename_len, "%s/%s", file_sel.dir_name, file_sel.sel->filename);
    else
      file_sel.ret = -1;
  }
  
  destroy_file_sel(&file_sel);

  reset_color();
  clear_screen();
  scr->redraw_needed = true;
  return file_sel.ret;
}
