#include "../include/mm.h"
#include "../include/pmm.h"
#include "../include/vga.h"
#include "../include/string.h"

// Memory allocator using PMM page allocator
// Block-based allocator with free list coalescing
//
// Block header (16 bytes):
//   magic - validation marker
//   size  - total block size including header (always 16-byte aligned)
//   free  - 0=used, 1=free
//   next  - pointer to next block (NULL = end of list)
//
#define BLOCK_MAGIC  0x1CEB00DA
#define HEADER_SIZE  16
#define MIN_BLOCK    32  // Minimum block size (16 header + 16 data)

struct block_header {
    uint32_t magic;
    uint32_t size;
    uint32_t free;
    struct block_header* next;
};

static struct block_header* block_list = NULL;

// Serial debug output
static void sstr(const char* s) {
    while (*s) {
        asm volatile("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8) : "memory");
    }
}

static void shex(uint32_t n) {
    const char* h = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        char c = h[(n >> i) & 0xF];
        asm volatile("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8) : "memory");
    }
}

static ssize_t sdec_int(uint32_t n) {
    char buf[12];
    int i = 0;
    if (n == 0) { char z = '0'; asm volatile("outb %0, %1" : : "a"((uint8_t)z), "Nd"((uint16_t)0x3F8) : "memory"); return 1; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { char c = buf[--i]; asm volatile("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8) : "memory"); }
    return 1;
}

// Align size up to 16 bytes
static inline uint32_t align16(uint32_t size) {
    return (size + 15) & ~15;
}

// Add a new page to the heap as a free block
static void mm_add_page(void) {
    void* page = pmm_alloc_page();
    if (!page) {
        sstr("[MM] Failed to allocate page!\n");
        return;
    }

    struct block_header* blk = (struct block_header*)page;
    blk->magic = BLOCK_MAGIC;
    blk->size = PAGE_SIZE;
    blk->free = 1;
    blk->next = NULL;

    // Append to end of list
    if (!block_list) {
        block_list = blk;
    } else {
        struct block_header* cur = block_list;
        while (cur->next) cur = cur->next;
        cur->next = blk;
    }

    sstr("[MM] Added page at 0x");
    shex((uint32_t)page);
    sstr(", block size=");
    sdec_int(PAGE_SIZE);
    sstr("\n");
}

// Split a block: split 'blk' at 'split_size' bytes
static void block_split(struct block_header* blk, uint32_t split_size) {
    // Only split if remaining is >= MIN_BLOCK
    if (blk->size - split_size < MIN_BLOCK) return;

    struct block_header* new_blk = (struct block_header*)((uint8_t*)blk + split_size);
    new_blk->magic = BLOCK_MAGIC;
    new_blk->size = blk->size - split_size;
    new_blk->free = 1;
    new_blk->next = blk->next;

    blk->size = split_size;
    blk->next = new_blk;
}

// Coalesce adjacent free blocks
static void block_coalesce(void) {
    struct block_header* cur = block_list;
    while (cur && cur->next) {
        struct block_header* nxt = cur->next;
        // Check if cur and nxt are adjacent in memory
        if (cur->free && nxt->free &&
            (uint8_t*)cur + cur->size == (uint8_t*)nxt) {
            cur->size += nxt->size;
            cur->next = nxt->next;
            // Continue checking with same cur (next might also be adjacent)
        } else {
            cur = nxt;
        }
    }
}

void mm_init(void) {
    sstr("[MM] Initializing heap...\n");

    // Allocate initial pages (4 pages = 16KB initial heap)
    for (int i = 0; i < 4; i++) {
        mm_add_page();
    }

    // Coalesce initial pages (they should be adjacent if PMM is linear)
    block_coalesce();

    uint32_t total = kmalloc_get_total();
    uint32_t free_p = kmalloc_get_free();
    sstr("[MM] Heap: ");
    sdec_int(total / 1024);
    sstr(" KB total, ");
    sdec_int(free_p);
    sstr(" bytes free\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Round up size to 4-byte alignment, then add header
    uint32_t data_size = (size + 3) & ~3;
    uint32_t needed = HEADER_SIZE + data_size;
    if (needed < MIN_BLOCK) needed = MIN_BLOCK;
    needed = align16(needed);

    // Search for a free block large enough
    struct block_header* best = NULL;
    struct block_header* cur = block_list;
    while (cur) {
        if (cur->free && cur->size >= needed) {
            // First-fit
            best = cur;
            break;
        }
        cur = cur->next;
    }

    // No free block found - allocate a new page(s)
    if (!best) {
        // Allocate enough pages to satisfy request
        uint32_t pages_needed = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint32_t i = 0; i < pages_needed; i++) {
            mm_add_page();
        }
        block_coalesce();

        // Search again
        cur = block_list;
        while (cur) {
            if (cur->free && cur->size >= needed) {
                best = cur;
                break;
            }
            cur = cur->next;
        }

        if (!best) {
            sstr("[MM] kmalloc: out of memory!\n");
            return NULL;
        }
    }

    // Split if block is much larger than needed
    block_split(best, needed);

    // Mark as used
    best->free = 0;

    // Return pointer to data area (after header)
    return (void*)((uint8_t*)best + HEADER_SIZE);
}

void kfree(void* ptr) {
    if (!ptr) return;

    struct block_header* blk = (struct block_header*)((uint8_t*)ptr - HEADER_SIZE);

    if (blk->magic != BLOCK_MAGIC) {
        sstr("[MM] kfree: invalid pointer (bad magic) at 0x");
        shex((uint32_t)ptr);
        sstr("\n");
        return;
    }

    if (blk->free) {
        // Double-free, but recover gracefully
        return;
    }

    blk->free = 1;

    // Coalesce adjacent free blocks
    block_coalesce();
}

uint32_t kmalloc_get_used(void) {
    uint32_t used = 0;
    struct block_header* cur = block_list;
    while (cur) {
        if (!cur->free) used += cur->size;
        cur = cur->next;
    }
    return used;
}

uint32_t kmalloc_get_free(void) {
    uint32_t free = 0;
    struct block_header* cur = block_list;
    while (cur) {
        if (cur->free) free += cur->size;
        cur = cur->next;
    }
    return free;
}

uint32_t kmalloc_get_total(void) {
    uint32_t total = 0;
    struct block_header* cur = block_list;
    while (cur) {
        total += cur->size;
        cur = cur->next;
    }
    return total;
}

void mm_test(void) {
    vga_writestring("\n--- kmalloc Test ---\n");

    uint32_t before_free = kmalloc_get_free();
    vga_writestring("Free before: ");
    vga_write_dec(before_free);
    vga_writestring(" bytes\n");

    void* p1 = kmalloc(16);
    void* p2 = kmalloc(32);
    void* p3 = kmalloc(64);
    void* p4 = kmalloc(128);

    if (p1) {
        strcpy((char*)p1, "kmalloc works!");  // 15 chars + null, fits in 16 bytes
        vga_writestring("  p1(16) = 0x");
        vga_write_hex((uint32_t)p1);
        vga_writestring(" -> \"");
        vga_writestring((char*)p1);
        vga_writestring("\"\n");
    }
    if (p2) {
        int* arr = (int*)p2;
        arr[0] = 42; arr[1] = 99;
        vga_writestring("  p2(32) = 0x");
        vga_write_hex((uint32_t)p2);
        vga_writestring(", arr[0]=");
        vga_write_dec(arr[0]);
        vga_writestring(" arr[1]=");
        vga_write_dec(arr[1]);
        vga_putchar('\n');
    }
    if (p3) {
        vga_writestring("  p3(64) = 0x");
        vga_write_hex((uint32_t)p3);
        vga_putchar('\n');
    }
    if (p4) {
        vga_writestring("  p4(128) = 0x");
        vga_write_hex((uint32_t)p4);
        vga_putchar('\n');
    }

    uint32_t after_alloc = kmalloc_get_used();
    vga_writestring("Used after alloc: ");
    vga_write_dec(after_alloc);
    vga_writestring(" bytes\n");

    // Free p2 and p3
    vga_writestring("Freeing p2...\n");
    kfree(p2);
    vga_writestring("Freeing p3...\n");
    kfree(p3);

    uint32_t after_free = kmalloc_get_free();
    vga_writestring("Free after partial free: ");
    vga_write_dec(after_free);
    vga_writestring(" bytes\n");

    // Allocate again - should reuse freed space
    void* p5 = kmalloc(48);
    if (p5) {
        vga_writestring("  p5(48 realloc) = 0x");
        vga_write_hex((uint32_t)p5);
        vga_putchar('\n');
    }

    vga_writestring("--- kmalloc Test Done ---\n");
}