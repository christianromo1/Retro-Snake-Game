#include <stdio.h>
#include "page.h"

int main(void) {
    // Initialize the free list
    init_pfa_list();
    printf("init_pfa_list() complete\n");

    // Allocate 2 pages and check the result
    struct ppage *list = allocate_physical_pages(2);
    if (list == (void *)0) {
        printf("FAIL: allocate returned NULL\n");
        return 1;
    }
    printf("Allocated page 1 physical addr: %p\n", list->physical_addr);
    printf("Allocated page 2 physical addr: %p\n", list->next->physical_addr);
    printf("Page 2 prev points back to page 1: %s\n",
           list->next->prev == list ? "PASS" : "FAIL");

    // Free them back and re-allocate to confirm they return
    free_physical_pages(list);
    printf("free_physical_pages() complete\n");

    struct ppage *list2 = allocate_physical_pages(2);
    printf("Re-allocated after free: %s\n",
           list2 != (void *)0 ? "PASS" : "FAIL");

    // Try to allocate more than available (should return NULL)
    struct ppage *too_many = allocate_physical_pages(200);
    printf("Allocating 200 pages (only 128 exist) returns NULL: %s\n",
           too_many == (void *)0 ? "PASS" : "FAIL");

    return 0;
}
