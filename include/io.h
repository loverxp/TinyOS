#ifndef IO_H
#define IO_H

#include "types.h"

// Output a byte to an I/O port
extern void outb(uint16_t port, uint8_t value);

// Output a word to an I/O port
extern void outw(uint16_t port, uint16_t value);

// Output a long to an I/O port
extern void outl(uint16_t port, uint32_t value);

// Input a byte from an I/O port
extern uint8_t inb(uint16_t port);

// Input a word from an I/O port
extern uint16_t inw(uint16_t port);

// Input a long from an I/O port
extern uint32_t inl(uint16_t port);

// Wait for I/O operation to complete
static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

// Enable interrupts
static inline void enable_interrupts(void) {
    asm volatile("sti");
}

// Disable interrupts
static inline void disable_interrupts(void) {
    asm volatile("cli");
}

// Halt the CPU
static inline void halt(void) {
    asm volatile("hlt");
}

#endif // IO_H
