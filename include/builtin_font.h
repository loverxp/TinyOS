#ifndef BUILTIN_FONT_H
#define BUILTIN_FONT_H

#include "types.h"

/* Get pointer to built-in 8x16 bitmap font data.
   Returns 4096 bytes (256 chars * 16 bytes each) covering ASCII 0x20-0x7E.
   Chars before 0x20 and after 0x7E map to spaces (all zeros). */
const uint8_t* builtin_font_get(void);

#endif /* BUILTIN_FONT_H */