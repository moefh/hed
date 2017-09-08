/* hed.h */

#ifndef HED_H_FILE
#define HED_H_FILE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define HED_VERSION "0.2"
#define HED_BANNER  "hed v" HED_VERSION

#if defined(__GNUC__)
#define HED_PRINTF_FORMAT(x,y) __attribute__((format (printf, (x), (y))))
#else
#define HED_PRINTF_FORMAT(x,y)
#endif

#define UNUSED(x)  ((void)x)

#endif /* HED_H_FILE */
