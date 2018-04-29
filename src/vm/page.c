#include "page.h"

//ZERO, FRAME, FILE, SWAP (have a function for each type?)
struct page* create_spt_entry (void* upage, enum PAGE_STATUS loc, void* aux) {
   
   //The aux parameter will be used since different page table
   //entries will have different arguments. A struct should be
   //made for each argument type.
   
   struct page *p = malloc(sizeof (struct page));
   p->upage = upage;
   p->status = loc;
   
   //A page "in frame" may also have file information, but not on initialization.
   if (loc == IN_FILE) {
     p->file_info = aux;
   }
   
   struct hash *h = &thread_current ()->pages;
   hash_insert (h, &p->elem);
   
   return NULL;
}

bool page_in (void* upage) {
    
    //There might be cases where we don't want to use PAL_ZERO?
    void* kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    
    struct thread *tc = thread_current ();
    struct hash *h = &tc->pages;
    struct hash_elem *e = hash_lookup_key (h, (int) upage);
    struct page *p = hash_entry (e, struct page, elem);
    struct file_info *fi = p->file_info;
    bool success = pagedir_set_page (tc->pagedir, upage, kpage, p->writable);
    
    if (!success) {
        //TODO what other cleanup do we need?
        return false;
    }
    
    int amount_read;
    switch (p->status) {
    case IN_FILE:
      //printf ("d\n");
      /*
      if (file_read (fi->file, kpage, fi->file_bytes) != (int) fi->file_bytes)
      {
        palloc_free_page (kpage);
        return false;
      }
      printf ("kpage is %p, file is %p, bytes is %d\n",
        kpage, fi->file, fi->file_bytes);
      */
      //Sets the remaining bytes in the page to be zero
      //memset (kpage + fi->file_bytes, 0, (PGSIZE - fi->file_bytes));
      
      //p->status = IN_FRAME;
      //p->file_info = fi; ?
      //p->frame = kpage; ?
      break;
    case IN_FRAME:
      PANIC ("Paging-in frame. Impossible State.\n");
    default:
      printf ("Unkown page-in location.\n");
      return false;
    }
    
    //mappings happen irrespective of file loading
    printf ("pagedir: %p\nupage: %p\nkpage: %p\nfile: %p\n\n",
      tc->pagedir, upage, kpage, fi->file);
    
    return true;
}

