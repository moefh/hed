/* term.c */

#include <stdlib.h>
#include <string.h>

#include "editor.h"

int main(int argc, char **argv)
{
  const char *filename = (argc >= 2) ? argv[1] : NULL;
  
  struct hed_editor editor;
  return hed_run_editor(&editor, filename);
}
