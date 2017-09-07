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
  char err_msg[256];
  struct hed_screen screen;
};

int hed_run_editor(struct hed_editor *editor, const char *filename);

#endif /* EDITOR_H_FILE */
