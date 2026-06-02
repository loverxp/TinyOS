#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// Callback type for character events
typedef void (*keyboard_char_callback_t)(char);

// Register callback triggered on each key press
void keyboard_register_char_callback(keyboard_char_callback_t callback);

// Keyboard interrupt handler
void keyboard_handler(void);

// Initialize keyboard
void keyboard_initialize(void);

#endif // KEYBOARD_H