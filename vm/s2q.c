#include "sim.h"
#include "coremap.h"
#include "malloc369.h"

int threshold;
int fifo_q_size;
int lru_q_size;
list_head *fifo_q;
list_head *lru_q;

/* Page to evict is chosen using the simplified 2Q algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int s2q_evict(void)
{
    int ret = -1;
    if (fifo_q_size > threshold) {
        // fifo queue is not empty
        // remove from the head
        struct list_entry * victim = list_first_entry(fifo_q);
        struct frame * frame = container_of(victim, struct frame, framelist_entry);
        assert(frame->in_use);
        ret = frame - coremap;
        list_del(victim);
        fifo_q_size--;
    } else {
        // fifo queue is empty, use lru queue
        // remove from the tail
        assert(lru_q_size > 0);
        struct list_entry * victim = list_last_entry(lru_q);
        struct frame * frame = container_of(victim, struct frame, framelist_entry);
        assert(frame->in_use);
        assert(get_referenced(frame->pte));
        ret = frame - coremap;
        list_del(victim);
        lru_q_size--;
    }
    set_referenced(coremap[ret].pte, false);
    return ret;
}

/* This function is called on each access to a page to update any information
 * needed by the simplified 2Q algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void s2q_ref(int frame, vaddr_t vaddr)
{
	(void)vaddr;

    struct frame * frame_ptr = &coremap[frame];
    struct pt_entry_s * pte = frame_ptr->pte;
    struct list_entry * entry = &frame_ptr->framelist_entry;
    bool in_lru = get_referenced(pte);
    if (in_lru) {
        // in the lru queue
        // move to the head of the lru queue
        list_del(entry);
        list_add_head(lru_q, entry);
    } else if(!in_lru && list_entry_is_linked(entry)){
        // in the fifo queue
        // remove from the fifo queue
        list_del(entry);
        fifo_q_size--;
        // add to the head of lru
        list_add_head(lru_q, entry);
        lru_q_size++;
        set_referenced(pte, true);
    } else {
        // not in any queue
        // move to the tail of the fifo queue
        list_add_tail(fifo_q, entry);
        fifo_q_size++;
    }
}

/* Initialize any data structures needed for this replacement algorithm. */
void s2q_init(void)
{
    fifo_q = (list_head*)malloc369(sizeof(list_head));
    lru_q = (list_head*)malloc369(sizeof(list_head));
    fifo_q_size = 0;
    lru_q_size = 0;
    threshold = memsize / 10;
    list_init(fifo_q);
    list_init(lru_q);
}

/* Cleanup any data structures created in s2q_init(). */
void s2q_cleanup(void)
{
    free369(fifo_q);
    free369(lru_q);
}
