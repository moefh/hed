/* utf8.c */

#include <stdio.h>

#include "hed.h"
#include "utf8.h"

const void *utf8_prev(const void *start, const void *str)
{
  const uint8_t *s = str;

  s--;
  while ((const void *)s > start && (*s & 0xc0) == 0x80) {
    s--;
  }
  const void *last = s;
  const void *next;
  while ((next = utf8_next(last)) != NULL && next < str) {
    last = next;
  }
  return last;
}

const void *utf8_next(const void *str)
{
  const uint8_t *s = str;

  if (*s == 0)
    return NULL;
  
  if ((s[0] & 0x80) == 0) {
    return s + 1;
  }
  
  if ((s[0] & 0xe0) == 0xc0) {
    if ((s[1] & 0xc0) != 0x80)
      return s + 1;
    uint32_t val = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
    if (val <= 0x7f)
      return s + 1;
    return s + 2;
  }

  if ((s[0] & 0xf0) == 0xe0) {
    if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
      return s + 1;
    uint32_t val = ((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
    if (val <= 0x7ff)
      return s + 1;
    return s + 3;
  }

  if ((s[0] & 0xf8) == 0xf0) {
    if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 || (s[3] & 0xc0) != 0x80)
      return s + 1;
    uint32_t val = ((s[0] & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
    if (val <= 0xffff || val > 0x10ffff)
      return s + 1;
    return s + 4;
  }

  return s + 1;
}

size_t utf8_len(const void *str)
{
  size_t len = 0;
  while ((str = utf8_next(str)) != NULL)
    len++;
  return len;
}

size_t utf8_len_upto(const void *str, const void *end)
{
  size_t len = 0;
  while ((str = utf8_next(str)) != NULL && str <= end)
    len++;
  return len;
}
