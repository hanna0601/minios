#include "sim.h"
#include "coremap.h"

// the position of the current clock hand
static int clock_hand = 0;

/* Page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int clock_evict(void)
{
    int start = clock_hand;
    int victim = -1;

    do {
        struct pt_entry_s *pte = coremap[clock_hand].pte;
        if(get_referenced(pte)) {
            set_referenced(pte, false);
        } else {
            victim = clock_hand;
            clock_hand = (clock_hand + 1) % memsize;

            return victim;
        }

        // move to the next frame
        clock_hand = (clock_hand + 1) % memsize;

    } while(clock_hand != start);

    // clock hand has made a full cycle
    victim = clock_hand;
    clock_hand = (clock_hand + 1) % memsize;

    assert(!get_referenced(coremap[victim].pte));
    return victim;
}

/* This function is called on each access to a page to update any information
 * needed by the CLOCK algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void clock_ref(int frame, vaddr_t vaddr)
{
    (void)vaddr;
    struct pt_entry_s *pte = coremap[frame].pte;
    set_referenced(pte, true);
}

/* Initialize any data structures needed for this replacement algorithm. */
void clock_init(void)
{
    clock_hand = 0;
}

/* Cleanup any data structures created in clock_init(). */
void clock_cleanup(void)
{

}
