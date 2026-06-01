#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// Check if keyboard has input
int keyboard_has_input(void);

// Read a character from keyboard buffer (blocking)
char keyboard_read_char(void);

// Read a character from keyboard buffer (non-blocking)
char keyboard_try_read(void);

// Keyboard interrupt handler
void keyboard_handler(void);

// Initialize keyboard
void keyboard_initialize(void);

#endif // KEYBOARD_H
