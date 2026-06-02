#ifndef TIMER_H
#define TIMER_H

#include "types.h"

// Timer interrupt handler
void timer_handler(void);

// Get current tick count
uint32_t timer_get_ticks(void);

// Callback type for 1-second events
typedef void (*timer_second_callback_t)(void);

// Register callback triggered every second
void timer_register_second_callback(timer_second_callback_t callback);

// Initialize timer with specified frequency (Hz)
void timer_initialize(uint32_t frequency);

#endif // TIMER_H