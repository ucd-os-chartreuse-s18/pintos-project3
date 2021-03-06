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
   struct hash_elem *e = hash_insert (h, &p->elem);
   if (e != NULL) {
     printf ("Duplicate entry into hash table.\n");
   }
   
   return NULL;
}

bool page_in (void* upage) {
    
    //printf ("page in %p\n", upage);
    void* kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    
    struct thread *tc = thread_current ();
    struct hash *h = &tc->pages;
    struct hash_elem *e = hash_lookup_key (h, (int) upage);
    
    if (e == NULL) {
      //printf ("hash not found in %s\n", tc->name);
      return false;
    }
    
    struct page *p = hash_entry (e, struct page, elem);
    struct file_info *fi = p->file_info;
    //this "true" will need to be related to some variable
    bool success = pagedir_set_page (tc->pagedir, upage, kpage, true);
    
    if (!success) {
        //TODO what other cleanup do we need?
        printf ("pagedir set page failed\n");
        return false;
    }
    
    switch (p->status) {
    case IN_FILE:
      
      file_seek (fi->file, fi->file_offset);
      if (file_read (fi->file, kpage, fi->file_bytes) != (int) fi->file_bytes) {
        palloc_free_page (kpage);
        printf ("could not read\n");
        return false;
      }
      
      file_seek (fi->file, fi->file_offset);
      
      //None of this stuff should matter in the intial mapping
      //We access the frame on swap TODO update
      p->status = IN_FRAME;
      p->file_info = fi; //?
      p->frame = kpage;  //?
      break;
    case IN_FRAME:
      PANIC ("Paging-in frame. Impossible State.\n");
    default:
      printf ("Unkown page-in location.\n");
      return false;
    }
    
    return true;
}

bool grow_stack (void *faultaddr, void* stk_ptr) {
  
  uint8_t *kpage;
  uint8_t *upage = pg_round_down (faultaddr);
  int page_distance = ROUND_UP (faultaddr - stk_ptr, PGSIZE);
  int new_page_cnt = page_distance / PGSIZE;
    
  for (int i = 1; i <= new_page_cnt; i++) {
    kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    install_page (upage, kpage, true);
    upage += PGSIZE;
  }

  return true;
}

