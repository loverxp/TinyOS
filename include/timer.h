#ifndef TIMER_H
#define TIMER_H

#include "types.h"

// Timer interrupt handler
void timer_handler(void);

// Get current tick count
uint32_t timer_get_ticks(void);

// Sleep for specified number of milliseconds
void timer_sleep(uint32_t milliseconds);

// Initialize timer with specified frequency (Hz)
void timer_initialize(uint32_t frequency);

#endif // TIMER_H
