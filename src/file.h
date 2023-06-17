/* file.h */

#ifndef FILE_H_FILE
#define FILE_H_FILE

#include "hed.h"

enum hed_edit_pane {
  HED_PANE_HEX,
  HED_PANE_TEXT,
};

enum hed_data_endianess {
  HED_DATA_LITTLE_ENDIAN,
  HED_DATA_BIG_ENDIAN,
};

enum hed_data_signedness {
  HED_DATA_UNSIGNED,
  HED_DATA_SIGNED,
};

struct hed_file {
  struct hed_file *next;
  struct hed_file *prev;
  
  uint8_t *data;
  size_t data_len;
  char *filename;
  bool modified;
  bool show_data;
  enum hed_data_endianess endianess;
  enum hed_data_signedness signedness;
  
  enum hed_edit_pane pane;
  size_t cursor_pos;
  size_t top_line;
};

struct hed_file *hed_read_file(const char *filename);
struct hed_file *hed_new_file_from_data(uint8_t *data, size_t data_len);
void hed_free_file(struct hed_file *file);

int hed_write_file(struct hed_file *file, const char *filename);
struct hed_file *hed_read_file(const char *filename);

bool get_file_u8(struct hed_file *file, size_t pos, uint8_t *data);
bool get_file_u16(struct hed_file *file, size_t pos, uint16_t *data);
bool get_file_u32(struct hed_file *file, size_t pos, uint32_t *data);
bool get_file_u64(struct hed_file *file, size_t pos, uint64_t *data);
bool get_file_f32(struct hed_file *file, size_t pos, float *data);
bool get_file_f64(struct hed_file *file, size_t pos, double *data);

#endif /* FILE_H_FILE */
