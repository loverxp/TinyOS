#ifndef PAGING_H
#define PAGING_H

#include "types.h"

// Page directory entry (PDE) - 4KB page table
typedef struct {
    uint32_t present    : 1;
    uint32_t rw         : 1;   // 0=read-only, 1=read-write
    uint32_t user       : 1;   // 0=supervisor, 1=user
    uint32_t writethrough:1;
    uint32_t cache      : 1;
    uint32_t accessed   : 1;
    uint32_t _reserved0 : 1;
    uint32_t ps         : 1;   // 0=4KB page table, 1=4MB page
    uint32_t global     : 1;
    uint32_t _available : 3;
    uint32_t table_addr : 20;  // physical address of page table >> 12
} __attribute__((packed)) page_dir_entry_t;

// Page table entry (PTE) - 4KB page
typedef struct {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t writethrough:1;
    uint32_t cache      : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t _available : 3;
    uint32_t page_addr  : 20;  // physical address of 4KB page >> 12
} __attribute__((packed)) page_table_entry_t;

// Number of entries per page table / directory
#define PT_ENTRIES  1024
#define PD_ENTRIES  1024

// Page size
#define PAGE_SIZE   4096

// Initialize paging (identity map first 8MB)
void paging_init(void);

// Get page fault info
uint32_t paging_get_fault_addr(void);
uint32_t paging_get_fault_err(void);

// Debug: print page table info
void paging_dump_info(void);

#endif // PAGING_H