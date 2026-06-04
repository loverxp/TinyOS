#ifndef SNAKE_H
#define SNAKE_H

#include "../include/types.h"

// Snake display modes
#define SNAKE_MODE_TEXT     0   // VGA text mode (80x25)
#define SNAKE_MODE_GRAPHICS 1   // VGA graphics mode (future)

// Start the snake game in specified mode
// mode: SNAKE_MODE_TEXT or SNAKE_MODE_GRAPHICS
void snake_start(int mode);

#endif // SNAKE_H