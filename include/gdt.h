#ifndef GDT_H
#define GDT_H

#include "types.h"

// Segment selectors
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18  // DPL=3
#define GDT_USER_DATA   0x20  // DPL=3
#define GDT_TSS         0x28

// Initialize and load GDT with kernel/user segments
void gdt_init(void);

// Set TSS descriptor and load task register
void gdt_set_tss(uint32_t tss_base, uint32_t tss_limit);

#endif