/* term.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "editor.h"

static uint8_t *read_stdin(size_t *ret_len)
{
  uint8_t *data = NULL;
  size_t len = 0;
  size_t cap = 0;

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

static void print_help(const char *progname)
{
  printf("%s [options] [+OFFSET [FILE]]\n", progname);
  printf("\n"
         "options:\n"
         " -V               show version information and exit\n"
         " -h               show this help and exit\n"
         " -v               view mode (read-only)\n"
         " +OFFSET          start at OFFSET (may have prefix 0x or 0 for hex or octal)\n"
         " FILE             file to edit or view, can be - for stdin\n");
}

static void print_version(void)
{
  printf("hed, a tiny hex editor\n");
  printf("Version " HED_VERSION " compiled on " __DATE__ "\n");
  printf("Copyright (C) 2017 Ricardo R. Massaro\n");
  printf("Source code: https://github.com/ricardo-massaro/hed\n");
}

int main(int argc, char **argv)
{
  const char *filename = NULL;
  bool view_mode = false;
  unsigned long offset = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '+') {
      char *end = NULL;
      errno = 0;
      offset = strtoul(argv[i] + 1, &end, 0);
      if (errno != 0 || *end != '\0') {
        fprintf(stderr, "%s: invalid offset: %s\n", argv[0], argv[i] + 1);
        exit(1);
      }
    } else if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'V': print_version(); exit(0);
      case 'h': print_help(argv[0]); exit(0);
      case 'v': view_mode = true; break;
      case '\0': filename = argv[i]; break;
      default:
        fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]);
        exit(1);
      }
    } else {
      if (filename) {
        fprintf(stderr, "%s: too many files given\n", argv[0]);
        exit(1);
      }
      filename = argv[i];
    }
  }

  struct hed_editor editor;
  hed_init_editor(&editor);
  if (view_mode)
    editor.read_only = true;

  if (filename) {
    if (strcmp(filename, "-") == 0) {
      size_t data_len = 0;
      uint8_t *data = read_stdin(&data_len);
      if (! data)
        exit(1);
      hed_set_data(&editor, data, data_len);
    } else
      hed_read_file(&editor, argv[1]);
  }
  
  return hed_run_editor(&editor, offset);
}
