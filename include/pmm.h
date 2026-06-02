#ifndef PMM_H
#define PMM_H

#include "types.h"

#define PAGE_SIZE 4096

// Initialize physical memory manager from Multiboot info
void pmm_init(uint32_t multiboot_info_addr);

// Allocate a single 4KB page, returns physical address (0 on failure)
void* pmm_alloc_page(void);

// Free a previously allocated page
void pmm_free_page(void* addr);

// Get memory statistics
uint32_t pmm_get_total_pages(void);
uint32_t pmm_get_free_pages(void);
uint32_t pmm_get_used_pages(void);
uint32_t pmm_get_total_memory_kb(void);

#endif // PMM_H