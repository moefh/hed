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
  size_t dir_list_size;
  
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
  if (n_files == 0)
    return;
  
  struct file_item **vec = malloc(sizeof(struct file_item *) * n_files);
  if (vec) {
    n_files = 0;
    for (struct file_item *f = *list; f != NULL; f = f->next)
      vec[n_files++] = f;
    qsort(vec, n_files, sizeof(struct file_item *), compare_files);
    for (size_t i = 0; i < n_files; i++) {
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
  char *buf = NULL;
  size_t buf_len = 0;
  struct stat st;

  struct file_item *list = NULL;
  while (true) {
    struct dirent *ent = readdir(dir);
    if (! ent)
      break;
    if (strcmp(ent->d_name, ".") == 0)
      continue;
    if (stat_at(dir_name, ent->d_name, &buf, &buf_len, &st) < 0)
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
  
  closedir(dir);
  free(buf);

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
  fs->sel = NULL;
  fs->quit = false;
  fs->ret = -1;
  fs->dir_list_size = 0;
  fs->top_line = 0;
}

static void destroy_file_sel(struct file_sel *fs)
{
  if (fs->dir_name)
    free(fs->dir_name);
  if (fs->dir_list)
    free_dir_list(fs->dir_list);
}

static int change_dir(struct file_sel *fs, const char *dir_name)
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
  
  struct file_item *dir_list = read_dir_list(dir_name);
  if (! dir_list) {
    free(new_dir_name);
    return -1;
  }
  size_t num_files = 0;
  for (struct file_item *f = dir_list; f != NULL; f = f->next)
    num_files++;

  if (fs->dir_name)
    free(fs->dir_name);
  if (fs->dir_list)
    free_dir_list(fs->dir_list);
  fs->dir_name = new_dir_name;
  fs->dir_list = dir_list;
  fs->dir_list_size = num_files;

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
  while (file != NULL && line + 2 + BORDER_LINES < scr->h) {
    if (file == fs->sel)
      set_color(FG_BLACK, BG_GRAY);
    else
      reset_color();
    move_cursor(1, line + 1 + HEADER_LINES);
    out("%-.*s", scr->w - 20, file->filename);
    clear_eol();
    move_cursor(scr->w - 20, line + 1 + HEADER_LINES);

    if (file->is_file)
      out("%14" PRIu64 " bytes", (uint64_t) file->size);
    else
      out("               (dir)");
        
    file = file->next;
    line++;
  }

  while (line + 2 + BORDER_LINES < scr->h) {
    reset_color();
    move_cursor(1, line + 1 + HEADER_LINES);
    clear_eol();
    line++;
  }
  
  hed_scr_flush();
  scr->redraw_needed = false;
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
    
  case KEY_ARROW_UP:
    if (fs->sel->prev) {
      fs->sel = fs->sel->prev;
      scr->redraw_needed = true;
    }
    break;

  case KEY_ARROW_DOWN:
    if (fs->sel->next) {
      fs->sel = fs->sel->next;
      scr->redraw_needed = true;
    }
    break;
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
