#include "../include/prng.h"

/* xorshift32 state — must be non-zero */
static uint32_t prng_state = 1;

void prng_seed(uint32_t seed) {
    prng_state = seed ? seed : 1;  /* 0 is invalid for xorshift */
}

uint32_t prng_next(void) {
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

uint32_t prng_range(uint32_t max) {
    if (max == 0) return 0;
    return prng_next() % max;
}
