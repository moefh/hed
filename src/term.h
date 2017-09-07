/* term.h */

#ifndef TERM_H_FILE
#define TERM_H_FILE

int term_setup_raw(void);
void term_restore(void);
int term_get_window_size(int *width, int *height);

#endif /* TERM_H_FILE */
