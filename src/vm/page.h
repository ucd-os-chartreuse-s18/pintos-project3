#ifndef PAGE_H
#define PAGE_H

#include <stdbool.h>
#include <hash.h>
#include "../threads/thread.h"
#include "../devices/block.h"
#include "../filesys/off_t.h"

/* Frames are global while pages are per-process. */
struct page {
  
  //The upage is a key. Upon faulting, the address will be used to find this page.
  //Since it is a user page, it is not unique globally, so the hash should be per-process.
  //We will need to access a thread's pagedir anyway to create a mapping (when paging in),
  //so we might as well keep things consistent.
  uint32_t *upage;                    /* hash key */
  struct hash_elem elem;              /* corresponds to "pages" in thread.h */
  
  struct frame *frame;                /* castable to kpage? */
}

allocate_page () {
    
}

/* Virtual page. */
#ifdef ASDF_H
struct page {
    
    /* Immutable members. */
    void *addr;                 /* User virtual address. */
    bool read_only;             /* Read-only page? */
    struct thread *thread;      /* Owning thread. */
    
    /* Accessed only in owning process context. */
    struct hash_elem hash_elem; /* struct thread `pages' hash element. */
    
    /* Set only in owning process context with frame->lock held
    * Cleared only with scan_lock and frame->lock held. */
    struct frame *frame;        /* Page frame. */
    
    /* Swap information, protected by frame->lock. */
    block_sector_t sector;      /* Starting sector of swap area, or -1 */
    
    /* Memory-mapped file information, protected by frame->lock. */
    bool private;               /* False to write back to file,
                                  true to write back to swap. */
    struct file *file;          /* File. */
    off_t file_offset;          /* Offset in file. */
    off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE. */
    
};
#endif

#endif
