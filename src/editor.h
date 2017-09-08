/* editor.h */

#ifndef EDITOR_H_FILE
#define EDITOR_H_FILE

#include "hed.h"
#include "screen.h"

#define HEADER_LINES     2
#define FOOTER_LINES     2
#define BORDER_LINES     (HEADER_LINES+FOOTER_LINES)
#define KEY_HELP_SPACING 16

enum hed_edit_mode {
  HED_MODE_DEFAULT,
  HED_MODE_READ_FILENAME,
  HED_MODE_READ_STRING,
  HED_MODE_READ_YESNO,
};

enum hed_edit_pane {
  HED_PANE_HEX,
  HED_PANE_TEXT,
};

struct hed_editor {
  char *filename;
  uint8_t *data;
  size_t data_len;
  bool modified;
  bool quit;

  bool read_only;
  char search_str[256];
  bool half_byte_edited;
  enum hed_edit_pane pane;
  enum hed_edit_mode edit_mode;
  struct hed_screen screen;
};

void hed_init_editor(struct hed_editor *editor);
int hed_read_file(struct hed_editor *editor, const char *filename);
int hed_set_data(struct hed_editor *editor, uint8_t *data, size_t data_len);
void hed_set_cursor_pos(struct hed_editor *editor, size_t pos, size_t visible_len_after);
int hed_run_editor(struct hed_editor *editor, size_t start_cursor_pos);

void hed_draw_key_help(int x, int y, const char *key, const char *help);

#endif /* EDITOR_H_FILE */
