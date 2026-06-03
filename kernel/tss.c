#include "../include/tss.h"
#include "../include/gdt.h"

static struct tss_entry tss;

void tss_init(uint32_t kernel_stack) {
    // Zero out the TSS
    for (int i = 0; i < sizeof(struct tss_entry) / 4; i++) {
        ((uint32_t*)&tss)[i] = 0;
    }

    // Set kernel stack for Ring 0 when entering from Ring 3
    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = kernel_stack;

    // I/O map base beyond TSS limit (no I/O bitmap)
    tss.iomap_base = sizeof(struct tss_entry);

    // Register TSS in GDT
    gdt_set_tss((uint32_t)&tss, sizeof(struct tss_entry) - 1);
}