/* file.h */

#ifndef FILE_H_FILE
#define FILE_H_FILE

#include "hed.h"

enum hed_edit_pane {
  HED_PANE_HEX,
  HED_PANE_TEXT,
};

struct hed_file {
  struct hed_file *next;
  struct hed_file *prev;
  
  uint8_t *data;
  size_t data_len;
  char *filename;
  bool modified;
  
  enum hed_edit_pane pane;
  size_t cursor_pos;
  size_t top_line;
};

struct hed_file *hed_read_file(const char *filename);
struct hed_file *hed_new_file_from_data(uint8_t *data, size_t data_len);
void hed_free_file(struct hed_file *file);

int hed_write_file(struct hed_file *file, const char *filename);
struct hed_file *hed_read_file(const char *filename);

#endif /* FILE_H_FILE */
