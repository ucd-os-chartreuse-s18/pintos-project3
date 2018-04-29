#include "frame.h"

void falloc_init () {
    //TODO look at and copy palloc style as closely as possbile
}

//Every call to allocate_frame should at least specify PAL_USER.
//We could maybe try to fill in the blanks, but it would probably
//be safer to make sure the caller of the function wants a user page.
void* allocate_frame (UNUSED enum palloc_flags flags) {
    
    ASSERT ((flags & PAL_USER) == 0);
    
    /*
    struct frame *new_frame = malloc(sizeof (struct frame));
    void* kpage = palloc_get_page (flags);
    new_frame->kpage = palloc_get_page (flags);
    if (new_frame->kpage == NULL) {
        free (new_frame->kpage);
        PANIC ("Ran out of frames!");
    } */
    
    /* The reason why the caller doesn't really need the frame is because the
     * frame is mostly needed to map things, which we will do in page_in () */
    //return new_frame->kpage;
    return NULL;
}

//For when we evict:
//pagedir_clear_page (uint32_t *pd, void *upage)

void deallocate_frame (UNUSED void* ptr) {
    //The palloc-related deallocation is just setting a value in the bitmap
    //right now, I'm not worrying about the bitmap
    
    //base this off of palloc?
}

//PALLOC: FRAMES (Availibility)
//PAGEDIR: MAPPINGS OF FRAMES AND UPAGES
//Note: Not every page is a frame!

