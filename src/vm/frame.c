#include "frame.h"

struct hash upage_frame_map;

void falloc_init () {
    keyed_hash_init (&upage_frame_map);
}

//Every call to allocate_frame should at least specify PAL_USER.
//We could maybe try to fill in the blanks, but it would probably
//be safer to make sure the caller of the function wants a user page.
void* allocate_frame (enum palloc_flags flags) {
    
    ASSERT ((flags & PAL_USER) == 0);
    
    struct frame *new_frame;
    new_frame->kpage = palloc_get_page (flags);
    if (new_frame->kpage == NULL) {
        PANIC ("Ran out of frames!");
    }
    
    /* Note: Afaik we only return this so that the caller can deallocate it. */
    return new_frame->kpage;
}

//pagedir_clear_page (uint32_t *pd, void *upage)

void deallocate_frame () {
    //The palloc-related deallocation is just setting a value in the bitmap
    //right now, I'm not worrying about the bitmap
}

void page_in (void* upage) {
    //Note: pages from files and the swap table (ie. non-frames) can be paged in
    //in fact, if we have a mapping (to a frame), the page should not page fault!!
    
    //We should be able to find the upage in the supplemental page table of the
    //current thread.
}

//PALLOC: FRAMES (Availibility)
//PAGEDIR: MAPPINGS OF FRAMES AND UPAGES
//Note: Not every page is a frame!

