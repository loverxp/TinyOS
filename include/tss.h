#ifndef TSS_H
#define TSS_H

#include "types.h"

// Task State Segment structure
struct tss_entry {
    uint32_t prev_tss;   // 0x00
    uint32_t esp0;       // 0x04 - Kernel stack pointer for Ring 0
    uint32_t ss0;        // 0x08 - Kernel stack segment for Ring 0
    uint32_t esp1;       // 0x0C
    uint32_t ss1;        // 0x10
    uint32_t esp2;       // 0x14
    uint32_t ss2;        // 0x18
    uint32_t cr3;        // 0x1C
    uint32_t eip;        // 0x20
    uint32_t eflags;     // 0x24
    uint32_t eax;        // 0x28
    uint32_t ecx;        // 0x2C
    uint32_t edx;        // 0x30
    uint32_t ebx;        // 0x34
    uint32_t esp;        // 0x38
    uint32_t ebp;        // 0x3C
    uint32_t esi;        // 0x40
    uint32_t edi;        // 0x44
    uint32_t es;         // 0x48
    uint32_t cs;         // 0x4C
    uint32_t ss;         // 0x50
    uint32_t ds;         // 0x54
    uint32_t fs;         // 0x58
    uint32_t gs;         // 0x5C
    uint32_t ldt;        // 0x60
    uint16_t trap;       // 0x64
    uint16_t iomap_base; // 0x66
} __attribute__((packed));

// Initialize TSS with kernel stack for Ring 0 entry
void tss_init(uint32_t kernel_stack);

#endif