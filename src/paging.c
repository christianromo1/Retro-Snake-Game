#include <stdint.h>
#include "paging.h"
#include "page.h"

// ============ STATIC PAGE DIRECTORY AND PAGE TABLE POOL ============
//
// Both must be 4096-byte aligned and declared globally (not on the stack).
// We use a pool of page tables so map_pages can handle multiple page
// directory entries without needing dynamic allocation.

struct page_directory_entry kernel_pd[1024] __attribute__((aligned(4096)));

#define PT_POOL_SIZE 16   // supports up to 16 distinct 4MB regions
static struct page pt_pool[PT_POOL_SIZE][1024] __attribute__((aligned(4096)));
static int pt_pool_next = 0;   // index of next available page table in the pool

// ============ INTERNAL HELPERS ============

// Zero-initialise and return the next free page table from the pool.
// Returns NULL if the pool is exhausted.
static struct page *alloc_page_table(void) {
    if (pt_pool_next >= PT_POOL_SIZE) {
        return (struct page *)0;   // pool exhausted
    }
    struct page *pt = pt_pool[pt_pool_next++];

    // Zero every entry so all pages start as not-present
    for (int i = 0; i < 1024; i++) {
        *((uint32_t *)&pt[i]) = 0;
    }
    return pt;
}

// ============ MAP_PAGES ============

// Map a linked list of physical pages (pglist) to consecutive virtual
// addresses starting at vaddr, inside page directory pd.
//
// Each ppage in the list maps to one 4 KB page frame:
//   virtual  address: vaddr + (i * 4096)
//   physical address: pglist[i].physical_addr
//
// Returns the starting virtual address on success.
void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd) {
    void *start_vaddr = vaddr;

    for (struct ppage *cur = pglist; cur != (struct ppage *)0; cur = cur->next) {
        uint32_t va    = (uint32_t)vaddr;
        uint32_t pd_idx = va >> 22;                // top 10 bits  → PDE index
        uint32_t pt_idx = (va >> 12) & 0x3FF;     // middle 10 bits → PTE index

        // ---- Locate (or create) the page table for this PDE ----
        struct page *pt;
        if (!pd[pd_idx].present) {
            // No page table yet for this directory entry; allocate one.
            pt = alloc_page_table();
            if (pt == (struct page *)0) {
                // Pool exhausted — cannot map this page.
                // In a real kernel you would return an error code here.
                return (void *)0;
            }

            // Wire the PDE to the new page table.
            pd[pd_idx].frame         = (uint32_t)pt >> 12;
            pd[pd_idx].present       = 1;
            pd[pd_idx].rw            = 1;   // read/write
            pd[pd_idx].user          = 0;   // supervisor only
            pd[pd_idx].writethru     = 0;
            pd[pd_idx].cachedisabled = 0;
            pd[pd_idx].accessed      = 0;
            pd[pd_idx].pagesize      = 0;   // 4 KB pages
        } else {
            // Page table already exists for this PDE; recover its address.
            pt = (struct page *)((uint32_t)(pd[pd_idx].frame) << 12);
        }

        // ---- Set the page table entry ----
        // physical_addr is void* in struct ppage — must cast to uint32_t
        // before doing bit-shift arithmetic.
        uint32_t phys = (uint32_t)cur->physical_addr;
        pt[pt_idx].frame    = phys >> 12;
        pt[pt_idx].present  = 1;
        pt[pt_idx].rw       = 1;   // read/write
        pt[pt_idx].user     = 0;   // supervisor only
        pt[pt_idx].accessed = 0;
        pt[pt_idx].dirty    = 0;
        pt[pt_idx].unused   = 0;

        // Advance to the next 4 KB virtual page
        vaddr = (void *)(va + 0x1000);
    }

    return start_vaddr;
}

// ============ CR3 / CR0 HELPERS ============

// Load pd into CR3 (the Page Directory Base Register).
void loadPageDirectory(struct page_directory_entry *pd) {
    asm("mov %0, %%cr3"
        :
        : "r"(pd)
        :);
}

// Enable paging: set PE (bit 0) and PG (bit 31) in CR0.
void enablePaging(void) {
    asm("mov %%cr0, %%eax\n"
        "or $0x80000001, %%eax\n"
        "mov %%eax, %%cr0"
        :
        :
        : "eax");
}
