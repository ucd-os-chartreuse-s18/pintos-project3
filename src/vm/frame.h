#ifndef FRAME_H
#define FRAME_H

/*
The most important operation on the frame table is obtaining an unused frame.
This is easy when the frame is free. When none is free, a frame must be made
free by evicting some page from its frame.

Evicting is where accessed and dirty bits come in handy. When implementing
eviction, aliases might cause trouble. See Page 43 of the pintos manual for
more information on the bits, when they are set, and aliases.
*/

struct frame {

};

#endif

/*
Frame Table: (crtl + f to try to find more info)

Each entry in the frame table contains a pointer to the upage, if any,
that currently occupy it, and other data of your choice.
*/