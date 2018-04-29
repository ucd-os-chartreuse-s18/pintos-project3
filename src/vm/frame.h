#ifndef FRAME_ALLOC_H
#define FRAME_ALLOC_H

#include <stdint.h>
#include <keyed_hash.h>
#include <debug.h>
#include "../threads/palloc.h"

/*
The most important operation on the frame table is obtaining an unused frame.
This is easy when the frame is free. When none is free, a frame must be made
free by evicting some page from its frame.

Evicting is where accessed and dirty bits come in handy. When implementing
eviction, aliases might cause trouble. See Page 43 of the pintos manual for
more information on the bits, when they are set, and aliases.

This might end up being very similar to palloc. See `struct pool`, which has
a bitmap representing free pages and a lock. We could maybe also have a base
specified by `uint8_t`, or we might have an array or list of `struct frame`.
We will likely need more data than palloc uses for its base since the struct
we are using is making new data that palloc isn't using. NOTE: This data structure
is globally-defined.
*/

struct frame {
    uint32_t *kpage;
    
    /* From the manual: Each entry in the frame table contains a pointer to the
     * upage, if any, that currently occupy it, and other data of your choice. */
    uint32_t *upage;
};

//From Ivo:
//Access to each frame should be protected by a per-frame lock.
//Access to your frame-search mechanism should be protected by a global lock.

void falloc_init (void);
void* allocate_frame (enum palloc_flags flags);
void deallocate_frame ();
void page_in (void* upage);

#endif
