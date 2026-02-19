#include "page.h"

// ============ PAGE FRAME ALLOCATOR ============

#define NUM_PAGES  128
#define PAGE_SIZE  (2 * 1024 * 1024)   // 2MB per page -> 128 pages = 256MB total

// Statically allocate an array of ppage structs, one per physical page.
// These structs live in the kernel's BSS/data segment; they are NOT the
// pages themselves — they just track them.
struct ppage physical_page_array[NUM_PAGES];

// Head of the free list — pages not yet assigned to any process.
// Renamed to pfa_free_list to avoid conflict with the function free_physical_pages().
struct ppage *pfa_free_list;


// ---- Doubly-linked list helpers (used internally) ----

// Push a single ppage onto the FRONT of a list.
// list_head is a pointer-to-pointer so we can update the caller's head.
static void list_push_front(struct ppage **list_head, struct ppage *p) {
    p->next = *list_head;
    p->prev = (void *)0;            // NULL — front of list has no predecessor

    if (*list_head != (void *)0) {
        (*list_head)->prev = p;     // old head's prev now points back to p
    }

    *list_head = p;                 // p is the new head
}

// Pop (remove and return) the first element from a list.
// Returns NULL if the list is empty.
static struct ppage *list_pop_front(struct ppage **list_head) {
    if (*list_head == (void *)0) {
        return (void *)0;           // empty list
    }

    struct ppage *p = *list_head;
    *list_head = p->next;

    if (*list_head != (void *)0) {
        (*list_head)->prev = (void *)0; // new head has no predecessor
    }

    // Detach the popped node
    p->next = (void *)0;
    p->prev = (void *)0;
    return p;
}

// Find the tail of a list (last node).
static struct ppage *list_tail(struct ppage *head) {
    if (head == (void *)0) return (void *)0;
    while (head->next != (void *)0) {
        head = head->next;
    }
    return head;
}


// ---- Public API ----

// init_pfa_list: loop through every element of physical_page_array,
// set its physical address, and link it into pfa_free_list.
void init_pfa_list(void) {
    pfa_free_list = (void *)0;

    for (int i = 0; i < NUM_PAGES; i++) {
        // Page i starts at physical address i * 2MB
        // Cast through unsigned long to avoid pointer-from-int warning on 64-bit hosts
        physical_page_array[i].physical_addr = (void *)((unsigned long)i * PAGE_SIZE);
        physical_page_array[i].next = (void *)0;
        physical_page_array[i].prev = (void *)0;

        list_push_front(&pfa_free_list, &physical_page_array[i]);
    }
}

// allocate_physical_pages: remove npages from the free list and return
// them as their own linked list (allocd_list). Returns NULL on failure.
struct ppage *allocate_physical_pages(unsigned int npages) {
    struct ppage *allocd_list = (void *)0;
    struct ppage *allocd_tail = (void *)0;

    for (unsigned int i = 0; i < npages; i++) {
        // Can't fulfill request — not enough free pages
        if (pfa_free_list == (void *)0) {
            return (void *)0;
        }

        // Take one page off the front of the free list
        struct ppage *p = list_pop_front(&pfa_free_list);

        // Append it to the allocated list we're building
        p->prev = allocd_tail;
        p->next = (void *)0;

        if (allocd_tail != (void *)0) {
            allocd_tail->next = p;  // link previous tail forward to p
        } else {
            allocd_list = p;        // first page — it's also the head
        }
        allocd_tail = p;
    }

    return allocd_list;             // caller owns this list now
}

// free_physical_pages: prepend an entire ppage list back onto pfa_free_list.
// Basically the reverse of allocate_physical_pages.
void free_physical_pages(struct ppage *ppage_list) {
    if (ppage_list == (void *)0) return;

    // Walk to the tail of the list being returned
    struct ppage *tail = list_tail(ppage_list);

    // Splice: tail of returned list -> current head of free list
    tail->next = pfa_free_list;
    if (pfa_free_list != (void *)0) {
        pfa_free_list->prev = tail;
    }

    // The returned list's head becomes the new free list head
    ppage_list->prev = (void *)0;
    pfa_free_list = ppage_list;
}
