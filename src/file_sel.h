/* file_sel.h */

#ifndef FILE_SEL_H_FILE
#define FILE_SEL_H_FILE

struct hed_editor;

int hed_select_file(struct hed_editor *editor, char *filename, size_t max_filename_len);

#endif /* FILE_SEL_H_FILE */
