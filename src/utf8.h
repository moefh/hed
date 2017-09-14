/* utf8.h */

#ifndef UTF8_H_FILE
#define UTF8_H_FILE

size_t utf8_len(const void *str);
size_t utf8_len_upto(const void *str, const void *end);
const void *utf8_next(const void *str);
const void *utf8_prev(const void *start, const void *str);

#endif /* UTF8_H_FILE */
