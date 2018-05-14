#ifndef PAGE_H
#define PAGE_H

#include <stdbool.h>
#include <keyed_hash.h>
#include <stdio.h>
#include <string.h>
#include "../threads/palloc.h"
#include "../threads/vaddr.h"
#include "../threads/thread.h"
#include "../threads/malloc.h"
#include "../devices/block.h"
#include "../filesys/off_t.h"
#include "../filesys/file.h"

/* These should be mutually-exclusive. Don't think powers of two will do much,
 * but it might be interesting/helpful to describe a file that is currently
 * being written to/from a certain location? */
enum PAGE_STATUS {
    IN_FRAME = 1,
    IN_FILE  = 2,
    IN_SWAP  = 4,
    MMAP     = 8
};

struct file_info {
  struct file *file;          /* File. */
  off_t file_offset;          /* Offset in file. */
  off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE. */
  bool private;               /* False to write back to file,
                                 true to write back to swap. */
};

/* This "page" is a supplemental page, or supplemental page entry.
 * 
 * It is Accessed only in owning process context. A regular thread has no need
 * to map upages to kpages since they will only ever use kpages. This means a
 * non-user program (and kpages) should not fault, and we don't have to look
 * them up to load them in. */
struct page {
  
  /* The upage is a key. Upon faulting, the address will be used to find this
   * page in the hash. */
  uint32_t *upage;                    /* hash key */
  struct hash_elem elem;              /* corresponds to "pages" in thread.h */
  
  //Used in creating a mapping (see install_page). Maybe we should also store
  //flags here?
  bool writable;
  
  enum PAGE_STATUS status;
  
  struct file_info *file_info;
  
  /* Set only in owning process context with frame->lock held
  * Cleared only with scan_lock and frame->lock held. */
  //struct frame *frame;
  uint32_t *frame; //kpage
  
  /* Swap information, protected by frame->lock. */
  block_sector_t sector;      /* Starting sector of swap area, or -1 */
  
};

struct page* create_spt_entry (void*, enum PAGE_STATUS, void*);

bool page_in (void* upage);

#endif
