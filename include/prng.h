#ifndef PRNG_H
#define PRNG_H

#include "types.h"

/* Seed the PRNG with an initial value (e.g. from timer ticks or RTC) */
void prng_seed(uint32_t seed);

/* Generate a pseudo-random 32-bit number using xorshift32 */
uint32_t prng_next(void);

/* Generate a random number in range [0, max) */
uint32_t prng_range(uint32_t max);

#endif /* PRNG_H */
