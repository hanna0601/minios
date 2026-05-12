/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Andrew Peterson, Karen Reid, Alexey Khrabrov, Angela Brown, Kuei Sun
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019, 2021 Karen Reid
 * Copyright (c) 2023, Angela Brown, Kuei Sun
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "malloc369.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"
#include "pagetable.h"

// Counters for various events.
// Your code must increment these when the related events occur.
size_t hit_count = 0;
size_t miss_count = 0;
size_t ref_count = 0;
size_t evict_clean_count = 0;
size_t evict_dirty_count = 0;

// Page table
pgtbl_1_t *page_table[4096];

// Accessor functions for page table entries, to allow replacement
// algorithms to obtain information from a PTE, without depending
// on the internal implementation of the structure.

/* Returns true if the pte is marked valid, otherwise false */
bool is_valid(pt_entry_t *pte)
{
	return pte->valid;
}

/* Returns true if the pte is marked dirty, otherwise false */
bool is_dirty(pt_entry_t *pte)
{
	return pte->dirty;
}

/* Returns true if the pte is marked referenced, otherwise false */
bool get_referenced(pt_entry_t *pte)
{
	return pte->referenced;
}

/* Sets the 'referenced' status of the pte to the given val */
void set_referenced(pt_entry_t *pte, bool val)
{
	pte->referenced = val;
}

pt_entry_t *create_page_entry(void)
{
    // printf("create_page_entry\n");
    pt_entry_t *pte = malloc369(sizeof(pt_entry_t));
    pte->pfn = 0;
    pte->swap_off = INVALID_SWAP;
    pte->valid = true;
    pte->dirty = true;
    pte->referenced = false;
    return pte;
}

void destroy_page_entry(pt_entry_t *pte)
{
    if(pte != NULL) {
        free369(pte);
    }
}

/*
 * Initializes your page table.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is
 * being simulated, so there is just one overall page table.
 *
 * In a real OS, each process would have its own page table, which would
 * need to be allocated and initialized as part of process creation.
 * 
 * The format of the page table, and thus what you need to do to get ready
 * to start translating virtual addresses, is up to you. 
 */
void init_pagetable(void)
{
    // printf("init_pagetable\n");
    for (int i = 0; i < 4096; i++) {
        page_table[i] = NULL;
    }
}

/*
 * Write virtual page represented by pte to swap, if needed, and update 
 * page table entry.
 *
 * Called from allocate_frame() in coremap.c after a victim page frame has
 * been selected. 
 *
 * Counters for evictions should be updated appropriately in this function.
 */
void handle_evict(pt_entry_t * pte)
{
    // printf("handle_evict\n");
	if(pte->dirty) {
        off_t offset = swap_pageout(pte->pfn, pte->swap_off);
        assert(offset != INVALID_SWAP);
        pte->swap_off = offset;
        evict_dirty_count++;
    } else {
        evict_clean_count++;
    }
    pte->valid = false;
    pte->dirty = false;
    pte->referenced = false;
    // printf("handle_evict end\n");
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the page table entry is invalid and not on swap, then this is the first 
 * reference to the page and a (simulated) physical frame should be allocated 
 * and initialized to all zeros (using init_frame from coremap.c).
 * If the page table entry is invalid and on swap, then a (simulated) physical 
 * frame should be allocated and filled by reading the page data from swap.
 *
 * Make sure to update page table entry status information:
 *  - the page table entry should be marked valid
 *  - if the type of access is a write ('S'tore or 'M'odify),
 *    the page table entry should be marked dirty
 *  - a page should be marked dirty on the first reference to the page,
 *    even if the type of access is a read ('L'oad or 'I'nstruction type).
 *  - DO NOT UPDATE the page table entry 'referenced' information. That
 *    should be done by the replacement algorithm functions.
 *
 * When you have a valid page table entry, return the page frame number
 * that holds the requested virtual page.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
int find_frame_number(vaddr_t vaddr, char type)
{
    // printf("find_frame_number\n");
    ref_count++;
	int vpn = (vaddr >> 12) & 0xfffffffff;  // virtual page number 36 bits
    int vpn1 = (vpn >> 24) & 0xfff;  // 12 bits
    int vpn2 = (vpn >> 12) & 0xfff;  // 12 bits
    int vpn3 = vpn & 0xfff;  // 12 bits
    
    if(page_table[vpn1] == NULL) {
        page_table[vpn1] = malloc369(sizeof(pgtbl_1_t));
        for (int i = 0; i < 4096; i++) {
            page_table[vpn1]->pgtbl_2[i] = NULL;
        }
    }
    if(page_table[vpn1]->pgtbl_2[vpn2] == NULL) {
        page_table[vpn1]->pgtbl_2[vpn2] = malloc369(sizeof(pgtbl_2_t));
        for (int i = 0; i < 4096; i++) {
            page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[i] = NULL;
        }
    }
    if(page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[vpn3] == NULL) {
        page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[vpn3] = malloc369(sizeof(pgtbl_3_t));
        page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[vpn3]->pte = NULL;
    }

    pt_entry_t *pte = page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[vpn3]->pte;

    if(pte == NULL) {
        miss_count++;
        pte = create_page_entry();
        int frame = allocate_frame(pte);
        init_frame(frame);
        pte->pfn = frame;
        page_table[vpn1]->pgtbl_2[vpn2]->pgtbl_3[vpn3]->pte = pte;
    } else if(pte->valid) {
        // hit
        hit_count++;
    } else {
        // miss
        miss_count++;
        int frame = allocate_frame(pte);

        int ret = swap_pagein(frame, pte->swap_off);
        assert(ret == 0);

        pte->valid = true;
        pte->pfn = frame;
    } 
    // if the type of access is a write or the first reference to the page
    if(type == 'S' || type == 'M') {
        pte->dirty = true;
    }
    return pte->pfn;
}

void print_pagetable(void)
{
    // printf("|    VPN    |    PFN    |    SWAP    | V | D | R |\n");
    // for (int i = 0; i < 4096; i++) {
    //     for (int j = 0; j < 4096; j++) {
    //         for (int k = 0; k < 4096; k++) {
    //             pt_entry_t *pte = page_table[i]->pgtbl_2[j]->pgtbl_3[k]->pte;
    //             if (pte != NULL) {
    //                 printf("| %5d,%5d,%5d | %10d | %10ld | %d | %d | %d |\n", i, j, k, pte->pfn, pte->swap_off, pte->valid, pte->dirty, pte->referenced);
    //             }
    //         }
    //     }
    // }
}


void free_pagetable(void)
{
    for (int i = 0; i < 4096; i++) {
        if(page_table[i] == NULL) continue;
        for (int j = 0; j < 4096; j++) {
            if(page_table[i]->pgtbl_2[j] == NULL) continue;
            for (int k = 0; k < 4096; k++) {
                if(page_table[i]->pgtbl_2[j]->pgtbl_3[k] == NULL) continue;
                destroy_page_entry(page_table[i]->pgtbl_2[j]->pgtbl_3[k]->pte);
                free369(page_table[i]->pgtbl_2[j]->pgtbl_3[k]);
            }
            free369(page_table[i]->pgtbl_2[j]);
        }
        free369(page_table[i]);
    }
}
