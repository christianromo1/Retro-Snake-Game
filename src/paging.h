#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "page.h"   // for struct ppage

// ============ PAGE TABLE STRUCTURES (i386) ============

struct page_directory_entry {
    uint32_t present       : 1;   // Page present in memory
    uint32_t rw            : 1;   // Read-only if clear, R/W if set
    uint32_t user          : 1;   // Supervisor only if clear
    uint32_t writethru     : 1;   // Cache this directory as write-thru only
    uint32_t cachedisabled : 1;   // Disable cache on this page table?
    uint32_t accessed      : 1;   // Supervisor level only if clear
    uint32_t pagesize      : 1;   // Page size (0 = 4KB)
    uint32_t ignored       : 2;   // Ignored bits
    uint32_t os_specific   : 3;   // Available for OS use
    uint32_t frame         : 20;  // Frame address (physical addr >> 12)
};

struct page {
    uint32_t present    : 1;   // Page present in memory
    uint32_t rw         : 1;   // Read-only if clear, readwrite if set
    uint32_t user       : 1;   // Supervisor level only if clear
    uint32_t accessed   : 1;   // Has the page been accessed since last refresh?
    uint32_t dirty      : 1;   // Has the page been written to since last refresh?
    uint32_t unused     : 7;   // Amalgamation of unused and reserved bits
    uint32_t frame      : 20;  // Frame address (physical addr >> 12)
};

// ============ GLOBALS ============

// The kernel's top-level page directory (1024 entries, 4096-byte aligned)
extern struct page_directory_entry kernel_pd[1024];

// ============ FUNCTION PROTOTYPES ============

// Map a linked list of physical pages starting at virtual address vaddr.
// Returns the starting virtual address that was mapped.
void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd);

// Load pd into CR3 (sets the page directory base register)
void loadPageDirectory(struct page_directory_entry *pd);

// Enable paging by setting bits 0 and 31 of CR0
void enablePaging(void);

#endif // PAGING_H
