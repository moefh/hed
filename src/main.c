/* term.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "editor.h"

static uint8_t *read_stdin(size_t *ret_len)
{
  uint8_t *data = NULL;
  size_t len = 0;
  size_t cap = 0;

  fprintf(stderr, "Reading from stdin, press CTRL+C to abort\n");
  while (true) {
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    if (n == 0)
      break;
    if (len + n > cap) {
      while (len + n > cap)
        cap = (cap == 0) ? 16*1024 : cap * 2;
      data = realloc(data, cap);
      if (! data) {
        fprintf(stderr, "ERROR: out of memory\n");
        return NULL;
      }
    }
    memcpy(data + len, buf, n);
    len += n;
  }
  *ret_len = len;
  return data;
}

int main(int argc, char **argv)
{
  struct hed_editor editor;
  hed_init_editor(&editor);

  if (argc == 2) {
    if (strcmp(argv[1], "-") == 0) {
      size_t data_len = 0;
      uint8_t *data = read_stdin(&data_len);
      if (! data)
        exit(1);
      hed_set_data(&editor, data, data_len);
    } else
      hed_read_file(&editor, argv[1]);
  }
  return hed_run_editor(&editor);
}
