/* editor.h */

#ifndef EDITOR_H_FILE
#define EDITOR_H_FILE

#include "hed.h"
#include "screen.h"

#define HEADER_LINES     2
#define FOOTER_LINES     2
#define BORDER_LINES     (HEADER_LINES+FOOTER_LINES)
#define KEY_HELP_SPACING 16

enum hed_editor_mode {
  HED_MODE_DEFAULT,
  HED_MODE_READ_FILENAME,
  HED_MODE_READ_STRING,
  HED_MODE_READ_YESNO,
};

struct hed_file;

struct hed_editor {
  bool quit;
  bool half_byte_edited;
  bool read_only;
  char search_str[256];
  enum hed_editor_mode mode;
  struct hed_screen screen;
  struct hed_file *file;
};

void hed_init_editor(struct hed_editor *editor);
void hed_add_file(struct hed_editor *editor, struct hed_file *file);
int hed_run_editor(struct hed_editor *editor, size_t start_cursor_pos);
void hed_draw_key_help(int x, int y, const char *key, const char *help);

void hed_set_cursor_pos(struct hed_editor *editor, size_t pos, size_t visible_len_after);

#endif /* EDITOR_H_FILE */
