#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


// User-level virtual addresses on a 64-bit Linux system are 48 bits in our
// traces, and the page size is 4096 (12 bits). The remaining 36 bits are
// the virtual page number, which is used as the lookup key (or index) into
// your page table. 


// Page table entry 
// This structure will need to record the physical page frame number
// for a virtual page, as well as the swap offset if it is evicted. 
// You will also need to keep track of the Valid, Dirty and Referenced
// status bits (or flags). 
// You do not need to keep track of Read/Write/Execute permissions.
typedef struct pt_entry_s {
    // physical page frame number
    uint32_t pfn;
    // swap offset
    off_t swap_off;
    // Valid status bit
    bool valid;
    // Dirty status bit
    bool dirty;
    // Referenced status bit
    bool referenced;
} pt_entry_t;

typedef struct third_level_page_table {
    pt_entry_t *pte;
} pgtbl_3_t;

typedef struct second_level_page_table {
    pgtbl_3_t *pgtbl_3[4096];
} pgtbl_2_t;

typedef struct top_level_page_table {
    pgtbl_2_t *pgtbl_2[4096];
} pgtbl_1_t;


#endif /* __PAGETABLE_H__ */
