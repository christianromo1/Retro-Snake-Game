#ifndef PAGE_H
#define PAGE_H

// ============ PAGE FRAME ALLOCATOR ============
// Each ppage struct tracks one 2MB physical page of memory.
// Free pages are kept in a doubly-linked list: free_physical_pages.

struct ppage {
    struct ppage *next;
    struct ppage *prev;
    void *physical_addr;    // Physical address of the 2MB page this struct tracks
};

// Initialize the free page list — call this once from main()
void init_pfa_list(void);

// Allocate npages physical pages; returns pointer to a new linked list of them.
// Returns NULL if not enough free pages are available.
struct ppage *allocate_physical_pages(unsigned int npages);

// Return a list of pages back to the free list (opposite of allocate).
void free_physical_pages(struct ppage *ppage_list);

#endif
