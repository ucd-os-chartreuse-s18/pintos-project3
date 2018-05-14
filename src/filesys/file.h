#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include <stdbool.h>

struct inode;

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
bool file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

/* MMAP FUNCTIONS */

/* The function `file_map` sets the base upage for the file. The base
 * upage and all calculated pages (from `file_pagev`) should all be
 * accessible through the supplemental page table otherwise we have an
 * unconsistent mapping.
 * 
 * All functions except `file_mapped` assert a kind of mapping status.
 * The function `file_pagev` returns an array of all the page-aligned
 * boundaries from base to base + file_length. `file_pagec` is just the
 * number of pages. */
bool file_mapped (struct file*);
void file_map    (struct file*, void*);
void file_unmap  (struct file*);
int file_pagec   (struct file*);
void* file_pagev (struct file*);
bool mmap_close  (struct file*);

#endif /* filesys/file.h */
