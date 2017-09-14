/* file.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "screen.h"

static struct hed_file *new_file(void)
{
  struct hed_file *file = malloc(sizeof(struct hed_file));
  file->next = NULL;
  file->prev = NULL;
  file->filename = NULL;
  file->data = NULL;
  file->data_len = 0;
  file->modified = false;
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
