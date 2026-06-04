#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// Callback type for character events
typedef void (*keyboard_char_callback_t)(char);

// Raw scancode callback type (receives scancode + extended flag)
// extended=1 means the scancode was prefixed with 0xE0 (arrow keys, etc.)
typedef void (*keyboard_raw_callback_t)(uint8_t scancode, uint8_t extended);

// Register callback triggered on each key press
void keyboard_register_char_callback(keyboard_char_callback_t callback);

// Register callback for raw scancodes (includes extended keys like arrows)
void keyboard_register_raw_callback(keyboard_raw_callback_t callback);

// Keyboard interrupt handler
void keyboard_handler(void);

// Initialize keyboard
void keyboard_initialize(void);

#endif // KEYBOARD_H