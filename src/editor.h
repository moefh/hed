/* editor.h */

#ifndef EDITOR_H_FILE
#define EDITOR_H_FILE

#include "hed.h"
#include "screen.h"

struct hed_editor {
  char *filename;
  uint8_t *data;
  size_t data_len;
  bool modified;
  bool quit;
  bool reading_string;
  bool msg_was_set;
  char cur_msg[256];
  struct hed_screen screen;
};

int hed_run_editor(struct hed_editor *editor, const char *filename);
int hed_show_msg(struct hed_editor *editor, const char *fmt, ...) HED_PRINTF_FORMAT(2, 3);

#endif /* EDITOR_H_FILE */
