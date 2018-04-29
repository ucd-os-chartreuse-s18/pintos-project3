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
    
    struct frame new_frame;
    new_frame->kpage = palloc_get_page (flags);
    if (new_frame->kpage == NULL) {
        PANIC ("Ran out of frames!");
    }
    return new_frame->kpage;
}

//We might need to store a pagedir address, an address to the thread, or
//otherwise be able to access the original thread's pagedir when we unmap it.
//thread->pagedir, upage
//pagedir_clear_page (uint32_t *pd, void *upage)

void deallocate_frame () {
    
    //pagedir_clear_page ();
    //Note: this is NOT related to palloc
    //The palloc-related deallocation is just setting a value in the bitmap
    //right now, I'm not worrying about the bitmap
    
}

void page_in (void* upage) {
    
}

