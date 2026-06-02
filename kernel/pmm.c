#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/io.h"

// Kernel end symbol from linker.ld
extern uint32_t kernel_end;

// Bitmap data
static uint8_t* bitmap = NULL;
static uint32_t bitmap_size = 0;
static uint32_t total_pages = 0;
static uint32_t total_memory_kb = 0;
static uint32_t free_pages = 0;

// Serial debug - forward declarations
static void serial_write(char c);

static void serial_string(const char* s) {
    while (*s) {
        while ((inb(0x3FD) & 0x20) == 0);
        outb(0x3F8, *s++);
    }
}

static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
        serial_write(hex[(n >> i) & 0xF]);
}

static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_dec(uint32_t n) {
    char buf[12];
    int i = 0;
    if (n == 0) { serial_write('0'); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) serial_write(buf[--i]);
}

// Mark a page as used in the bitmap
static void pmm_mark_used(uint32_t page_idx) {
    size_t byte = page_idx / 8;
    uint8_t bit = page_idx % 8;
    if (byte < bitmap_size && !(bitmap[byte] & (1 << bit))) {
        bitmap[byte] |= (1 << bit);
        free_pages--;
    }
}

// Mark a page as free in the bitmap
static void pmm_mark_free(uint32_t page_idx) {
    size_t byte = page_idx / 8;
    uint8_t bit = page_idx % 8;
    if (byte < bitmap_size && (bitmap[byte] & (1 << bit))) {
        bitmap[byte] &= ~(1 << bit);
        free_pages++;
    }
}

// Check if a page is free
static int pmm_is_free(uint32_t page_idx) {
    size_t byte = page_idx / 8;
    uint8_t bit = page_idx % 8;
    if (byte >= bitmap_size) return 0;
    return !(bitmap[byte] & (1 << bit));
}

void pmm_init(uint32_t multiboot_info_addr) {
    serial_string("[PMM] Init... multiboot_info_addr=0x");
    serial_hex(multiboot_info_addr);
    serial_string("\n");

    // If no Multiboot info (e.g. QEMU -kernel mode), assume 32MB
    if (multiboot_info_addr == 0) {
        serial_string("[PMM] No Multiboot info, assuming 32MB\n");
        total_memory_kb = 32768;
    } else {
        // Parse Multiboot info structure for memory size
        // mem_lower at offset 4, mem_upper at offset 8
        uint32_t flags = *(volatile uint32_t*)multiboot_info_addr;

        if (flags & 0x01) {
            uint32_t mem_lower = *(volatile uint32_t*)(multiboot_info_addr + 4);
            uint32_t mem_upper = *(volatile uint32_t*)(multiboot_info_addr + 8);
            serial_string("[PMM] Multiboot mem_lower=");
            serial_dec(mem_lower);
            serial_string(" KB, mem_upper=");
            serial_dec(mem_upper);
            serial_string(" KB\n");

            // Validate: mem_lower should be ~640 KB, mem_upper reasonable
            if (mem_lower <= 1024 && mem_upper <= 1048576 && mem_upper > 0) {
                total_memory_kb = mem_lower + mem_upper;
            } else {
                serial_string("[PMM] Values look invalid, falling back to 32MB\n");
                total_memory_kb = 32768;
            }
        } else {
            total_memory_kb = 32768;  // Fallback to 32MB
            serial_string("[PMM] No memory info in Multiboot flags, assuming 32MB\n");
        }
    }

    serial_string("[PMM] Total memory: ");
    serial_dec(total_memory_kb);
    serial_string(" KB (");
    serial_dec(total_memory_kb / 1024);
    serial_string(" MB)\n");

    // Calculate total 4KB pages
    total_pages = (total_memory_kb * 1024) / PAGE_SIZE;
    serial_string("[PMM] Total 4KB pages: ");
    serial_dec(total_pages);
    serial_string("\n");

    // Bitmap size: 1 bit per page
    bitmap_size = (total_pages + 7) / 8;
    serial_string("[PMM] Bitmap size: ");
    serial_dec(bitmap_size);
    serial_string(" bytes\n");

    // Place bitmap right after kernel end, page-aligned
    uint32_t kend = (uint32_t)&kernel_end;
    uint32_t bitmap_addr = (kend + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    serial_string("[PMM] kernel_end=0x");
    serial_hex(kend);
    serial_string(", bitmap at 0x");
    serial_hex(bitmap_addr);
    serial_string("\n");

    bitmap = (uint8_t*)bitmap_addr;

    // Initialize all pages as free
    memset(bitmap, 0, bitmap_size);
    free_pages = total_pages;

    // Mark pages 0 to (bitmap_addr + bitmap_size) as used
    // This covers: BIOS area (0-1MB), kernel code/data, and bitmap itself
    uint32_t last_used_page = (bitmap_addr + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < last_used_page; i++) {
        pmm_mark_used(i);
    }

    serial_string("[PMM] Reserved ");
    serial_dec(last_used_page);
    serial_string(" pages for kernel+bitmap\n");

    serial_string("[PMM] Free: ");
    serial_dec(free_pages);
    serial_string(", Used: ");
    serial_dec(total_pages - free_pages);
    serial_string(" pages\n");

    serial_string("[PMM] OK\n");
}

void* pmm_alloc_page(void) {
    // Linear scan for first free page
    for (uint32_t i = 0; i < total_pages; i++) {
        if (pmm_is_free(i)) {
            pmm_mark_used(i);
            return (void*)(i * PAGE_SIZE);
        }
    }
    // Out of memory
    serial_string("[PMM] Out of memory!\n");
    return NULL;
}

void pmm_free_page(void* addr) {
    uint32_t page_idx = (uint32_t)addr / PAGE_SIZE;
    if (page_idx < total_pages) {
        pmm_mark_free(page_idx);
    }
}

uint32_t pmm_get_total_pages(void) {
    return total_pages;
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}

uint32_t pmm_get_used_pages(void) {
    return total_pages - free_pages;
}

uint32_t pmm_get_total_memory_kb(void) {
    return total_memory_kb;
}