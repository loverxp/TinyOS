#include "../include/gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    // [0] Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // [1] Kernel Code: 0x08, DPL=0, executable, readable, 4KB gran, 32-bit
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // [2] Kernel Data: 0x10, DPL=0, writable
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    // [3] User Code: 0x18, DPL=3, executable, readable
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);

    // [4] User Data: 0x20, DPL=3, writable
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

    // [5] TSS placeholder: 0x28, will fix up in gdt_set_tss
    gdt_set_gate(5, 0, 0, 0x89, 0x40);

    // Load GDT with lgdt, then far jump to reload CS
    __asm__ volatile (
        "lgdt %0\n"
        "ljmp %1, $1f\n"
        "1:\n"
        "mov %2, %%ds\n"
        "mov %2, %%es\n"
        "mov %2, %%fs\n"
        "mov %2, %%gs\n"
        "mov %2, %%ss\n"
        :
        : "m"(gp), "i"(GDT_KERNEL_CODE), "r"((uint16_t)GDT_KERNEL_DATA)
        : "memory"
    );
}

void gdt_set_tss(uint32_t tss_base, uint32_t tss_limit) {
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x40);

    // Reload GDT and load task register
    __asm__ volatile (
        "lgdt %0\n"
        "ltr %1\n"
        :
        : "m"(gp), "r"((uint16_t)GDT_TSS)
        : "memory"
    );
}