/* file.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "screen.h"

static int is_cpu_float_little_endian(void)
{
  static int cpu_float_is_little_endian = -1;
  if (cpu_float_is_little_endian < 0) {
    float one = 1.0f;
    uint8_t data[sizeof(float)];
    memcpy(data, &one, sizeof(float));
    cpu_float_is_little_endian = (data[0] == 0) ? 1 : 0;
  }
  return cpu_float_is_little_endian;
}

static struct hed_file *new_file(void)
{
  struct hed_file *file = malloc(sizeof(struct hed_file));
  file->next = NULL;
  file->prev = NULL;
  file->filename = NULL;
  file->data = NULL;
  file->data_len = 0;
  file->modified = false;
  file->show_data = false;
  file->pane = HED_PANE_HEX;
  file->top_line = 0;
  file->cursor_pos = 0;
  return file;
}

struct hed_file *hed_new_file_from_data(uint8_t *data, size_t data_len)
{
  struct hed_file *file = new_file();
  if (! file)
    return NULL;
  
  file->data = data;
  file->data_len = data_len;
  file->modified = (data != NULL);
  return file;
}

void hed_free_file(struct hed_file *file)
{
  if (file->filename)
    free(file->filename);
  if (file->data)
    free(file->data);
  free(file);
}

struct hed_file *hed_read_file(const char *filename)
{
  FILE *f = fopen(filename, "r");
  if (! f) {
    show_msg("ERROR: can't open file '%s'", filename);
    return NULL;
  }

  uint8_t *data = NULL;
  char *new_filename = malloc(strlen(filename) + 1);
  if (! new_filename) {
    show_msg("ERROR: out of memory");
    goto err;
  }
  strcpy(new_filename, filename);
  
  if (fseek(f, 0, SEEK_END) < 0) {
    show_msg("ERROR: can't determine file size");
    goto err;
  }
  long size = ftell(f);
  if (size < 0 || (unsigned long) size > SIZE_MAX) {
    show_msg("ERROR: file is too large");
    goto err;
  }
  if (fseek(f, 0, SEEK_SET) == -1) {
    show_msg("ERROR: can't seek to start of file");
    goto err;
  }
  data = malloc((size_t) size);
  if (! data) {
    show_msg("ERROR: not enough memory for %ld bytes", size);
    goto err;
  }
  
  if (fread(data, 1, size, f) != (size_t) size) {
    show_msg("ERROR: error reading file");
    goto err;
  }
  
  struct hed_file *file = new_file();
  if (! file) {
    show_msg("ERROR: error reading file");
    goto err;
  }
  file->data = data;
  file->data_len = size;
  file->filename = new_filename;
  fclose(f);
  return file;

 err:
  if (new_filename)
    free(new_filename);
  if (data)
    free(data);
  fclose(f);
  return NULL;
}

int hed_write_file(struct hed_file *file, const char *filename)
{
  if (! file->data)
    return 0;

  char *new_filename = NULL;
  if (! file->filename || strcmp(filename, file->filename) != 0) {
    new_filename = malloc(strlen(filename) + 1);
    if (! new_filename)
      return show_msg("ERROR: out of memory");
    strcpy(new_filename, filename);
  }
  
  FILE *f = fopen(filename, "w");
  if (! f) {
    free(new_filename);
    return show_msg("ERROR: can't open file '%s'", filename);
  }
  if (fwrite(file->data, 1, file->data_len, f) != file->data_len) {
    free(new_filename);
    fclose(f);
    return show_msg("ERROR: can't write file '%s'", filename);
  }
  fclose(f);
  show_msg("File saved: '%s'", filename);
  file->modified = false;

  if (new_filename) {
    if (file->filename)
      free(file->filename);
    file->filename = new_filename;
  }
  return 0;
}

bool get_file_u8(struct hed_file *file, size_t pos, uint8_t *data)
{
  if (! file->data || pos + 1 > file->data_len) return false;
  *data = file->data[pos];
  return true;
}

bool get_file_u16(struct hed_file *file, size_t pos, uint16_t *data)
{
  if (! file->data || pos + 2 > file->data_len) return false;

  if (file->endianess == HED_DATA_LITTLE_ENDIAN) {
    *data = (((uint16_t) file->data[pos+1] << 8) |
             ((uint16_t) file->data[pos+0] << 0));
  } else {
    *data = (((uint16_t) file->data[pos+0] << 8) |
             ((uint16_t) file->data[pos+1] << 0));
  }
  return true;
}

bool get_file_u32(struct hed_file *file, size_t pos, uint32_t *data)
{
  if (! file->data || pos + 4 > file->data_len) return false;

  if (file->endianess == HED_DATA_LITTLE_ENDIAN) {
    *data = (((uint32_t) file->data[pos+3] << 24) |
             ((uint32_t) file->data[pos+2] << 16) |
             ((uint32_t) file->data[pos+1] <<  8) |
             ((uint32_t) file->data[pos+0] <<  0));
  } else {
    *data = (((uint32_t) file->data[pos+0] << 24) |
             ((uint32_t) file->data[pos+1] << 16) |
             ((uint32_t) file->data[pos+2] <<  8) |
             ((uint32_t) file->data[pos+3] <<  0));
  }
  return true;
}

bool get_file_u64(struct hed_file *file, size_t pos, uint64_t *data)
{
  if (! file->data || pos + 8 > file->data_len) return false;

  if (file->endianess == HED_DATA_LITTLE_ENDIAN) {
    *data = (((uint64_t) file->data[pos+7] << 56) |
             ((uint64_t) file->data[pos+6] << 48) |
             ((uint64_t) file->data[pos+5] << 40) |
             ((uint64_t) file->data[pos+4] << 32) |
             ((uint64_t) file->data[pos+3] << 24) |
             ((uint64_t) file->data[pos+2] << 16) |
             ((uint64_t) file->data[pos+1] <<  8) |
             ((uint64_t) file->data[pos+0] <<  0));
  } else {
    *data = (((uint64_t) file->data[pos+0] << 56) |
             ((uint64_t) file->data[pos+1] << 48) |
             ((uint64_t) file->data[pos+2] << 40) |
             ((uint64_t) file->data[pos+3] << 32) |
             ((uint64_t) file->data[pos+4] << 24) |
             ((uint64_t) file->data[pos+5] << 16) |
             ((uint64_t) file->data[pos+6] <<  8) |
             ((uint64_t) file->data[pos+7] <<  0));
  }
  return true;
}

bool get_file_f32(struct hed_file *file, size_t pos, float *data)
{
  if (! file->data || pos + 4 > file->data_len) return false;
  if (is_cpu_float_little_endian() == (file->endianess == HED_DATA_LITTLE_ENDIAN)) {
    memcpy(data, &file->data[pos], 4);
    return true;
  }
  uint8_t buf[4] = { file->data[pos+3], file->data[pos+2], file->data[pos+1], file->data[pos+0] };
  memcpy(data, buf, 4);
  return true;
}

bool get_file_f64(struct hed_file *file, size_t pos, double *data)
{
  if (! file->data || pos + 8 > file->data_len) return false;

  if (is_cpu_float_little_endian() == (file->endianess == HED_DATA_LITTLE_ENDIAN)) {
    memcpy(data, &file->data[pos], 8);
    return true;
  }
  uint8_t buf[8] = {
    file->data[pos+7], file->data[pos+6], file->data[pos+5], file->data[pos+4],
    file->data[pos+3], file->data[pos+2], file->data[pos+1], file->data[pos+0],
  };
  memcpy(data, buf, 8);
  return true;
}
