#ifndef FRAME_ALLOC_H
#define FRAME_ALLOC_H

#include <stdint.h>
#include <keyed_hash.h>
#include "../threads/palloc.h"

/*
The most important operation on the frame table is obtaining an unused frame.
This is easy when the frame is free. When none is free, a frame must be made
free by evicting some page from its frame.

Evicting is where accessed and dirty bits come in handy. When implementing
eviction, aliases might cause trouble. See Page 43 of the pintos manual for
more information on the bits, when they are set, and aliases.
*/

//Properties of "frame":
//1) We can look up a frame using a upage
//2) We can iterate all frames to search for a frame that isn't being used. This
//   is where palloc has many similarities. FOR NOW: We don't have to worry about
//   this. It will come up later when palloc_get_page returns NULL.
struct frame {
    uint32_t *upage;
    struct hash_elem elem;
    uint32_t *kpage;
};

/*
This is a pool for palloc. If we didn't use palloc, we would need a lock. I suppose
we could have a linked list of frames and check to see if upage was NULL, but a
used_map might be faster. If we could augment palloc for frame eviction, this might
be a smart move that saves us time.
struct pool
{
    struct lock lock;                   // Mutual exclusion.
    struct bitmap *used_map;            // Bitmap of free pages.
    uint8_t *base;                      // Base of pool.
}; */
//Access to each frame should be protected by a per-frame lock.
//Access to your frame-search mechanism should be protected by a global lock.

void falloc_init (void);

void* allocate_frame (enum palloc_flags flags);
void deallocate_frame ();

//creates a mapping between
void page_in (void* upage);

/*
Frame Table: (crtl + f to try to find more info)

Each entry in the frame table contains a pointer to the upage, if any,
that currently occupy it, and other data of your choice.
*/

#endif
