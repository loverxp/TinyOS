// paging.c - x86 Paging (Page Directory / Page Table) implementation
// Identity maps the first 8MB of physical memory for kernel + user area.

#include "../include/paging.h"
#include "../include/pmm.h"
#include "../include/vga.h"
#include "../include/stdio.h"

// The page directory (must be 4KB aligned)
static page_dir_entry_t* page_dir = NULL;
static page_table_entry_t* page_tables[4] = {NULL, NULL, NULL, NULL};

// Last page fault info (for debugging)
static uint32_t last_fault_addr = 0;
static uint32_t last_fault_err  = 0;

// Track whether paging is enabled
static uint8_t paging_active = 0;

// CR0 register flags
#define CR0_PG  (1 << 31)  // Paging Enable
#define CR0_WP  (1 << 16)  // Write Protect

void paging_init(void) {
    printf("  Setting up page tables...\n");

    // Allocate page directory (1 page = 4KB = 1024 entries)
    page_dir = (page_dir_entry_t*)pmm_alloc_page();
    if (!page_dir) {
        printf("  ERROR: Failed to allocate page directory!\n");
        return;
    }

    // Clear page directory
    for (int i = 0; i < PD_ENTRIES; i++) {
        uint32_t* pde = (uint32_t*)page_dir;
        pde[i] = 0;
    }

    // We need page tables for the first 8MB (2 page tables)
    // PT0: 0x00000000 - 0x003FFFFF (kernel, VGA, BIOS)
    // PT1: 0x00400000 - 0x007FFFFF (user program area)

    int num_tables = 2;

    for (int t = 0; t < num_tables; t++) {
        page_tables[t] = (page_table_entry_t*)pmm_alloc_page();
        if (!page_tables[t]) {
            printf("  ERROR: Failed to allocate page table %d!\n", t);
            return;
        }

        // Clear page table
        for (int i = 0; i < PT_ENTRIES; i++) {
            uint32_t* pte = (uint32_t*)page_tables[t];
            pte[i] = 0;
        }

        // Identity map this 4MB region
        uint32_t base = t * 0x400000;  // 0 or 4MB
        for (int i = 0; i < PT_ENTRIES; i++) {
            uint32_t phys_addr = base + i * PAGE_SIZE;
            page_table_entry_t entry;
            entry.present     = 1;
            entry.rw          = 1;    // read-write
            entry.user        = 1;    // supervisor + user accessible
            entry.writethrough= 0;
            entry.cache       = 0;
            entry.accessed    = 0;
            entry.dirty       = 0;
            entry.pat         = 0;
            entry.global      = 0;
            entry._available  = 0;
            entry.page_addr   = phys_addr >> 12;

            uint32_t* pte = (uint32_t*)page_tables[t];
            pte[i] = *(uint32_t*)&entry;
        }

        // Set page directory entry for this 4MB region
        page_dir_entry_t pde;
        pde.present     = 1;
        pde.rw          = 1;
        pde.user        = 1;
        pde.writethrough= 0;
        pde.cache       = 0;
        pde.accessed    = 0;
        pde._reserved0  = 0;
        pde.ps          = 0;    // 4KB pages (points to page table)
        pde.global      = 0;
        pde._available  = 0;
        pde.table_addr  = (uint32_t)page_tables[t] >> 12;

        uint32_t* pde_arr = (uint32_t*)page_dir;
        pde_arr[t] = *(uint32_t*)&pde;

        printf("    PT%d: 0x%08x -> 0x%08x\n", t, base, base + 0x3FFFFF);
    }

    // Set CR3 to point to page directory
    uint32_t cr3 = (uint32_t)page_dir;
    asm volatile("mov %0, %%cr3" : : "r"(cr3));

    // Enable paging by setting PG bit in CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PG | CR0_WP;   // Enable paging and write protection
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    paging_active = 1;
    printf("  Paging enabled (identity mapped: 0x00000000 - 0x007FFFFF)\n");
}

// Map a single 4KB page: virt_addr -> phys_addr with given flags
// Returns 0 on success, -1 on failure
int paging_map_page(uint32_t virt_addr, uint32_t phys_addr, int user, int rw) {
    if (!paging_active) return -1;

    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    // Read current PDE
    uint32_t* pde_arr = (uint32_t*)page_dir;
    page_dir_entry_t* pde = (page_dir_entry_t*)&pde_arr[pd_idx];

    page_table_entry_t* page_table;

    if (!pde->present) {
        // Allocate a new page table
        page_table = (page_table_entry_t*)pmm_alloc_page();
        if (!page_table) return -1;

        // Clear it
        for (int i = 0; i < PT_ENTRIES; i++) {
            uint32_t* pte = (uint32_t*)page_table;
            pte[i] = 0;
        }

        // Set PDE with flags: present, rw, user, accessed
        page_dir_entry_t new_pde;
        new_pde.present     = 1;
        new_pde.rw          = 1;            // supervisor always RW for page table access
        new_pde.user        = 1;            // allow user access to page table
        new_pde.writethrough= 0;
        new_pde.cache       = 0;
        new_pde.accessed    = 1;
        new_pde._reserved0  = 0;
        new_pde.ps          = 0;            // 4KB pages
        new_pde.global      = 0;
        new_pde._available  = 0;
        new_pde.table_addr  = (uint32_t)page_table >> 12;

        pde_arr[pd_idx] = *(uint32_t*)&new_pde;

        // Flush TLB for this PDE
        asm volatile("invlpg (%0)" : : "r"(virt_addr & ~0x3FFFFF));
    } else {
        // Existing page table
        page_table = (page_table_entry_t*)(pde->table_addr << 12);
    }

    // Set PTE
    page_table_entry_t entry;
    entry.present     = 1;
    entry.rw          = rw ? 1 : 0;
    entry.user        = user ? 1 : 0;
    entry.writethrough= 0;
    entry.cache       = 0;
    entry.accessed    = 0;
    entry.dirty       = 0;
    entry.pat         = 0;
    entry.global      = 0;
    entry._available  = 0;
    entry.page_addr   = phys_addr >> 12;

    uint32_t* pte_arr = (uint32_t*)page_table;
    pte_arr[pt_idx] = *(uint32_t*)&entry;

    // Flush TLB for this specific page
    asm volatile("invlpg (%0)" : : "r"(virt_addr));

    return 0;
}

// Read CR2 to get the faulting address (called from page fault handler)
uint32_t paging_get_fault_addr(void) {
    uint32_t addr;
    asm volatile("mov %%cr2, %0" : "=r"(addr));
    return addr;
}

uint32_t paging_get_fault_err(void) {
    return last_fault_err;
}

void paging_set_fault(uint32_t addr, uint32_t err) {
    last_fault_addr = addr;
    last_fault_err  = err;
}

// Dump page table info
void paging_dump_info(void) {
    if (!paging_active) {
        printf("Paging is DISABLED\n");
        return;
    }

    uint32_t cr0_val, cr2_val, cr3_val;
    asm volatile("mov %%cr0, %0" : "=r"(cr0_val));
    asm volatile("mov %%cr2, %0" : "=r"(cr2_val));
    asm volatile("mov %%cr3, %0" : "=r"(cr3_val));

    printf("Paging: ENABLED\n");
    printf("  CR0=0x%x  CR2=0x%x  CR3=0x%x\n", cr0_val, cr2_val, cr3_val);

    if (page_dir) {
        for (int i = 0; i < 2; i++) {
            uint32_t* pde_arr = (uint32_t*)page_dir;
            page_dir_entry_t* pde = (page_dir_entry_t*)&pde_arr[i];
            if (pde->present) {
                printf("  PDE[%d]: table=0x%x (present, %s, %s)\n",
                    i, pde->table_addr << 12,
                    pde->rw ? "RW" : "RO",
                    pde->user ? "User" : "Supervisor");
            }
        }
    }
}